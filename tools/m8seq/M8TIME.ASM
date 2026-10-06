; M8TIME.COM - time the candidate drawing techniques for the Mach8 driver on the real 5160.
;
; docs/mach8_driver_design.md: for each drawing job the cheapest technique on this bus wins. Each
; test below runs N operations with interrupts off, timed by PIT channel 0 (0.838 us per tick),
; including a FIFO-room check before each operation (as a driver must) and a wait for the engine
; to go idle at the end. Writes M8TIME.BIN: "M8T3" (M8T1/2 sent the 1-bit tests down the 8514/A path, which never completed), then per test: ticks (word), N (word).
;
;   0 text A  glyph cache: 8x16 glyph expanded from one plane of spare memory (XFree86),
;             per glyph CUR_X, DESTX, DESTY, CMD = 4 writes                       N = 64
;   1 text B  per-character mono expansion from the CPU, Mach8 DP_CONFIG 3251 (ATI),
;             per glyph 5 registers + 8 data words = 13 writes                     N = 64
;   2 text C  one expansion per string (DDK-style), same path, 8 glyphs wide,
;             5 registers + 64 data words, run N times (= 8N glyphs)               N = 8
;   3 fill    64x16 rectangle fill, CUR_X, CUR_Y, MAJ, MIN, CMD = 5 writes        N = 64
;   4 blit    64x16 screen-to-screen copy, 7 writes                              N = 64
;   5 raw     64x16 pixels uploaded from the CPU, 16-bit words, 5 + 512 writes    N = 4
;   6 vga     1024 bytes written to VGA text memory B800:2000 (an unseen page),
;             rep stosw                                                          N = 4
; All drawing goes to rows 600-799 of the 8514 side's memory, below the 800x600 screen.
; Run from a DOS boot; engine registers only.

        org     100h

        jmp     start

oname   db      'M8TIME.BIN', 0
m_ok    db      'M8TIME: done, results in M8TIME.BIN', 13, 10, '$'
m_out   db      'M8TIME: cannot write M8TIME.BIN', 13, 10, '$'
hdr     db      'M8T3'
res     times 14 dw 0
t0      dw      0
nleft   dw      0
xpos    dw      0
data    times 512 dw 5A3Ch            ; source words for the CPU-data tests

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

%macro  TSTART 0
        cli
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        mov     [t0], ax
%endmacro

; ends the timing and stores ticks and N into res[%1]
%macro  TEND 2
        call    idle
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        sti
        mov     bx, [t0]
        sub     bx, ax                  ; PIT counts down
        mov     [res + %1 * 4], bx
        mov     word [res + %1 * 4 + 2], %2
%endmacro

start:
        cld
        call    init

        ; ---- one glyph in plane 5 of spare memory at (0,700), 8x16, for test 0 ----
        call    idle
        OUTW    0AAE8h, 20h             ; WRT_MASK: plane 5 only
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0FFh
        OUTW    86E8h, 0
        OUTW    82E8h, 700
        OUTW    96E8h, 7
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0AAE8h, 0FFh

        ; ---- test 0: glyph cache, plane-select expansion blit ----
        OUTW    0BEE8h, 0A0C0h          ; PIX_CNTL: the plane selected by RD_MASK picks the mix
        OUTW    0AEE8h, 40h             ; RD_MASK: plane 5 (bit 6, rotated)
        OUTW    0BAE8h, 27h             ; foreground: colour
        OUTW    0B6E8h, 03h             ; background: leave the destination
        OUTW    0A6E8h, 0Fh
        OUTW    96E8h, 7
        OUTW    0BEE8h, 15
        OUTW    82E8h, 700              ; source row, set once per font block
        mov     word [xpos], 0
        mov     word [nleft], 64
        TSTART
.t0:    call    fifo
        OUTW    86E8h, 0                ; source X
        mov     dx, 8EE8h               ; DESTX
        mov     ax, [xpos]
        out     dx, ax
        OUTW    8AE8h, 600              ; DESTY
        OUTW    9AE8h, 0C0B3h           ; blit
        add     word [xpos], 8
        dec     word [nleft]
        jnz     .t0
        TEND    0, 64
        call    plain

        ; ---- test 1: per-character expansion from the CPU, ATI's way ----
        ; Mach8 extended path as ATIM8.DRV's ExtTextOut: DP_CONFIG 3251 (foreground colour where
        ; the CPU bit is 1, data packed), per glyph CUR_X, DEST_X_START, DEST_X_END, CUR_Y,
        ; DEST_Y_END (starts it) and the glyph's 128 bits as 8 words.
        OUTW    0BAE8h, 27h
        OUTW    0B6E8h, 03h
        OUTW    0CEEEh, 3251h
        mov     word [xpos], 0
        mov     word [nleft], 64
        TSTART
