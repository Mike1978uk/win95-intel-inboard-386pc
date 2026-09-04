/* CSTAT.C - CFU_AUTOSTAT (run/armed/state/hub_port/hub_naddr) plus
 * CFU_HUBSTAT hub-descent diagnostics (per-port wPortStatus etc).
 */
#include <windows.h>
#include <stdio.h>
#include "cfu1.h"
int main(void){
    HANDLE h; DWORD s=0,r=0,hs[18]; int i; char p[]="\\\\.\\CFU1.VXD";
    h=CreateFile(p,0,0,NULL,0,0,NULL);
    if(h==INVALID_HANDLE_VALUE){printf("no vxd\n");return 1;}
    if(DeviceIoControl(h,CFU_AUTOSTAT,NULL,0,&s,4,&r,NULL))
        printf("raw=%08lX run=%lu armed=%lu state=%lu hub_port=%lu next_addr=%lu\n",
               s, s&1, (s>>1)&1, (s>>8)&255, (s>>16)&255, (s>>24)&255);
    else printf("AUTOSTAT ioctl failed\n");
    if(DeviceIoControl(h,CFU_HUBSTAT,NULL,0,hs,sizeof(hs),&r,NULL)){
        printf("hub: nports=%lu port=%lu naddr=%lu conn=%lu rescan=%lu "
               "pwrfail=%lu p2pg=%lums estage=%lu\n",
               hs[0],hs[1],hs[2],hs[3],hs[4],hs[5],hs[6]*2,hs[7]);
        printf("slots: primary=port %lu  B=port %lu  parked=%02lX\n",
               hs[15], hs[16], hs[17]);
        for(i=0;i<7 && (DWORD)i<hs[0];i++){
            printf("  port %d: ",i+1);
            if(hs[8+i]==0xFFFFFFFFUL)      printf("GET_STATUS FAILED\n");
            else if(hs[8+i]==0xFFFFFFFEUL) printf("GET_STATUS SHORT\n");
            else printf("status=%04lX change=%04lX%s%s%s%s\n",
                        hs[8+i]&0xFFFF, hs[8+i]>>16,
                        (hs[8+i]&1)?" CONN":"", (hs[8+i]&2)?" ENA":"",
                        (hs[8+i]&0x100)?" PWR":"", (hs[8+i]&0x200)?" LS":"");
        }
    } else printf("HUBSTAT ioctl failed (old VxD?)\n");
    CloseHandle(h); return 0;
}
