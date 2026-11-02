/* M8CUBE.EXE - flat-shaded cubes turning in a 320x200 window, every polygon
 * filled by the Mach8 engine through the driver (ExtEscape 0x4D3F, poly3d.c).
 * The CPU transforms, culls back faces, sorts by depth and shades (Lambert,
 * one light); the bus carries each face's corners and one colour, and the
 * engine fills it into a back buffer off screen, then copies the changed box
 * to the window.
 *
 *   M8CUBE [seconds] [objects] [d]
 *     seconds   run that long, then write C:\M8CUBE.TXT (0 = until closed)
 *     objects   cubes, 1..8 (default 1)
 *     d         draw on the screen directly, no back buffer
 *     v         copy the back buffer to the window at vertical blank (no tear)
 * 8 bpp desktop only.
 * Build: wcl386 -q -bt=nt -l=nt_win -ox -fe=M8CUBE.EXE M8CUBE.C
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define cosf( x )   ((float)cos( x ))
#define sinf( x )   ((float)sin( x ))

#define ESC_POLY    0x4D3F
#define W           320
#define H           200
#define SHADES      32
#define HUES        3
#define MAX_OBJ     8
#define MAX_FACES   (MAX_OBJ * 6)

static HWND         wnd;
static HPALETTE     pal;
static BYTE         shade_index[HUES][SHADES];
static int          objects = 1;
static int          direct;
static int          vblank;
static int          prev_l, prev_t, prev_r = W, prev_b = H;
static WORD         buf[16 + MAX_FACES * 12];

static const float  cube_v[8][3] = {
    { -1, -1, -1 }, { 1, -1, -1 }, { 1, 1, -1 }, { -1, 1, -1 },
    { -1, -1, 1 }, { 1, -1, 1 }, { 1, 1, 1 }, { -1, 1, 1 }
};
/* Corners in one winding seen from outside, and the outward normal: a face
 * whose screen area is negative faces the viewer (y grows downwards). */
