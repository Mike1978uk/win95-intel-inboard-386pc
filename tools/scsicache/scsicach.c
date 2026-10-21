/* SCSICACH: list every SCSI target Windows 95's ASPI layer can see, with its
 * caching page (MODE SENSE page 8: WCE = write cache on, RCD = read cache off).
 * Read only: MODE SELECT is never sent, because WCE trades data integrity for
 * speed and is the owner's choice (#31). Writes C:\SCSICACH.TXT.
 *
 * WNASPI32.DLL is loaded at run time, so no import library is needed; on
 * Windows 95 it reaches miniport adapters (T130.MPD) through IOS. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#pragma pack(1)
typedef struct {
    BYTE  Cmd, Status, HaId, Flags;
    DWORD HdrRsvd;
    BYTE  Target, Lun;
    WORD  Rsvd1;
    DWORD BufLen;
    BYTE  *BufPointer;
    BYTE  SenseLen, CDBLen, HaStat, TargStat;
    void  *PostProc;
    BYTE  Rsvd2[20];
    BYTE  CDB[16];
    BYTE  Sense[18];
} SRB_EXEC;
#pragma pack()

#define SC_EXEC_SCSI_CMD    0x02
#define SRB_DIR_IN          0x08
#define SS_PENDING          0x00
#define SS_COMP             0x01

/* WNASPI32 exports are cdecl; Watcom would otherwise pass arguments in registers. */
typedef DWORD (__cdecl *ASPI_INFO)( void );
typedef DWORD (__cdecl *ASPI_SEND)( void * );

static ASPI_SEND send_srb;

/* One command, polled to completion: no event or post routine to set up. */
static int exec( int ha, int t, BYTE *cdb, int cdblen, BYTE *buf, DWORD len, SRB_EXEC *s )
{
    DWORD start = GetTickCount();

    memset( s, 0, sizeof( *s ) );
    s->Cmd = SC_EXEC_SCSI_CMD;
    s->HaId = (BYTE)ha;
    s->Flags = SRB_DIR_IN;
    s->Target = (BYTE)t;
    s->BufLen = len;
    s->BufPointer = buf;
    s->SenseLen = 14;
    s->CDBLen = (BYTE)cdblen;
    memcpy( s->CDB, cdb, cdblen );
    send_srb( s );
    /* ASPI completes the request asynchronously: re-read Status every poll. */
    while( *(volatile BYTE *)&s->Status == SS_PENDING && GetTickCount() - start < 10000 ) {
        Sleep( 10 );
    }
    return( s->Status == SS_COMP );
}

int main( void )
{
    HINSTANCE   dll = LoadLibrary( "WNASPI32.DLL" );
    ASPI_INFO   info;
    DWORD       r;
    int         nha, ha, t;
    BYTE        inq[36], ms[255];
    BYTE        cinq[6] = { 0x12, 0, 0, 0, 36, 0 };
    BYTE        cms[6]  = { 0x1A, 0x08, 0x08, 0, 255, 0 };   /* DBD, page 8, current */
    SRB_EXEC    s;
    FILE        *f = fopen( "C:\\SCSICACH.TXT", "w" );

    if( f == NULL ) {
        return( 1 );
    }
    if( dll == NULL ) {
        fprintf( f, "WNASPI32.DLL not loaded\n" );
        fclose( f );
        return( 1 );
    }
    info = (ASPI_INFO)GetProcAddress( dll, "GetASPI32SupportInfo" );
    send_srb = (ASPI_SEND)GetProcAddress( dll, "SendASPI32Command" );
    if( info == NULL || send_srb == NULL ) {
        fprintf( f, "ASPI entry points missing\n" );
        fclose( f );
        return( 1 );
    }
    r = info();
    nha = (int)(r & 0xFF);
    fprintf( f, "ASPI status %02lX, adapters %d\n", (r >> 8) & 0xFF, nha );
    for( ha = 0; ha < nha; ++ha ) {
        for( t = 0; t < 8; ++t ) {
            memset( inq, 0, sizeof( inq ) );
            if( !exec( ha, t, cinq, 6, inq, 36, &s ) ) {
                continue;
            }
            fprintf( f, "%d:%d type %02X %.8s %.16s %.4s : ", ha, t, inq[0] & 0x1F,
                     inq + 8, inq + 16, inq + 32 );
            memset( ms, 0, sizeof( ms ) );
            /* Unit attention (sense key 6) is a one-time notice: ask again. Older
             * drives reject DBD (5/24 invalid field): ask with block descriptors. */
            cms[1] = 0x08;
            if( !exec( ha, t, cms, 6, ms, 255, &s ) && (s.Sense[2] & 0x0F) == 6 ) {
                exec( ha, t, cms, 6, ms, 255, &s );
            }
            if( s.Status != SS_COMP && (s.Sense[2] & 0x0F) == 5 ) {
                cms[1] = 0;
                exec( ha, t, cms, 6, ms, 255, &s );
            }
            if( s.Status != SS_COMP ) {
                fprintf( f, "no page 8 (status %02X, sense key %X, ASC %02X)\n",
                         s.Status, s.Sense[2] & 0x0F, s.Sense[12] );
                continue;
            }
            {
                BYTE *pg = ms + 4 + ms[3];

                if( (pg[0] & 0x3F) != 0x08 ) {
                    fprintf( f, "page %02X returned, not 08\n", pg[0] & 0x3F );
                } else {
                    fprintf( f, "WCE=%d RCD=%d (write cache %s, read cache %s)\n",
                             (pg[2] >> 2) & 1, pg[2] & 1,
                             (pg[2] & 4) ? "on" : "off", (pg[2] & 1) ? "off" : "on" );
                }
            }
        }
    }
    fclose( f );
    return( 0 );
}
