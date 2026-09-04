/* CFUDISK.C - read a USB mass-storage device on the RATOC REX-CFU1.
 *
 * Entirely ring-3: drives the resident CFU1.VXD through its existing
 * CFU_CTRL (control transfers) and CFU_XACT (raw bulk transactions)
 * ioctls.  Implements USB enumeration, a USB hub layer (so the device may
 * be plugged in directly or sit behind a hub), Bulk-Only Transport, and
 * enough SCSI (INQUIRY, READ CAPACITY, READ(10)) to identify the device,
 * report its size, and read sectors.  Parses the MBR partition table and
 * a FAT12/16 boot sector, and lists the root directory.
 *
 * Usage:  CFUDISK              identify + capacity + partition/FAT summary
 *         CFUDISK /SEC n       hex-dump logical sector n (of the volume)
 *         CFUDISK /DIR         list the root directory (FAT12/16)
 *
 * Build:  wcc386 -bt=nt -zq -ox CFUDISK.C
 *         wlink system nt op q name CFUDISK.EXE file CFUDISK.obj lib user32
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cfu1.h"

static HANDLE hVxd = INVALID_HANDLE_VALUE;
static int g_ring0 = 0;                 /* /R0: run BOT through the VxD */

/* device/endpoint state discovered by enumeration */
static unsigned char g_addr = 1;        /* address of the storage device   */
static unsigned char g_mps0 = 8;        /* ep0 max packet                  */
static unsigned char g_ep_in = 0x81;    /* bulk IN endpoint address        */
static unsigned char g_ep_out = 0x02;   /* bulk OUT endpoint address       */
static int g_tog_in = 0, g_tog_out = 0; /* data toggles                    */
static unsigned long g_blocks = 0, g_bsize = 512;
static unsigned long g_part_lba = 0;    /* start LBA of the FAT volume     */

static int vxd_ioctl(DWORD code, void *in, DWORD cbin, void *out, DWORD cbout)
{
    DWORD ret = 0;
    if (!DeviceIoControl(hVxd, code, in, cbin, out, cbout, &ret, NULL))
        return -1;
    return 0;
}

