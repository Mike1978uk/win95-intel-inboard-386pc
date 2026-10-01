/*
 * LZ4BENCH - time LZ4 on real swap pages, on the machine that will run it.
 *
 *   LZ4BENCH [MHz]        reads SWAPPAGE.BIN (4 KB pages), writes LZ4BENCH.TXT,
 *                         both in the current directory
 *
 * Stage 2 of swap compression (docs/magnaram_and_swap_compression_2026_10_01.md).
 * The case for compressing swap rests on the CPU costs, which so far are
 * cycles-per-byte guesses. This measures them: plain LZ4 compression and
 * decompression, LZ4 with a short match search (the "HC" trade), and a plain
 * copy as the floor.
 *
 * Every page is round-tripped and compared before anything is timed; a
 * compressor that loses data must not produce a number. Timing is the best of
 * five passes with QueryPerformanceCounter (the PIT on Windows 95), at
 * time-critical thread priority, so interrupts inflate a pass rather than the
 * result.
 *
 * Win32 console, 32-bit flat like the VxD will be. Built without a C runtime
 * (build.sh).
 */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned long u32;
typedef void *HANDLE;
typedef struct { u32 lo; long hi; } LARGE_INTEGER;

#define WINAPI __attribute__((dllimport, stdcall))

WINAPI void   ExitProcess(unsigned);
WINAPI HANDLE GetStdHandle(u32);
WINAPI int    WriteFile(HANDLE, const void *, u32, u32 *, void *);
WINAPI int    ReadFile(HANDLE, void *, u32, u32 *, void *);
WINAPI HANDLE CreateFileA(const char *, u32, u32, void *, u32, u32, HANDLE);
WINAPI int    CloseHandle(HANDLE);
WINAPI char  *GetCommandLineA(void);
WINAPI int    QueryPerformanceCounter(LARGE_INTEGER *);
WINAPI int    QueryPerformanceFrequency(LARGE_INTEGER *);
WINAPI HANDLE GetCurrentThread(void);
WINAPI int    SetThreadPriority(HANDLE, int);
WINAPI int    MulDiv(int, int, int);
int __attribute__((cdecl)) wsprintfA(char *, const char *, ...);

#define PAGE      4096
#define MAXPAGES  64
#define HASH_LOG  12
#define HASH_SIZE (1 << HASH_LOG)

static u8  pages[MAXPAGES * PAGE];
static u8  comp[PAGE + PAGE / 255 + 16];
static u8  back[PAGE];
static u16 htab[HASH_SIZE];          /* position + 1 of the last 4-byte group */
static u16 chain[PAGE];              /* previous position + 1, same hash */
static HANDLE out_file;
static char line[160];

static void say(const char *s)
{
    u32 n = 0, w;
    while (s[n])
        n++;
    WriteFile(GetStdHandle((u32)-11), s, n, &w, 0);
    if (out_file)
        WriteFile(out_file, s, n, &w, 0);
}

static u32 rd32(const u8 *p)
{
    return *(const u32 *)p;
}

static u32 hash4(const u8 *p)
{
    return (rd32(p) * 2654435761u) >> (32 - HASH_LOG);
}

/* Forward copy. Correct for overlapping matches when the source is at least
 * 4 bytes behind, which is the only way it is used with movsd. */
