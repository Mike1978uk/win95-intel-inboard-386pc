/* DSTEST.EXE - one DirectSound secondary buffer played and left alone, the way
 * dxdiag's sound test does it, with DirectSound's view of that buffer logged
 * every 20 ms. Nothing else touches DirectSound while it plays, so the log
 * shows whether its mixer keeps the buffer going on its own.
 *
 *   DSTEST once  <label>   1 s 440 Hz tone, Play without DSBPLAY_LOOPING
 *   DSTEST loop  <label>   the same buffer looping, stopped after 2 s
 *   DSTEST mono  <label>   as once, after SetFormat 22 kHz 8-bit mono on the
 *                          primary (dxdiag's first test)
 *   DSTEST mmtimer <label> no sound: a 20 ms periodic timeSetEvent counted for
 *                          2 s (DirectSound's mixer runs on one, set by DDHELP)
 *   DSTEST onceq <label>   as once, but nothing asked of DirectSound until 2 s
 *                          (asking for the position may itself wake the mixer)
 *
 * Appends to C:\DSTEST.TXT: time since Play, play and write cursors, status.
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -fe=DSTEST.EXE DSTEST.C
 */

#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

/* No dsound.h in this compiler: only the calls used. */
typedef struct DSB DSB;
typedef struct {
    HRESULT (WINAPI *QueryInterface)( DSB *, REFIID, LPVOID * );
    ULONG   (WINAPI *AddRef)( DSB * );
    ULONG   (WINAPI *Release)( DSB * );
    HRESULT (WINAPI *GetCaps)( DSB *, LPVOID );
    HRESULT (WINAPI *GetCurrentPosition)( DSB *, LPDWORD, LPDWORD );
    HRESULT (WINAPI *GetFormat)( DSB *, LPVOID, DWORD, LPDWORD );
    HRESULT (WINAPI *GetVolume)( DSB *, LPLONG );
    HRESULT (WINAPI *GetPan)( DSB *, LPLONG );
    HRESULT (WINAPI *GetFrequency)( DSB *, LPDWORD );
    HRESULT (WINAPI *GetStatus)( DSB *, LPDWORD );
    HRESULT (WINAPI *Initialize)( DSB *, LPVOID, LPVOID );
    HRESULT (WINAPI *Lock)( DSB *, DWORD, DWORD, LPVOID *, LPDWORD, LPVOID *, LPDWORD, DWORD );
    HRESULT (WINAPI *Play)( DSB *, DWORD, DWORD, DWORD );
    HRESULT (WINAPI *SetCurrentPosition)( DSB *, DWORD );
    HRESULT (WINAPI *SetFormat)( DSB *, LPVOID );
    HRESULT (WINAPI *SetVolume)( DSB *, LONG );
    HRESULT (WINAPI *SetPan)( DSB *, LONG );
    HRESULT (WINAPI *SetFrequency)( DSB *, DWORD );
    HRESULT (WINAPI *Stop)( DSB * );
    HRESULT (WINAPI *Unlock)( DSB *, LPVOID, DWORD, LPVOID, DWORD );
    HRESULT (WINAPI *Restore)( DSB * );
} DSBVT;
struct DSB { DSBVT *v; };
typedef struct DS DS;
typedef struct {
    HRESULT (WINAPI *QueryInterface)( DS *, REFIID, LPVOID * );
    ULONG   (WINAPI *AddRef)( DS * );
    ULONG   (WINAPI *Release)( DS * );
    HRESULT (WINAPI *CreateSoundBuffer)( DS *, LPVOID, DSB **, LPVOID );
    HRESULT (WINAPI *GetCaps)( DS *, LPVOID );
    HRESULT (WINAPI *DuplicateSoundBuffer)( DS *, DSB *, DSB ** );
    HRESULT (WINAPI *SetCooperativeLevel)( DS *, HWND, DWORD );
} DSVT;
struct DS { DSVT *v; };
typedef struct {
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
    WAVEFORMATEX    *lpwfxFormat;
} DSBDESC;

#define DSBCAPS_PRIMARYBUFFER   0x00000001
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000
#define DSBCAPS_GLOBALFOCUS     0x00008000
#define DSSCL_PRIORITY          2
#define RATE                    22050
#define POLL_MS                 20
#define RUN_MS                  2000

static FILE *out;
static volatile LONG ticks;

static void CALLBACK mm_tick( UINT id, UINT msg, DWORD user, DWORD r1, DWORD r2 )
{
    (void)id; (void)msg; (void)user; (void)r1; (void)r2;
    InterlockedIncrement( (LPLONG)&ticks );
}

static int mm_timer( void )
{
    UINT    id;
    DWORD   t0;
    LONG    at[10];
    int     k;

    t0 = timeGetTime();
    id = timeSetEvent( 20, 10, mm_tick, 0, TIME_PERIODIC );
    fprintf( out, "timeSetEvent id %u\n", id );
    for( k = 0; k < 10; ++k ) {
        Sleep( 200 );
        at[k] = ticks;
    }
    if( id != 0 ) {
        timeKillEvent( id );
    }
    for( k = 0; k < 10; ++k ) {
        fprintf( out, "%5d ms  ticks %ld\n", (k + 1) * 200, at[k] );
    }
    fprintf( out, "elapsed %lu ms, expected about %lu ticks\n",
             (unsigned long)(timeGetTime() - t0), (unsigned long)((timeGetTime() - t0) / 20) );
    fclose( out );
    return( 0 );
}