static HANDLE load_vxd(void)
{
    char exe[MAX_PATH], path[MAX_PATH + 8];
    char *p;
    HANDLE h;
    GetModuleFileName(NULL, exe, sizeof exe);
    p = strrchr(exe, '\\');
    if (p) *(p + 1) = 0;
    sprintf(path, "\\\\.\\%sCFU1.VXD", exe);
    h = CreateFile(path, 0, 0, NULL, 0, FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (h == INVALID_HANDLE_VALUE)
        h = CreateFile("\\\\.\\CFU1.VXD", 0, 0, NULL, 0,
                       FILE_FLAG_DELETE_ON_CLOSE, NULL);
    return h;
}

/*--------------------------------------------------------- control -------*/
static int control(unsigned char addr, unsigned char rt, unsigned char req,
                   unsigned short val, unsigned short idx,
                   void *data, unsigned len)
{
    CFU_CREQ q;
    static CFU_CRSP r;
    memset(&q, 0, sizeof q);
    q.devaddr = addr;
    q.mps = g_mps0;
    q.wlen = (unsigned short)len;
    q.setup[0] = rt; q.setup[1] = req;
    q.setup[2] = (unsigned char)val; q.setup[3] = (unsigned char)(val >> 8);
    q.setup[4] = (unsigned char)idx; q.setup[5] = (unsigned char)(idx >> 8);
    q.setup[6] = (unsigned char)len; q.setup[7] = (unsigned char)(len >> 8);
    memset(&r, 0, sizeof r);
    if (vxd_ioctl(CFU_CTRL, &q, sizeof q, &r, sizeof r) != 0) return -1;
    if (r.err) return -(int)(r.err & 0xFF) - 1;
    if (data && r.got) memcpy(data, r.data, r.got > len ? len : r.got);
    return (int)r.got;
}

/*----------------------------------------------------------- bulk --------*/
/* one bulk packet with NAK retry; returns SL811 status (bit0=ACK), or -1 */
static int bulk_pkt(unsigned char pidep, unsigned char ctl,
                    void *data, int len, int *got)
{
    CFU_XREQ q;
    static CFU_XRSP r;
    int tries = 2000;
    memset(&q, 0, sizeof q);
    q.pidep = pidep;
    q.devaddr = g_addr;
    q.ctl = ctl;
    q.len = (unsigned char)len;
    if ((ctl & 0x04) && len) memcpy(q.data, data, len);   /* OUT payload */
    while (tries--) {
        memset(&r, 0, sizeof r);
        if (vxd_ioctl(CFU_XACT, &q, sizeof q, &r, sizeof r) != 0) return -1;
        if (r.status == 0xFFFFFFFFUL) return -1;           /* timeout */
        if (r.status & 0x40) { Sleep(1); continue; }        /* NAK: retry */
        if (r.status & 0x01) {                              /* ACK */
            if (got) *got = (int)r.got;
            if (!(ctl & 0x04) && data && r.got)
                memcpy(data, r.data, r.got > (DWORD)len ? len : r.got);
            return (int)r.status;
        }
        return (int)r.status;                               /* STALL/error */
    }
    return -1;                                               /* NAK forever */
}

static int bulk_out(const void *data, int len)
{
    const unsigned char *p = (const unsigned char *)data;
    int done = 0, chunk, st;
    while (done < len) {
        chunk = len - done; if (chunk > 64) chunk = 64;
        st = bulk_pkt((unsigned char)(0x10 | (g_ep_out & 0x0F)),
                      (unsigned char)(0x07 | (g_tog_out ? 0x40 : 0)),
                      (void *)(p + done), chunk, NULL);
        if (st < 0 || !(st & 0x01)) return -1;
        g_tog_out ^= 1;
        done += chunk;
    }
    return done;
}

static int bulk_in(void *data, int len)
{
    unsigned char *p = (unsigned char *)data;
    int done = 0, chunk, got, st;
    while (done < len) {
        chunk = len - done; if (chunk > 64) chunk = 64;
        got = 0;
        st = bulk_pkt((unsigned char)(0x90 | (g_ep_in & 0x0F)),
                      (unsigned char)(0x03 | (g_tog_in ? 0x40 : 0)),
                      p + done, chunk, &got);
        if (st < 0 || !(st & 0x01)) return done ? done : -1;
        g_tog_in ^= 1;
        done += got;
        if (got < chunk) break;                             /* short packet */
    }
    return done;
}

static void clear_halt(unsigned char ep)
{
    control(g_addr, 0x02, 1, 0, ep, NULL, 0);   /* CLEAR_FEATURE ENDPOINT_HALT */
}

/*------------------------------------------------------------ BOT --------*/
static unsigned long g_tag = 1;

/* ring-0 BOT: hand the whole command to the VxD (CFU_MSCCMD).  Same
 * contract as bot(): returns CSW status (0=good) or negative on failure. */
static int bot_ring0(const unsigned char *cdb, int cdblen, int dir,
                     void *data, int datalen)
{
    static unsigned char in[20 + 512];
    static unsigned char out[4 + 512];
    unsigned long csw;
    memset(in, 0, sizeof in);
    in[0] = (unsigned char)cdblen;
    in[1] = (dir == 1) ? 1 : 0;
    in[2] = (unsigned char)datalen;
    in[3] = (unsigned char)(datalen >> 8);
    memcpy(in + 4, cdb, cdblen);
    if (dir == 0 && datalen) memcpy(in + 20, data, datalen);
    memset(out, 0, sizeof out);
    if (vxd_ioctl(CFU_MSCCMD, in, 20 + (dir == 0 ? datalen : 0),
                  out, 4 + (dir == 1 ? datalen : 0)) != 0)
        return -110;
    csw = out[0] | (out[1]<<8) | ((unsigned long)out[2]<<16) | ((unsigned long)out[3]<<24);
    if (csw == 0xFFFFFFFFUL) return -111;
    if (dir == 1 && data && datalen) memcpy(data, out + 4, datalen);
    return (int)csw;
}

/* run one SCSI command via Bulk-Only Transport.  dir: 1=IN 0=OUT -1=none.
 * returns CSW status (0=good), or negative on transport failure. */
static int bot(const unsigned char *cdb, int cdblen, int dir,
               void *data, int datalen)
{
    if (g_ring0)
        return bot_ring0(cdb, cdblen, dir, data, datalen);
    {
    unsigned char cbw[31], csw[13];
    int got, st;

    memset(cbw, 0, sizeof cbw);
    cbw[0]='U'; cbw[1]='S'; cbw[2]='B'; cbw[3]='C';         /* dCBWSignature */
    cbw[4]=(unsigned char)g_tag; cbw[5]=(unsigned char)(g_tag>>8);
    cbw[6]=(unsigned char)(g_tag>>16); cbw[7]=(unsigned char)(g_tag>>24);
    cbw[8]=(unsigned char)datalen; cbw[9]=(unsigned char)(datalen>>8);
    cbw[10]=(unsigned char)(datalen>>16); cbw[11]=(unsigned char)(datalen>>24);
    cbw[12]=(dir==1)?0x80:0x00;                             /* bmCBWFlags */
    cbw[13]=0;                                              /* LUN */
    cbw[14]=(unsigned char)cdblen;
    memcpy(cbw+15, cdb, cdblen);
    g_tag++;

    if (bulk_out(cbw, 31) < 0) return -100;

    if (dir == 1 && datalen) {
        if (bulk_in(data, datalen) < 0) { clear_halt(g_ep_in); }
    } else if (dir == 0 && datalen) {
        if (bulk_out(data, datalen) < 0) { clear_halt(g_ep_out); }
    }

    got = 0;
    st = bulk_pkt((unsigned char)(0x90 | (g_ep_in & 0x0F)),
                  (unsigned char)(0x03 | (g_tog_in ? 0x40 : 0)), csw, 13, &got);
    if (st < 0 || !(st & 0x01)) {                           /* stalled CSW */
        clear_halt(g_ep_in);
        got = 0;
        st = bulk_pkt((unsigned char)(0x90 | (g_ep_in & 0x0F)),
                      (unsigned char)(0x03 | (g_tog_in ? 0x40 : 0)), csw, 13, &got);
        if (st < 0 || !(st & 0x01)) return -101;
    }
    g_tog_in ^= 1;
    if (got < 13 || csw[0]!='U' || csw[1]!='S' || csw[2]!='B' || csw[3]!='S')
        return -102;
    return csw[12];                                         /* bCSWStatus */
    }
}

/*---------------------------------------------------------- SCSI ---------*/
static int scsi_inquiry(unsigned char *out36)
{
    unsigned char cdb[6] = {0x12,0,0,0,36,0};
    return bot(cdb, 6, 1, out36, 36);
}
static int scsi_test_unit_ready(void)
{
    unsigned char cdb[6] = {0,0,0,0,0,0};
    return bot(cdb, 6, -1, NULL, 0);
}
static int scsi_request_sense(unsigned char *out18)
{
    unsigned char cdb[6] = {0x03,0,0,0,18,0};
    return bot(cdb, 6, 1, out18, 18);
}
static int scsi_read_capacity(void)
{
    unsigned char cdb[10] = {0x25,0,0,0,0,0,0,0,0,0};
    unsigned char d[8];
    int r = bot(cdb, 10, 1, d, 8);
    if (r != 0) return r;
    g_blocks = ((unsigned long)d[0]<<24)|((unsigned long)d[1]<<16)|
               ((unsigned long)d[2]<<8)|d[3];              /* last LBA */
    g_blocks += 1;
    g_bsize  = ((unsigned long)d[4]<<24)|((unsigned long)d[5]<<16)|
               ((unsigned long)d[6]<<8)|d[7];
    if (!g_bsize) g_bsize = 512;
    return 0;
}
static int scsi_read10(unsigned long lba, unsigned short count, void *buf)
{
    unsigned char cdb[10];
    cdb[0]=0x28; cdb[1]=0;
    cdb[2]=(unsigned char)(lba>>24); cdb[3]=(unsigned char)(lba>>16);
    cdb[4]=(unsigned char)(lba>>8);  cdb[5]=(unsigned char)lba;
    cdb[6]=0; cdb[7]=(unsigned char)(count>>8); cdb[8]=(unsigned char)count;
    cdb[9]=0;
    return bot(cdb, 10, 1, buf, count * (int)g_bsize);
}

/*------------------------------------------------------ enumerate --------*/
/* returns device class byte, or -1 on failure; fills g_addr/g_mps0 */
static int get_devdesc(unsigned char addr, unsigned char *d18)
{
    return control(addr, 0x80, 6, 0x0100, 0, d18, 18);
}

/* if the attached device is a hub, power its ports, find one with a
 * device, reset it, and give that device address 2.  Returns new address. */
static int hub_descend(void)
{
    unsigned char hd[8], ps[4], dd[18];
    int nports, p, r;

    r = control(1, 0xA0, 6, 0x2900, 0, hd, 8);   /* GET hub descriptor */
    if (r < 4) { printf("  hub descriptor read failed (%d)\n", r); return -1; }
    nports = hd[2];
    printf("  hub with %d port(s); powering ports...\n", nports);
    for (p = 1; p <= nports; p++)
        control(1, 0x23, 3, 8, p, NULL, 0);       /* SET_FEATURE PORT_POWER */
    Sleep(200);
    for (p = 1; p <= nports; p++) {
        if (control(1, 0xA3, 0, 0, p, ps, 4) < 4) continue;
        if (!(ps[0] & 0x01)) continue;            /* no device on this port */
        printf("  device on port %d, resetting...\n", p);
        control(1, 0x23, 3, 4, p, NULL, 0);       /* SET_FEATURE PORT_RESET */
        Sleep(60);
        control(1, 0x23, 1, 0x14, p, NULL, 0);    /* CLEAR_FEATURE C_PORT_RESET */
        Sleep(20);
        /* downstream device now answers at address 0 */
        g_addr = 0; g_mps0 = 8;
        if (get_devdesc(0, dd) < 8) { printf("  downstream desc failed\n"); return -1; }
        g_mps0 = dd[7] ? dd[7] : 8;
        if (control(0, 0x00, 5, 2, 0, NULL, 0) < 0)   /* SET_ADDRESS 2 */
            { printf("  SET_ADDRESS(2) failed\n"); return -1; }
        Sleep(20);
        g_addr = 2;
        return dd[4];                              /* downstream class */
    }
    printf("  no device found on any hub port\n");
    return -1;
}

/* find the mass-storage interface's bulk endpoints from the config desc */
static int find_bulk_endpoints(unsigned char addr)
{
    unsigned char cfg[9], all[256];
    int total, i, in = 0, out = 0, found = 0;

    if (control(addr, 0x80, 6, 0x0200, 0, cfg, 9) < 9) return -1;
    total = cfg[2] | (cfg[3] << 8);
    if (total > (int)sizeof all) total = sizeof all;
    if (control(addr, 0x80, 6, 0x0200, 0, all, total) < total) return -1;

    for (i = 0; i + 1 < total; i += all[i]) {
        if (all[i] == 0) break;
        if (all[i+1] == 4) {                       /* interface descriptor */
            /* class 08 subclass 06 (SCSI) protocol 50 (BOT) */
            found = (all[i+5] == 8 && all[i+7] == 0x50);
        } else if (all[i+1] == 5 && found) {       /* endpoint descriptor */
            if ((all[i+3] & 3) == 2) {             /* bulk */
                if (all[i+2] & 0x80) in = all[i+2]; else out = all[i+2];
            }
        }
    }
    if (!in || !out) return -1;
    g_ep_in = (unsigned char)in;
    g_ep_out = (unsigned char)out;
    /* SET_CONFIGURATION 1 */
    control(addr, 0x00, 9, 1, 0, NULL, 0);
    g_tog_in = g_tog_out = 0;
    return 0;
}

static int enumerate(void)
{
    unsigned char lowspeed = 0, d18[18];
    int cls;

    vxd_ioctl(CFU_USBINIT, &lowspeed, 1, NULL, 0);  /* reset + host init */
    g_addr = 0; g_mps0 = 8;
    if (get_devdesc(0, d18) < 8) { printf("no device answering\n"); return -1; }
    g_mps0 = d18[7] ? d18[7] : 8;
    cls = d18[4];
    if (control(0, 0x00, 5, 1, 0, NULL, 0) < 0) { printf("SET_ADDRESS failed\n"); return -1; }
    Sleep(20);
    g_addr = 1;

    if (cls == 0x09) {                              /* it's a hub */
        printf("directly-attached device is a USB hub:\n");
        cls = hub_descend();
        if (cls < 0) return -1;
    }
    printf("storage device: class %02X at address %u, ep0 mps %u\n",
           cls, g_addr, g_mps0);
    if (find_bulk_endpoints(g_addr) != 0) {
        printf("no Bulk-Only mass-storage interface found (class 08/06/50)\n");
        return -1;
    }
    printf("bulk endpoints: IN 0x%02X, OUT 0x%02X\n", g_ep_in, g_ep_out);
    if (g_ring0) {
        unsigned char s[3];
        s[0] = g_addr; s[1] = g_ep_in; s[2] = g_ep_out;
        vxd_ioctl(CFU_MSCSET, s, 3, NULL, 0);   /* hand state to the VxD */
        printf("(ring-0 BOT path: commands run inside the VxD)\n");
    }
    return 0;
}

/*------------------------------------------------------- helpers ---------*/
static void hexdump(const unsigned char *b, int n, unsigned long base)
{
    int i, j;
    for (i = 0; i < n; i += 16) {
        printf("%08lX ", base + i);
        for (j = 0; j < 16; j++) printf("%02X ", b[i+j]);
        printf(" ");
        for (j = 0; j < 16; j++) {
            unsigned char c = b[i+j];
            putchar((c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
}

static int read_sector(unsigned long lba, unsigned char *buf)
{
    unsigned char sense[18];
    int i, r;
    for (i = 0; i < 3; i++) {                       /* devices often NAK once */
        r = scsi_read10(lba, 1, buf);
        if (r == 0) return 0;
        scsi_request_sense(sense);
        Sleep(20);
    }
    return -1;
}

/* FAT BPB, filled from the volume boot sector */
static int   f_bps, f_spc, f_is32;
static unsigned short f_rsvd, f_rootent;
static unsigned char  f_nfat;
static unsigned long  f_spf, f_rootclus, f_datalba;

static void fat_parse_bpb(const unsigned char *bs)
{
    unsigned short spf16 = bs[22] | (bs[23] << 8);
    f_bps  = bs[11] | (bs[12] << 8);
    f_spc  = bs[13];
    f_rsvd = (unsigned short)(bs[14] | (bs[15] << 8));
    f_nfat = bs[16];
    f_rootent = (unsigned short)(bs[17] | (bs[18] << 8));
    f_is32 = (spf16 == 0);
    if (f_is32) {
        f_spf = bs[36]|((unsigned long)bs[37]<<8)|((unsigned long)bs[38]<<16)|((unsigned long)bs[39]<<24);
        f_rootclus = bs[44]|((unsigned long)bs[45]<<8)|((unsigned long)bs[46]<<16)|((unsigned long)bs[47]<<24);
    } else {
        f_spf = spf16;
        f_rootclus = 0;
    }
    f_datalba = g_part_lba + f_rsvd + (unsigned long)f_nfat * f_spf;
}

/* FAT32 next-cluster lookup */
static unsigned long fat32_next(unsigned long clus)
{
    unsigned char fs[512];
    unsigned long fatlba = g_part_lba + f_rsvd + (clus * 4) / f_bps;
    int off = (int)((clus * 4) % f_bps);
    if (read_sector(fatlba, fs) != 0) return 0x0FFFFFFF;
    return (fs[off] | ((unsigned long)fs[off+1]<<8) |
            ((unsigned long)fs[off+2]<<16) | ((unsigned long)fs[off+3]<<24)) & 0x0FFFFFFF;
}

/* print one 32-byte directory entry; returns 1 at end-of-directory marker */
static int dir_entry(const unsigned char *de, int *shown)
{
    char name[13];
    int k, n = 0;
    unsigned long sz;
    if (de[0] == 0x00) return 1;                    /* end of directory */
    if (de[0] == 0xE5) return 0;                    /* deleted */
    if (de[11] == 0x0F) return 0;                   /* long-name fragment */
    if ((de[11] & 0x08) && !(de[11] & 0x10)) return 0;  /* volume label */
    for (k = 0; k < 8 && de[k] != ' '; k++) name[n++] = de[k];
    if (de[8] != ' ') {
        name[n++] = '.';
        for (k = 8; k < 11 && de[k] != ' '; k++) name[n++] = de[k];
    }
    name[n] = 0;
    sz = de[28] | ((unsigned long)de[29]<<8) |
         ((unsigned long)de[30]<<16) | ((unsigned long)de[31]<<24);
    printf("    %-12s %s %lu bytes\n", name,
           (de[11] & 0x10) ? "<DIR>" : "     ", sz);
    (*shown)++;
    return 0;
}

/* scan one directory sector; returns 1 if end-of-directory was hit */
static int dir_sector(const unsigned char *s, int *shown)
{
    int e;
    for (e = 0; e < f_bps; e += 32)
        if (dir_entry(s + e, shown)) return 1;
    return 0;
}

static void list_root(void)
{
    unsigned char sec[512];
    int shown = 0, s;
    printf("  root directory:\n");
    if (f_is32) {
        unsigned long clus = f_rootclus;
        while (clus >= 2 && clus < 0x0FFFFFF8) {
            unsigned long first = f_datalba + (clus - 2) * f_spc;
            for (s = 0; s < f_spc; s++) {
                if (read_sector(first + s, sec) != 0) { clus = 0; break; }
                if (dir_sector(sec, &shown)) { clus = 0; break; }
            }
            if (clus == 0) break;
            clus = fat32_next(clus);
        }
    } else {
        unsigned long rootlba = g_part_lba + f_rsvd + (unsigned long)f_nfat * f_spf;
        unsigned long rootsecs = ((unsigned long)f_rootent * 32 + f_bps - 1) / f_bps;
        for (s = 0; (unsigned long)s < rootsecs; s++) {
            if (read_sector(rootlba + s, sec) != 0) break;
            if (dir_sector(sec, &shown)) break;
        }
    }
    if (!shown) printf("    (empty)\n");
}

/*--------------------------------------------------------- main ----------*/
int main(int argc, char **argv)
{
    unsigned char inq[36], sec[512], sense[18];
    int i, want_sec = -1, want_dir = 0, r;

    int mount_r0 = 0;
    for (i = 1; i < argc; i++) {
        if (!stricmp(argv[i], "/SEC") && i + 1 < argc) want_sec = atoi(argv[++i]);
        else if (!stricmp(argv[i], "/DIR")) want_dir = 1;
        else if (!stricmp(argv[i], "/R0")) g_ring0 = 1;
        else if (!stricmp(argv[i], "/MOUNT")) { mount_r0 = 1; g_ring0 = 1; }
    }

    printf("CFUDISK - USB mass storage on the REX-CFU1\n");
    hVxd = load_vxd();
    if (hVxd == INVALID_HANDLE_VALUE) {
        printf("cannot load CFU1.VXD (%lu)\n", GetLastError());
        return 1;
    }
    if (mount_r0) {
        DWORD cap[2]; cap[0] = cap[1] = 0;
        if (vxd_ioctl(CFU_MSCMOUNT, NULL, 0, cap, 8) != 0 || cap[0] == 0) {
            printf("VxD ring-0 mount failed (blocks=%lu)\n", cap[0]);
            CloseHandle(hVxd); return 1;
        }
        g_blocks = cap[0]; g_bsize = cap[1];
        printf("VxD ring-0 mount OK: %lu blocks x %lu bytes = %.1f MB\n",
               cap[0], cap[1], (double)cap[0]*cap[1]/(1024.0*1024.0));
        /* VxD already set its msc state; commands run ring-0 (g_ring0=1) */
    } else if (enumerate() != 0) { CloseHandle(hVxd); return 1; }

    /* spin up / wait ready */
    for (i = 0; i < 20; i++) {
        if (scsi_test_unit_ready() == 0) break;
        scsi_request_sense(sense);
        Sleep(100);
    }

    if (scsi_inquiry(inq) == 0) {
        char vendor[9], product[17];
        memcpy(vendor, inq + 8, 8); vendor[8] = 0;
        memcpy(product, inq + 16, 16); product[16] = 0;
        printf("INQUIRY: '%s' '%s'  (peripheral type %02X)\n",
               vendor, product, inq[0] & 0x1F);
    } else printf("INQUIRY failed\n");

    if (scsi_read_capacity() == 0) {
        double mb = (double)g_blocks * (double)g_bsize / (1024.0 * 1024.0);
        printf("CAPACITY: %lu blocks x %lu bytes = %.1f MB\n",
               g_blocks, g_bsize, mb);
    } else { printf("READ CAPACITY failed\n"); CloseHandle(hVxd); return 1; }

    if (want_sec >= 0) {
        if (read_sector((unsigned long)want_sec, sec) == 0) {
            printf("--- sector %d ---\n", want_sec);
            hexdump(sec, 512, 0);
        } else printf("read of sector %d failed\n", want_sec);
        CloseHandle(hVxd);
        return 0;
    }

    /* MBR: read sector 0, find the first partition */
    if (read_sector(0, sec) != 0) { printf("MBR read failed\n"); CloseHandle(hVxd); return 1; }
    if (sec[510] == 0x55 && sec[511] == 0xAA) {
        int part;
        printf("MBR partition table:\n");
        for (part = 0; part < 4; part++) {
            unsigned char *e = sec + 446 + part * 16;
            unsigned long start = e[8] | ((unsigned long)e[9]<<8) |
                                  ((unsigned long)e[10]<<16) | ((unsigned long)e[11]<<24);
            unsigned long size = e[12] | ((unsigned long)e[13]<<8) |
                                 ((unsigned long)e[14]<<16) | ((unsigned long)e[15]<<24);
            if (e[4] == 0) continue;
            printf("  #%d type %02X  start %lu  size %lu (%.1f MB)\n",
                   part+1, e[4], start, size, (double)size*g_bsize/(1024.0*1024.0));
            if (!g_part_lba) g_part_lba = start;
        }
    } else {
        printf("no MBR signature; treating sector 0 as the boot sector\n");
        g_part_lba = 0;
    }

    /* boot sector of the (first) volume */
    if (read_sector(g_part_lba, sec) == 0 &&
        (sec[510] == 0x55 && sec[511] == 0xAA)) {
        char oem[9], label[12];
        memcpy(oem, sec + 3, 8); oem[8] = 0;
        fat_parse_bpb(sec);
        printf("volume @ LBA %lu: OEM '%s', %d bytes/sector, %d sec/cluster\n",
               g_part_lba, oem, f_bps, f_spc);
        if (f_is32) {
            memcpy(label, sec + 71, 11); label[11] = 0;
            printf("  FAT32, root cluster %lu, label '%s'\n", f_rootclus, label);
        } else if (!memcmp(sec + 54, "FAT", 3)) {
            memcpy(label, sec + 43, 11); label[11] = 0;
            printf("  FAT type '%.8s', label '%s'\n", sec + 54, label);
        }
        if (want_dir) list_root();
    } else
        printf("boot sector unreadable or unsigned\n");

    printf("\n*** USB MASS STORAGE READABLE ON WINDOWS 98 VIA THE REX-CFU1 ***\n");
    CloseHandle(hVxd);
    return 0;
}
