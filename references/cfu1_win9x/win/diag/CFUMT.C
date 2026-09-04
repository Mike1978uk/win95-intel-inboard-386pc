/* CFUMT.C - minimal ring-0 mass-storage mount test (small, to stay under
 * the COMrade large-write threshold).  Uses CFU_MSCMOUNT (VxD enumerates
 * the stick + READ CAPACITY) then CFU_MSCCMD to run INQUIRY and read
 * sector 0 - all inside the VxD.  Proves the VxD can bring up the disk on
 * its own, the prerequisite for the IOS drive-letter path.
 */
#include <windows.h>
#include <stdio.h>
#include "cfu1.h"

static HANDLE h;
static int ioc(DWORD c, void *in, DWORD ci, void *out, DWORD co)
{ DWORD r=0; return DeviceIoControl(h,c,in,ci,out,co,&r,NULL)?0:-1; }

/* one BOT command through the VxD; returns CSW status or <0 */
static int bot(const unsigned char *cdb, int cl, int dir, void *data, int dl)
{
    static unsigned char in[20+512], out[4+512];
    unsigned long csw;
    memset(in,0,sizeof in);
    in[0]=(unsigned char)cl; in[1]=(dir==1)?1:0;
    in[2]=(unsigned char)dl; in[3]=(unsigned char)(dl>>8);
    memcpy(in+4,cdb,cl);
    memset(out,0,sizeof out);
    if (ioc(CFU_MSCCMD,in,20,out,4+(dir==1?dl:0))!=0) return -1;
    csw=out[0]|(out[1]<<8)|((unsigned long)out[2]<<16)|((unsigned long)out[3]<<24);
    if (csw==0xFFFFFFFFUL) return -2;
    if (dir==1&&data&&dl) memcpy(data,out+4,dl);
    return (int)csw;
}

int main(void)
{
    DWORD cap[2]={0,0};
    unsigned char inq[36], sec[512];
    unsigned char rd[10]={0x28,0,0,0,0,0,0,0,1,0};
    unsigned char iq[6]={0x12,0,0,0,36,0};
    int r;
    printf("CFUMT - ring-0 mount test\n");
    h=CreateFile("\\\\.\\CFU1.VXD",0,0,NULL,0,FILE_FLAG_DELETE_ON_CLOSE,NULL);
    if (h==INVALID_HANDLE_VALUE){printf("open failed %lu\n",GetLastError());return 1;}

    printf("CFU_MSCMOUNT (VxD enumerates in ring 0)...\n");
    if (ioc(CFU_MSCMOUNT,NULL,0,cap,8)!=0){printf("MSCMOUNT ioctl failed %lu\n",GetLastError());return 1;}
    printf("  blocks=%lu block_size=%lu  => %.1f MB\n",
           cap[0],cap[1],(double)cap[0]*cap[1]/(1024.0*1024.0));
    if (!cap[0]){printf("mount failed (no capacity)\n");CloseHandle(h);return 1;}

    memset(inq,0,sizeof inq);
    r=bot(iq,6,1,inq,36);
    if (r==0){char v[9],p[17];memcpy(v,inq+8,8);v[8]=0;memcpy(p,inq+16,16);p[16]=0;
        printf("  INQUIRY: '%s' '%s'\n",v,p);}
    else printf("  INQUIRY failed (%d)\n",r);

    memset(sec,0,sizeof sec);
    r=bot(rd,10,1,sec,512);
    if (r==0){
        printf("  sector 0: %02X %02X %02X %02X ... sig %02X%02X (%s)\n",
               sec[0],sec[1],sec[2],sec[3],sec[510],sec[511],
               (sec[510]==0x55&&sec[511]==0xAA)?"MBR OK":"no sig");
        printf("*** RING-0 VxD MOUNT + READ WORKS ***\n");
    } else printf("  sector 0 read failed (%d)\n",r);
    CloseHandle(h);
    return 0;
}