.t1:    call    fifo
        mov     dx, 86E8h
        mov     ax, [xpos]
        out     dx, ax
        mov     dx, 0A6EEh              ; DEST_X_START
        out     dx, ax
        add     ax, 8
        mov     dx, 0AAEEh              ; DEST_X_END
        out     dx, ax
        OUTW    82E8h, 620
        OUTW    0AEEEh, 620 + 16        ; DEST_Y_END
        mov     dx, 0E2E8h
        mov     si, data
        mov     cx, 8
        rep     outsw
        add     word [xpos], 8
        dec     word [nleft]
        jnz     .t1
        TEND    1, 64

        ; ---- test 2: one expansion per 8-glyph string (64 x 16), same path ----
        mov     word [nleft], 8
        TSTART
.t2:    call    fifo
        OUTW    86E8h, 0
        OUTW    0A6EEh, 0
        OUTW    0AAEEh, 64
        OUTW    82E8h, 640
        OUTW    0AEEEh, 640 + 16
        mov     dx, 0E2E8h
        mov     si, data
        mov     cx, 64
        rep     outsw
        dec     word [nleft]
        jnz     .t2
        TEND    2, 8
        OUTW    0CEEEh, 0               ; back to the 8514/A data path for the rest
        OUTW    0BAE8h, 27h
        call    plain

        ; ---- test 3: 64x16 fills ----
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 33h
        mov     word [xpos], 0
        mov     word [nleft], 64
        TSTART
.t3:    call    fifo
        mov     dx, 86E8h
        mov     ax, [xpos]
        out     dx, ax
        OUTW    82E8h, 660
        OUTW    96E8h, 63
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 40F3h
        add     word [xpos], 8
        dec     word [nleft]
        jnz     .t3
        TEND    3, 64

        ; ---- test 4: 64x16 blits ----
        OUTW    0BAE8h, 67h             ; foreground: video memory
        mov     word [xpos], 0
        mov     word [nleft], 64
        TSTART
.t4:    call    fifo
        OUTW    86E8h, 0
        OUTW    82E8h, 660
        mov     dx, 8EE8h
        mov     ax, [xpos]
        out     dx, ax
        OUTW    8AE8h, 680
        OUTW    96E8h, 63
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 0C0F3h
        add     word [xpos], 8
        dec     word [nleft]
        jnz     .t4
        TEND    4, 64
        OUTW    0BAE8h, 27h

        ; ---- test 5: raw 64x16 upload, colour data from the CPU ----
        OUTW    0BAE8h, 47h             ; foreground: CPU data
        mov     word [nleft], 4
        TSTART
.t5:    call    fifo
        OUTW    86E8h, 0
        OUTW    82E8h, 700
        OUTW    96E8h, 63
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 53B1h
        mov     dx, 0E2E8h
        mov     si, data
        mov     cx, 512
        rep     outsw
        dec     word [nleft]
        jnz     .t5
        TEND    5, 4
        OUTW    0BAE8h, 27h

        ; ---- test 6: VGA text memory writes ----
        push    es
        mov     ax, 0B800h
        mov     es, ax
        mov     word [nleft], 4
        TSTART
.t6:    mov     di, 2000h
        mov     ax, 0720h
        mov     cx, 512
        rep     stosw
        dec     word [nleft]
        jnz     .t6
        TEND    6, 4
        pop     es

        ; ---- write M8TIME.BIN ----
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 28
        mov     dx, hdr
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

init:
        OUTW    42E8h, 9000h
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0EEEEh, 0
        call    plain
        ret

plain:  call    idle
        OUTW    0BEE8h, 0A000h          ; PIX_CNTL: foreground mix always
        OUTW    0AEE8h, 0FFh
        OUTW    0AAE8h, 0FFh
        OUTW    0B6E8h, 07h
        ret

; FIFO room: EXT_FIFO_STATUS bit 4 clear, as TEST.COM's runner checks.
fifo:   push    cx
        mov     dx, 9AEEh
        mov     cx, 2000
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
.qok:   pop     cx
        ret

idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     cx
        ret