static void copy(u8 *d, const u8 *s, u32 n)
{
    u32 dw = n >> 2;
    __asm__ volatile("cld; rep movsl" : "+D"(d), "+S"(s), "+c"(dw) : : "memory");
    n &= 3;
    __asm__ volatile("rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
}

static u8 *put_len(u8 *op, u32 n)
{
    while (n >= 255) {
        *op++ = 255;
        n -= 255;
    }
    *op++ = (u8)n;
    return op;
}

/*
 * LZ4 block format. depth 0 = LZ4 fast (one hash candidate); depth N = follow
 * the hash chain N candidates and keep the longest match.
 */
static u32 lz4_compress(const u8 *src, u8 *dst, u32 depth)
{
    const u8 *ip = src + 1, *anchor = src;
    const u8 *iend = src + PAGE, *mflimit = iend - 12, *matchlimit = iend - 5;
    u8 *op = dst;
    u32 i, h;

    for (i = 0; i < HASH_SIZE; i++)
        htab[i] = 0;
    htab[hash4(src)] = 1;
    if (depth)
        chain[0] = 0;

    while (ip < mflimit) {
        const u8 *m = 0;
        u32 best = 0, cand, pos = (u32)(ip - src);

        h = hash4(ip);
        cand = htab[h];
        if (depth)
            chain[pos] = (u16)cand;
        htab[h] = (u16)(pos + 1);

        for (i = 0; cand && i <= depth; i++) {
            const u8 *r = src + cand - 1;
            if (rd32(r) == rd32(ip)) {
                u32 l = 4;
                while (ip + l < matchlimit && r[l] == ip[l])
                    l++;
                if (l > best) {
                    best = l;
                    m = r;
                }
            }
            if (!depth)
                break;
            cand = chain[cand - 1];
        }
        if (!m) {
            ip++;
            continue;
        }

        while (ip > anchor && m > src && ip[-1] == m[-1]) {
            ip--;
            m--;
            best++;
        }
        {
            u32 lit = (u32)(ip - anchor), ml = best - 4, off = (u32)(ip - m);
            u8 *token = op++;
            *token = (u8)((lit >= 15 ? 15 : lit) << 4);
            if (lit >= 15)
                op = put_len(op, lit - 15);
            copy(op, anchor, lit);
            op += lit;
            *op++ = (u8)off;
            *op++ = (u8)(off >> 8);
            *token |= (u8)(ml >= 15 ? 15 : ml);
            if (ml >= 15)
                op = put_len(op, ml - 15);
        }
        if (depth) {
            /* Keep the chain complete across the match, or HC loses candidates. */
            const u8 *q;
            for (q = ip + 1; q < ip + best && q < mflimit; q++) {
                u32 p = (u32)(q - src), hq = hash4(q);
                chain[p] = htab[hq];
                htab[hq] = (u16)(p + 1);
            }
        }
        ip += best;
        anchor = ip;
    }

    {
        u32 lit = (u32)(iend - anchor);
        u8 *token = op++;
        *token = (u8)((lit >= 15 ? 15 : lit) << 4);
        if (lit >= 15)
            op = put_len(op, lit - 15);
        copy(op, anchor, lit);
        op += lit;
    }
    return (u32)(op - dst);
}

/* Returns the decompressed size, or 0 on any malformed input. */
static u32 lz4_decompress(const u8 *src, u32 csize, u8 *dst)
{
    const u8 *ip = src, *iend = src + csize;
    u8 *op = dst, *oend = dst + PAGE;

    for (;;) {
        u32 token = *ip++, len = token >> 4, s, off;
        const u8 *m;

        if (len == 15)
            do {
                s = *ip++;
                len += s;
            } while (s == 255 && ip < iend);
        if (len > (u32)(oend - op) || len > (u32)(iend - ip))
            return 0;
        copy(op, ip, len);
        ip += len;
        op += len;
        if (ip >= iend)
            break;

        off = ip[0] | (ip[1] << 8);
        ip += 2;
        if (off == 0 || off > (u32)(op - dst))
            return 0;
        m = op - off;
        len = token & 15;
        if (len == 15)
            do {
                s = *ip++;
                len += s;
            } while (s == 255 && ip < iend);
        len += 4;
        if (len > (u32)(oend - op))
            return 0;
        if (off >= 4) {
            copy(op, m, len);
            op += len;
        } else {
            while (len--)
                *op++ = *m++;
        }
    }
    return (u32)(op - dst);
}

static u32 now(void)
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.lo;
}

/* hundredths, as "123.45" */
static void fmt2(char *b, u32 v)
{
    wsprintfA(b, "%lu.%02lu", v / 100, v % 100);
}