static const int    cube_f[6][4] = {
    { 0, 3, 2, 1 }, { 4, 5, 6, 7 }, { 0, 1, 5, 4 }, { 2, 3, 7, 6 }, { 1, 2, 6, 5 }, { 0, 4, 7, 3 }
};
static const float  cube_n[6][3] = {
    { 0, 0, -1 }, { 0, 0, 1 }, { 0, -1, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { -1, 0, 0 }
};

typedef struct {
    float   z;
    int     colour;
    short   x[4];
    short   y[4];
} FACE;

static FACE         faces[MAX_FACES];

/* Three ramps, dark to light: the indices the system palette gave them. */
static void make_palette( void )
{
    static const int hue[HUES][3] = { { 255, 140, 40 }, { 60, 160, 255 }, { 120, 255, 90 } };
    struct {
        WORD            ver;
        WORD            n;
        PALETTEENTRY    e[HUES * SHADES];
    } lp;
    int i;
    int k;

    lp.ver = 0x300;
    lp.n = HUES * SHADES;
    for( i = 0; i < HUES; ++i ) {
        for( k = 0; k < SHADES; ++k ) {
            PALETTEENTRY *e = &lp.e[i * SHADES + k];
            int          s = 24 + k * (256 - 24) / SHADES;

            e->peRed = (BYTE)(hue[i][0] * s / 256);
            e->peGreen = (BYTE)(hue[i][1] * s / 256);
            e->peBlue = (BYTE)(hue[i][2] * s / 256);
            e->peFlags = PC_NOCOLLAPSE;
        }
    }
    pal = CreatePalette( (LOGPALETTE *)&lp );
}

static void map_palette( HDC dc )
{
    PALETTEENTRY    sys[256];
    PALETTEENTRY    want;
    int             i;
    int             k;
    int             j;
    long            best;
    long            d;

    SelectPalette( dc, pal, FALSE );
    RealizePalette( dc );
    GetSystemPaletteEntries( dc, 0, 256, sys );
    for( i = 0; i < HUES; ++i ) {
        for( k = 0; k < SHADES; ++k ) {
            GetPaletteEntries( pal, i * SHADES + k, 1, &want );
            best = 0x7FFFFFFFL;
            for( j = 0; j < 256; ++j ) {
                d = labs( (long)sys[j].peRed - want.peRed ) + labs( (long)sys[j].peGreen -
                    want.peGreen ) + labs( (long)sys[j].peBlue - want.peBlue );
                if( d < best ) {
                    best = d;
                    shade_index[i][k] = (BYTE)j;
                }
            }
        }
    }
}

static int cmp_face( const void *a, const void *b )
{
    float d = ((const FACE *)b)->z - ((const FACE *)a)->z;

    return( d > 0 ? 1 : d < 0 ? -1 : 0 );
}

static short clampi( float v, int hi )
{
    int i = (int)(v + 0.5f);

    return( (short)(i < 0 ? 0 : i > hi ? hi : i) );
}

/* One frame: n faces to draw, *l..*b their box. */
static int build( float t, int *bl, int *bt, int *br, int *bb )
{
    static const float  light[3] = { -0.40f, -0.55f, -0.73f };
    int                 nf = 0;
    int                 o;
    int                 i;
    int                 k;

    *bl = W;
    *bt = H;
    *br = 0;
    *bb = 0;
    for( o = 0; o < objects; ++o ) {
        float   a = t * (0.9f + 0.13f * o) + o;
        float   b = t * (0.6f + 0.07f * o) + 2 * o;
        float   ca = cosf( a ), sa = sinf( a ), cb = cosf( b ), sb = sinf( b );
        float   cx = objects == 1 ? 0.0f : 2.4f * cosf( 6.2832f * o / objects + t * 0.3f );
        float   cy = objects == 1 ? 0.0f : 1.4f * sinf( 6.2832f * o / objects + t * 0.3f );
        float   cz = objects == 1 ? 5.0f : 8.0f;
        float   px[8], py[8], pz[8];

        for( i = 0; i < 8; ++i ) {
            /* Y then X rotation. */
            float x = cube_v[i][0] * ca + cube_v[i][2] * sa;
            float z = -cube_v[i][0] * sa + cube_v[i][2] * ca;
            float y = cube_v[i][1] * cb - z * sb;

            z = cube_v[i][1] * sb + z * cb;
            px[i] = x + cx;
            py[i] = y + cy;
            pz[i] = z + cz;
        }
        for( k = 0; k < 6; ++k ) {
            const int   *f = cube_f[k];
            float       nx = cube_n[k][0] * ca + cube_n[k][2] * sa;
            float       nz = -cube_n[k][0] * sa + cube_n[k][2] * ca;
            float       ny = cube_n[k][1] * cb - nz * sb;
            float       sx[4], sy[4], area = 0, li;
            int         s;

            nz = cube_n[k][1] * sb + nz * cb;
            for( i = 0; i < 4; ++i ) {
                sx[i] = W / 2 + 150.0f * px[f[i]] / pz[f[i]];
                sy[i] = H / 2 + 150.0f * py[f[i]] / pz[f[i]];
            }
            for( i = 0; i < 4; ++i ) {
                area += sx[i] * sy[(i + 1) & 3] - sx[(i + 1) & 3] * sy[i];
            }
            if( area >= 0 ) {
                continue;
            }
            li = -(nx * light[0] + ny * light[1] + nz * light[2]);
            s = (int)((li < 0 ? 0 : li) * (SHADES - 8)) + 7;
            faces[nf].colour = shade_index[o % HUES][s > SHADES - 1 ? SHADES - 1 : s];
            faces[nf].z = pz[f[0]] + pz[f[1]] + pz[f[2]] + pz[f[3]];
            for( i = 0; i < 4; ++i ) {
                faces[nf].x[i] = clampi( sx[i], W - 1 );
                faces[nf].y[i] = clampi( sy[i], H - 1 );
                if( faces[nf].x[i] < *bl ) *bl = faces[nf].x[i];
                if( faces[nf].x[i] + 1 > *br ) *br = faces[nf].x[i] + 1;
                if( faces[nf].y[i] < *bt ) *bt = faces[nf].y[i];
                if( faces[nf].y[i] + 1 > *bb ) *bb = faces[nf].y[i] + 1;
            }
            ++nf;
        }
    }
    qsort( faces, nf, sizeof( FACE ), cmp_face );
    return( nf );
}

static int frame( HDC dc, float t, int *drawn )
{
    POINT   org = { 0, 0 };
    int     l, tp, r, b;
    int     n = build( t, &l, &tp, &r, &b );
    WORD    *p = buf + 14;
    int     i;
    int     k;

    ClientToScreen( wnd, &org );
    buf[0] = 2;
    buf[1] = (direct ? 1 : 0) | (vblank ? 2 : 0);
    buf[2] = (WORD)org.x;
    buf[3] = (WORD)org.y;
    buf[4] = 0;                                     /* background: black */
    /* Clear what the last frame drew; show that and this frame's box. */
    buf[5] = (WORD)prev_l;
    buf[6] = (WORD)prev_t;
    buf[7] = (WORD)prev_r;
    buf[8] = (WORD)prev_b;
    buf[9] = (WORD)min( prev_l, l );
    buf[10] = (WORD)min( prev_t, tp );
    buf[11] = (WORD)max( prev_r, r );
    buf[12] = (WORD)max( prev_b, b );
    buf[13] = (WORD)n;
    for( i = 0; i < n; ++i ) {
        *p++ = (WORD)faces[i].colour;
        *p++ = 4;
        for( k = 0; k < 4; ++k ) {
            *p++ = (WORD)faces[i].x[k];
            *p++ = (WORD)faces[i].y[k];
        }
    }
    if( n == 0 ) {
        l = tp = r = b = 0;
    }
    prev_l = l;
    prev_t = tp;
    prev_r = r;
    prev_b = b;
    *drawn += n;
    return( ExtEscape( dc, ESC_POLY, (int)((BYTE *)p - (BYTE *)buf), (LPCSTR)buf, 0, NULL ) );
}

static LRESULT CALLBACK wndproc( HWND h, UINT msg, WPARAM wp, LPARAM lp )
{
    HDC dc;

    switch( msg ) {
    case WM_QUERYNEWPALETTE:
    case WM_PALETTECHANGED:
        if( msg == WM_PALETTECHANGED && (HWND)wp == h ) {
            break;
        }
        dc = GetDC( h );
        map_palette( dc );
        ReleaseDC( h, dc );
        return( TRUE );
    case WM_DESTROY:
        PostQuitMessage( 0 );
        return( 0 );
    }
    return( DefWindowProc( h, msg, wp, lp ) );
}

int PASCAL WinMain( HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show )
{
    WNDCLASS    wc;
    RECT        rc = { 0, 0, W, H };
    HDC         dc;
    MSG         m;
    WORD        in[3] = { 1, W, H };
    WORD        top = 0;
    DWORD       t0;
    DWORD       now;
    DWORD       last;
    long        frames = 0;
    long        frames_s = 0;
    int         drawn = 0;
    int         secs = 0;
    int         ok = 1;
    char        *a;
    char        title[80];
    FILE        *f;

    secs = atoi( cmd );
    a = strchr( cmd, ' ' );
    if( a != NULL ) {
        objects = atoi( a + 1 );
    }
    if( objects < 1 ) {
        objects = 1;
    }
    if( objects > MAX_OBJ ) {
        objects = MAX_OBJ;
    }
    direct = strstr( cmd, " d" ) != NULL;
    vblank = strstr( cmd, " v" ) != NULL;
    memset( &wc, 0, sizeof( wc ) );
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor( NULL, IDC_ARROW );
    wc.hbrBackground = GetStockObject( BLACK_BRUSH );
    wc.lpszClassName = "M8CUBE";
    RegisterClass( &wc );
    AdjustWindowRect( &rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE );
    wnd = CreateWindow( "M8CUBE", "M8CUBE", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, 40, 40,
                        rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, inst, NULL );
    ShowWindow( wnd, SW_SHOW );
    UpdateWindow( wnd );
    make_palette();
    dc = GetDC( wnd );
    map_palette( dc );
    if( ExtEscape( dc, ESC_POLY, sizeof( in ), (LPCSTR)in, sizeof( top ), (LPSTR)&top ) <= 0 ) {
        MessageBox( wnd, "The driver refused (not M8DISP at 8 bpp, or no off-screen rows).",
                    "M8CUBE", MB_OK );
        return( 1 );
    }
    t0 = last = GetTickCount();
    for( ;; ) {
        while( PeekMessage( &m, NULL, 0, 0, PM_REMOVE ) ) {
            if( m.message == WM_QUIT ) {
                goto done;
            }
            TranslateMessage( &m );
            DispatchMessage( &m );
        }
        now = GetTickCount();
        if( frame( dc, (now - t0) / 1000.0f, &drawn ) <= 0 ) {
            ok = 0;
            break;
        }
        ++frames;
        ++frames_s;
        if( now - last >= 1000 ) {
            sprintf( title, "M8CUBE  %ld fps  %d cubes%s", frames_s * 1000 / (now - last), objects,
                     direct ? "  direct" : "" );
            SetWindowText( wnd, title );
            frames_s = 0;
            last = now;
        }
        if( secs > 0 && now - t0 >= (DWORD)secs * 1000 ) {
            break;
        }
    }
done:
    now = GetTickCount();
    in[0] = 3;
    ExtEscape( dc, ESC_POLY, 2, (LPCSTR)in, 0, NULL );
    ReleaseDC( wnd, dc );
    if( secs > 0 ) {
        f = fopen( "C:\\M8CUBE.TXT", "a" );
        if( f != NULL ) {
            double s = (now - t0) / 1000.0;

            fprintf( f, "M8CUBE  %s  cubes %d  frames %ld  %.2f s  %.1f fps  %.0f faces/s  row %u%s\n",
                     direct ? "direct" : vblank ? "vblank" : "buffer", objects, frames, s, frames / s, drawn / s, top,
                     ok ? "" : "  REFUSED" );
            fclose( f );
        }
        DestroyWindow( wnd );
    }
    return( ok ? 0 : 1 );
}
