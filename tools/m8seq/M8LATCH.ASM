; M8LATCH.COM - what a VGA-half latch copy costs against the CPU moving the same pixels.
;
; A latch copy reads one address and writes one address, and the four planes behind them move
; inside the card: four pixels for two bus cycles. Unchained mode 13h, 4000 addresses (16000
; pixels) a test, so each timed block stays inside one PIT wrap. Channel 0 runs in mode 2 for the
; timed parts, so one count is one 0.838 us clock; mode 3 would count by twos.
;   fill   map mask F, rep stosb: 16000 pixels by 4000 writes
;   latch  write mode 1, rep movsb: 16000 pixels by 4000 reads and 4000 writes
;   plane  each plane by rep movsb, read map and map mask set: 16000 pixels by 16000 + 16000
;   ram    each plane from RAM by rep movsb: 16000 pixels by 16000 writes
; Each plane holds its own pattern. The destination is compared before the latch copy, which
; must mismatch (a test that cannot fail proves nothing), and after, which must not.
; Writes M8LATCH.BIN: "M8LA", then words: mode, fill, latch, plane x4, ram x4, mismatches before,
; mismatches after. DOS boot.
;
;   nasm -f bin -o M8LATCH.COM M8LATCH.ASM

        org     100h

NA      equ     4000
SRC     equ     0
DST     equ     4000h

        jmp     start

oname   db      'M8LATCH.BIN', 0
m_ok    db      'M8LATCH: done', 13, 10, '$'
m_out   db      'M8LATCH: cannot write M8LATCH.BIN', 13, 10, '$'
obuf    db      'M8LA'
r_mode  dw      0
r_fill  dw      0
r_latch dw      0
r_plane times 4 dw 0
r_ram   times 4 dw 0
r_m0    dw      0
r_m1    dw      0
OUTLEN  equ     $ - obuf
t0      dw      0
rbuf    times NA db 0

%macro  TSTART 0
        cli
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        mov     [cs:t0], ax
%endmacro

; BX = counts elapsed
%macro  TEND 0
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        sti
        mov     bx, [cs:t0]
        sub     bx, ax
%endmacro

; AH = map mask
mapm:   push    dx
        mov     dx, 3C4h
        mov     al, 2
        out     dx, ax
        pop     dx
        ret

; AH = plane to read
rmap:   push    dx
        mov     dx, 3CEh
        mov     al, 4
        out     dx, ax
        pop     dx
        ret

; AH = GR5
gr5:    push    dx
        mov     dx, 3CEh
        mov     al, 5
        out     dx, ax
        pop     dx
        ret

; BL = plane -> DL = its pattern key
patkey: mov     al, bl
        mov     cl, 6
        shl     al, cl
        xor     al, 15h
        mov     dl, al
        ret

; each plane at SRC holds (address low byte) xor its key
fillsrc:
        mov     bl, 0
.p:     mov     ah, 1
        mov     cl, bl
        shl     ah, cl
        call    mapm
        call    patkey
        mov     di, SRC
        mov     cx, NA
        xor     si, si
.b:     mov     ax, si
        xor     al, dl
        stosb
        inc     si
        loop    .b
        inc     bl
        cmp     bl, 4
        jb      .p
        mov     ah, 0Fh
        call    mapm
        ret

; AX = addresses x planes at DST differing from the pattern
cmpdst:
        xor     bp, bp
        mov     bl, 0
.p:     mov     ah, bl
        call    rmap
        call    patkey
        mov     si, DST
        mov     cx, NA
        xor     di, di
.b:     mov     ax, di
        xor     al, dl
        cmp     al, [es:si]
        je      .same
        inc     bp
.same:  inc     si
        inc     di
        loop    .b
        inc     bl
        cmp     bl, 4
        jb      .p
        mov     ax, bp
        ret

start:
        cld
        mov     ax, 0013h
        int     10h
        mov     ah, 0Fh
        int     10h
        xor     ah, ah
        mov     [r_mode], ax

        mov     dx, 3C4h                ; unchained, all 256 KB addressable by plane
        mov     ax, 0604h
        out     dx, ax
        mov     dx, 3D4h
        mov     ax, 0014h
        out     dx, ax
        mov     ax, 0E317h
        out     dx, ax

        mov     al, 34h                 ; channel 0 mode 2, wrap at 65536 clocks
        out     43h, al
        xor     al, al
        out     40h, al
        out     40h, al

        mov     ax, 0A000h
        mov     es, ax

        mov     ah, 0Fh                 ; fill
        call    mapm
        mov     di, DST
        mov     cx, NA
        TSTART
        xor     al, al
        rep     stosb
        TEND
        mov     [r_fill], bx

        call    fillsrc
        call    cmpdst                  ; before: DST is zero, so this must mismatch
        mov     [r_m0], ax

        mov     ah, 0Fh                 ; latch copy
        call    mapm
        mov     ah, 41h                 ; mode 13h's GR5, write mode 1
        call    gr5
        push    ds
        mov     ax, es
        mov     ds, ax
        mov     si, SRC
        mov     di, DST
        mov     cx, NA
        TSTART
        rep     movsb
        TEND
        pop     ds
        mov     [r_latch], bx
        mov     ah, 40h
        call    gr5
        call    cmpdst                  ; after: must be 0
        mov     [r_m1], ax

        mov     bp, 0                   ; plane by plane, VGA to VGA
.pl:    mov     ax, bp
        mov     ah, al
        call    rmap
        mov     cx, bp
        mov     ah, 1
        shl     ah, cl
        call    mapm
        push    ds
        mov     ax, es
        mov     ds, ax
        mov     si, SRC
        mov     di, DST
        mov     cx, NA
        TSTART
        rep     movsb
        TEND
        pop     ds
        mov     di, bp
        shl     di, 1
        mov     [r_plane + di], bx
        inc     bp
        cmp     bp, 4
        jb      .pl

        mov     bp, 0                   ; plane by plane, from RAM
.pr:    mov     cx, bp
        mov     ah, 1
        shl     ah, cl
        call    mapm
        mov     si, rbuf
        mov     di, DST
        mov     cx, NA
        TSTART
        rep     movsb
        TEND
        mov     di, bp
        shl     di, 1
        mov     [r_ram + di], bx
        inc     bp
        cmp     bp, 4
        jb      .pr

        mov     al, 36h                 ; channel 0 back to mode 3
        out     43h, al
        xor     al, al
        out     40h, al
        out     40h, al

        mov     ax, 0003h
        int     10h
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .noout
        mov     bx, ax
        mov     ah, 40h
        mov     cx, OUTLEN
        mov     dx, obuf
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.noout: mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h