void start(void)
{
    char *cl = GetCommandLineA();
    u32 mhz10 = 835, npages, got, i, pass, k;
    u32 fails = 0;
    HANDLE f;
    LARGE_INTEGER fq;
    static const char *names[] = { "copy (floor)", "LZ4 compress", "LZ4 decompress",
                                   "LZ4 search 4 compress", "LZ4 search 16 compress" };

    /* Optional MHz argument, e.g. 83.5 */
    if (*cl == '"') {
        for (cl++; *cl && *cl != '"'; cl++)
            ;
        if (*cl)
            cl++;
    } else
        while (*cl && *cl != ' ')
            cl++;
    while (*cl == ' ')
        cl++;
    if (*cl >= '0' && *cl <= '9') {
        mhz10 = 0;
        while (*cl >= '0' && *cl <= '9')
            mhz10 = mhz10 * 10 + (u32)(*cl++ - '0');
        mhz10 *= 10;
        if (*cl == '.' && cl[1] >= '0' && cl[1] <= '9')
            mhz10 += (u32)(cl[1] - '0');
    }

    out_file = CreateFileA("LZ4BENCH.TXT", 0x40000000, 1, 0, 2 /* CREATE_ALWAYS */, 0x80, 0);
    if (out_file == (HANDLE)-1)
        out_file = 0;

    f = CreateFileA("SWAPPAGE.BIN", 0x80000000, 1, 0, 3 /* OPEN_EXISTING */, 0x80, 0);
    if (f == (HANDLE)-1) {
        say("cannot open SWAPPAGE.BIN\r\n");
        ExitProcess(2);
    }
    ReadFile(f, pages, sizeof pages, &got, 0);
    CloseHandle(f);
    npages = got / PAGE;
    if (!npages) {
        say("SWAPPAGE.BIN is empty\r\n");
        ExitProcess(2);
    }

    /* Self-test: every page, every compressor, round-tripped and compared. */
    for (k = 0; k < 3; k++) {
        static const u32 depths[] = { 0, 4, 16 };
        u32 sum = 0;
        for (i = 0; i < npages; i++) {
            const u8 *p = pages + i * PAGE;
            u32 c = lz4_compress(p, comp, depths[k]), d = lz4_decompress(comp, c, back), j;
            if (d != PAGE)
                fails++;
            else
                for (j = 0; j < PAGE; j++)
                    if (back[j] != p[j]) {
                        fails++;
                        break;
                    }
            sum += c;
        }
        wsprintfA(line, "%s: %lu pages, ratio %lu.%02lu:1\r\n", k == 0 ? "LZ4" : k == 1 ? "LZ4 search 4" : "LZ4 search 16",
                  npages, npages * PAGE / sum, (npages * PAGE * 100 / sum) % 100);
        say(line);
    }
    if (fails) {
        wsprintfA(line, "SELF-TEST FAILED: %lu round trips wrong. No timings.\r\n", fails);
        say(line);
        ExitProcess(1);
    }
    /* The check must be able to fail: corrupt one literal and it has to notice. */
    {
        u32 c = lz4_compress(pages, comp, 0), d, j, same = 1;
        comp[1] ^= 0x55;
        d = lz4_decompress(comp, c, back);
        if (d == PAGE)
            for (j = 0; j < PAGE; j++)
                if (back[j] != pages[j])
                    same = 0;
        if (d == PAGE && same) {
            say("SELF-TEST BROKEN: a corrupted page compared equal. No timings.\r\n");
            ExitProcess(1);
        }
    }
    say("self-test passed: every page round-trips byte-exact, and corruption is caught\r\n");

    QueryPerformanceFrequency(&fq);
    SetThreadPriority(GetCurrentThread(), 15 /* TIME_CRITICAL */);
    wsprintfA(line, "timer %lu Hz, cycles assume %lu.%lu MHz\r\n\r\n", fq.lo, mhz10 / 10, mhz10 % 10);
    say(line);
    say("                         us/page   cycles/byte\r\n");

    for (k = 0; k < 5; k++) {
        u32 best = 0xFFFFFFFF, us, us100, cpb100;
        char a[24], b[24];
        for (pass = 0; pass < 5; pass++) {
            u32 t = 0;
            for (i = 0; i < npages; i++) {
                const u8 *p = pages + i * PAGE;
                u32 t0;
                if (k == 2) {
                    /* Only the decompression is timed; its input is made first. */
                    u32 c = lz4_compress(p, comp, 0);
                    t0 = now();
                    lz4_decompress(comp, c, back);
                } else {
                    t0 = now();
                    if (k == 0)
                        copy(back, p, PAGE);
                    else
                        lz4_compress(p, comp, k == 1 ? 0 : k == 3 ? 4 : 16);
                }
                t += now() - t0;
            }
            if (t < best)
                best = t;
        }
        us = (u32)MulDiv((int)best, 1000000, (int)fq.lo);         /* all pages, us */
        us100 = (u32)MulDiv((int)us, 100, (int)npages);           /* per page, 1/100 us */
        cpb100 = (u32)MulDiv((int)us100, (int)mhz10, 10 * PAGE);  /* 1/100 cycle per byte */
        fmt2(a, us100);
        fmt2(b, cpb100);
        wsprintfA(line, "%-24s %8s  %10s\r\n", names[k], a, b);
        say(line);
    }
    if (out_file)
        CloseHandle(out_file);
    ExitProcess(0);
}
