/* CFU1.H - shared ioctl ABI between CFU1.VXD and CFUDIAG.EXE.
 * Keep in sync with the equates at the top of win/vxd/CFU1.ASM.
 */
#ifndef CFU1_H
#define CFU1_H

#define CFU_GETVER      0x1000  /* out: DWORD version (0x00010000)          */
#define CFU_ENABLE      0x1001  /* in: CFU_SETUP  out: CFU_RESULT           */
#define CFU_DISABLE     0x1002
#define CFU_RDREG       0x1003  /* in: BYTE reg              out: DWORD val */
#define CFU_WRREG       0x1004  /* in: BYTE reg, BYTE val                   */
#define CFU_RDATTR      0x1005  /* in: WORD offset           out: DWORD val */
#define CFU_WRATTR      0x1006  /* in: WORD offset, BYTE val                */
#define CFU_IRQHOOK     0x1007  /* in: BYTE irq                             */
#define CFU_IRQCOUNT    0x1008  /* out: DWORD count                         */
#define CFU_IRQUNHOOK   0x1009
#define CFU_PCICRD      0x100A  /* in: BYTE reg (socket-relative) out: DWORD*/
#define CFU_PCICWR      0x100B  /* in: BYTE reg, BYTE val                   */
#define CFU_GETCONF     0x100C  /* out: CFU_CONF                            */
#define CFU_USBINIT     0x100D  /* in: BYTE lowspeed  out: DWORD INTSTAT    */
#define CFU_XACT        0x100E  /* in: CFU_XREQ       out: CFU_XRSP         */
#define CFU_CTRL        0x100F  /* in: CFU_CREQ       out: CFU_CRSP         */
#define CFU_HIDSTART    0x1010  /* in: BYTE ep, devaddr, mps, interval_ms   */
#define CFU_HIDSTOP     0x1011
#define CFU_HIDREAD     0x1012  /* out: DWORD count + count*(1+8) bytes     */
#define CFU_AUTOMOUSE   0x1013  /* in: BYTE 1=on/0=off (hot-plug supervisor)*/
#define CFU_AUTOSTAT    0x1014  /* out: DWORD b0 run, b1 armed, b8.. state  */
#define CFU_FORCEKEY    0x1015  /* in: raw scan bytes -> VKD_Force_Keys     */
#define CFU_KBDSTART    0x1016  /* in: BYTE ep, devaddr, mps, interval_ms   */
#define CFU_KBDSTAT     0x1017  /* out: 6 DWORDs pipeline counters (debug)  */
#define CFU_MSCSET      0x1018  /* in: BYTE addr, ep_in, ep_out             */
#define CFU_MSCCMD      0x1019  /* in: cdblen,dir,datalen,cdb[16],out-data;
                                   out: DWORD csw, in-data                  */
#define CFU_IOSREG      0x101A  /* register with IOS; out: DWORD reg_result */
#define CFU_IOSSTAT     0x101B  /* out: total, lastfunc, regresult, cnt[16] */
#define CFU_MSCMOUNT    0x101C  /* ring-0 enumerate; out: blocks, block_size*/
#define CFU_IORSTAT     0x101D  /* out: total, last, isp, cfgdcb, inqdcb,
                                   cnt[48]                                  */
#define CFU_PEEK        0x101E  /* in: DWORD addr, DWORD len(<=256); out raw*/
#define CFU_DCBENUM     0x101F  /* out: DWORD count + up to 15 DCB addrs    */
#define CFU_IOSCFG      0x1020  /* broadcast AEP_CONFIG_DCB; out: results   */
#define CFU_IOSVOL      0x1021  /* create logical vol DCB + associate;
                                   out: DWORD (create<<16)|assoc            */
#define CFU_VOLUP       0x1022  /* mount + volume up; out: (mnt<<16)|hasvol */
#define CFU_VOLDOWN     0x1023  /* polite teardown; in: BYTE force;
                                   out: 0 ok, 1 vetoed, 2 no volume         */
#define CFU_VOLSTAT     0x1024  /* out: mounted, log_dcb, part_start,
                                   part_len, drive_index (5 DWORDs)         */
#define CFU_IORTRACE    0x1025  /* out: DWORD total + 32*(func,flags,lba,n) */
#define CFU_IRQTEST     0x1026  /* in: BYTE irq; out: DWORD count (~1s)     */
#define CFU_HUBSTAT     0x1027  /* out: 18 DWORDs: nports, port, naddr,
                                   conn, rescan, pwrfail, p2pg, estage,
                                   pstat[7] (FFFFFFFF fail, FFFFFFFE short),
                                   hid_port, hd2_port, parked mask          */
#define CFU_ASYNCA      0x1028  /* in: BYTE 1/0 stage-A IRQ completion
                                   on/off; out: DWORD irqcount             */
#define CFU_ASYNCST     0x1029  /* out: 4 DWORDs: irqcount, IRQ-completed
                                   windows, timeouts, amode                */

#pragma pack(push, 1)
typedef struct {
    unsigned short  iobase;     /* 0 = default 0x260                        */
    unsigned short  pcic;       /* 0 = default 0x3E0                        */
    unsigned char   socket;     /* 0..3                                     */
    unsigned char   cor;        /* reserved; VxD writes 0x02                */
    unsigned short  attrseg;    /* 0 = default 0xD000                       */
} CFU_SETUP;

typedef struct {
    unsigned long   err;        /* 0 ok, else CFUERR_*                      */
    unsigned char   corrb;      /* COR readback, expect 0x42                */
    unsigned char   hwrev;      /* SL811 reg 0x0E, expect 0x20 (HS v1.5)    */
    unsigned char   pcicstat;   /* PCIC interface status register           */
    unsigned char   rsvd;
} CFU_RESULT;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    unsigned short  iobase;     /* Windows-assigned I/O base (0 = none)     */
    unsigned char   irq;        /* Windows-assigned IRQ (0xFF = none)       */
    unsigned char   socket;     /* socket found by cor_fix (0xFF = none)    */
    unsigned char   flags;      /* bit0 devnode bit1 started bit2 COR-fixed */
    unsigned char   corrb;      /* COR readback at CONFIG_START             */
    unsigned short  rsvd;
} CFU_CONF;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {                /* one raw transaction on EP A */
    unsigned char   pidep;      /* PID<<4 | endpoint                        */
    unsigned char   devaddr;
    unsigned char   ctl;        /* EP A host control bits                   */
    unsigned char   len;        /* <= 120                                   */
    unsigned char   data[120];  /* OUT/SETUP payload                        */
} CFU_XREQ;

typedef struct {
    unsigned long   status;     /* SL811 packet status; 0xFFFFFFFF timeout  */
    unsigned long   got;        /* bytes received (IN)                      */
    unsigned char   data[120];
} CFU_XRSP;

typedef struct {                /* complete control transfer */
    unsigned char   devaddr;
    unsigned char   mps;        /* ep0 max packet size                      */
    unsigned short  wlen;       /* data stage length                        */
    unsigned char   setup[8];
} CFU_CREQ;

typedef struct {
    unsigned long   err;        /* 0 ok; low byte stage 1/2/3, byte1 status */
    unsigned long   got;
    unsigned char   data[256];
} CFU_CRSP;
#pragma pack(pop)

#define CFUERR_NOCARD   1
#define CFUERR_NOREADY  2
#define CFUERR_NOMAP    3
#define CFUERR_BUFFER   4
#define CFUERR_NOTENAB  5
#define CFUERR_BADIOCTL 6

#endif