static void mono22( WAVEFORMATEX *wf )
{
    memset( wf, 0, sizeof( *wf ) );
    wf->wFormatTag = WAVE_FORMAT_PCM;
    wf->nChannels = 1;
    wf->nSamplesPerSec = RATE;
    wf->nAvgBytesPerSec = RATE;
    wf->nBlockAlign = 1;
    wf->wBitsPerSample = 8;
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    HMODULE         m;
    HRESULT (WINAPI *create)( LPVOID, DS **, LPVOID );
    DS              *ds = NULL;
    DSB             *prim = NULL;
    DSB             *b = NULL;
    DSBDESC         d;
    WAVEFORMATEX    wf;
    LPVOID          p1, p2;
    DWORD           n1, n2, i, t0, t, play, write, status, caps[24];
    HWND            wnd;
    char            mode[16] = "", label[64] = "";
    int             loop, setfmt, quiet;
    HRESULT         r;

    (void)inst; (void)prev; (void)show;
    sscanf( cmd, "%15s %63s", mode, label );
    loop = strcmp( mode, "loop" ) == 0;
    setfmt = strcmp( mode, "mono" ) == 0;
    quiet = strcmp( mode, "onceq" ) == 0;
    out = fopen( "C:\\DSTEST.TXT", "a" );
    if( out == NULL ) {
        return( 1 );
    }
    fprintf( out, "== DSTEST %s %s\n", mode, label );
    if( strcmp( mode, "mmtimer" ) == 0 ) {
        return( mm_timer() );
    }
    /* dxdiag plays from its own foreground window; this one stands in for it */
    wnd = CreateWindow( "STATIC", "DSTEST", WS_POPUP | WS_VISIBLE, 0, 0, 160, 40,
                        NULL, NULL, NULL, NULL );
    SetForegroundWindow( wnd );
    m = LoadLibrary( "DSOUND.DLL" );
    create = m ? (HRESULT (WINAPI *)( LPVOID, DS **, LPVOID ))GetProcAddress( m, "DirectSoundCreate" ) : NULL;
    if( create == NULL || (r = create( NULL, &ds, NULL )) != 0 ) {
        fprintf( out, "DirectSoundCreate failed\n" );
        fclose( out );
        return( 1 );
    }
    memset( caps, 0, sizeof( caps ) );
    caps[0] = sizeof( caps );
    ds->v->GetCaps( ds, caps );
    fprintf( out, "caps flags %08lx  (20h = emulation)\n", (unsigned long)caps[1] );
    fprintf( out, "coop %08lx\n", (unsigned long)ds->v->SetCooperativeLevel( ds, wnd, DSSCL_PRIORITY ) );
    if( setfmt ) {
        memset( &d, 0, sizeof( d ) );
        d.dwSize = sizeof( d );
        d.dwFlags = DSBCAPS_PRIMARYBUFFER;
        if( ds->v->CreateSoundBuffer( ds, &d, &prim, NULL ) == 0 ) {
            mono22( &wf );
            fprintf( out, "primary setformat %08lx\n", (unsigned long)prim->v->SetFormat( prim, &wf ) );
        }
    }

    mono22( &wf );
    memset( &d, 0, sizeof( d ) );
    d.dwSize = sizeof( d );
    d.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
    d.dwBufferBytes = RATE;                                     /* 1 s */
    d.lpwfxFormat = &wf;
    r = ds->v->CreateSoundBuffer( ds, &d, &b, NULL );
    fprintf( out, "secondary %08lx\n", (unsigned long)r );
    if( r != 0 || b == NULL ) {
        fclose( out );
        return( 1 );
    }
    if( b->v->Lock( b, 0, RATE, &p1, &n1, &p2, &n2, 0 ) == 0 ) {
        for( i = 0; i < n1; ++i ) {
            ((BYTE *)p1)[i] = (BYTE)(((i * 440 / RATE) & 1) ? 0xA0 : 0x60);
        }
        b->v->Unlock( b, p1, n1, p2, n2 );
    }

    t0 = timeGetTime();
    r = b->v->Play( b, 0, 0, loop ? 1 : 0 );
    fprintf( out, "play %08lx\n", (unsigned long)r );
    if( quiet ) {
        Sleep( RUN_MS );
    }
    for( t = 0; t < RUN_MS; t = timeGetTime() - t0 ) {
        play = write = status = 0;
        b->v->GetCurrentPosition( b, &play, &write );
        b->v->GetStatus( b, &status );
        fprintf( out, "%5lu ms  play %5lu  write %5lu  status %lx\n",
                 (unsigned long)t, (unsigned long)play, (unsigned long)write, (unsigned long)status );
        Sleep( POLL_MS );
    }
    b->v->Stop( b );
    b->v->Release( b );
    if( prim != NULL ) {
        prim->v->Release( prim );
    }
    ds->v->Release( ds );
    DestroyWindow( wnd );
    fclose( out );
    return( 0 );
}
