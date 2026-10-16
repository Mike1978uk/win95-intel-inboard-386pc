; M8SPLIT.COM - the 28800's separate read and write banks, as VVESA.COM 2.22 drives them (docs/mach8_levers.md):
; B2h = write bank in bits 1-4, read bank in bits 5-7 (+ bit 0 for banks 8-15); BEh bit 3 enables the split.
; If it works, the CPU copies between two 64 KB banks of VGA memory in one pass, with no bank switch per block.
;
; Sets ATI mode 62h (640x480x256, the 28800's 512 KB) through INT 10h, restores B2h, BEh and mode 03h at the end.
;   test 1  BEh bit 3 set; fill bank 1 (4 KB pattern); B2h = read 1 / write 2; rep movsw A000:0 -> A000:0, timed;
;           then read bank 2 and count mismatching words.
;   test 2  BEh bit 3 clear; copy bank 1 -> RAM -> bank 3 with a bank switch between, timed; count mismatches in bank 3.
;   test 3  BEh bit 3 clear; B2h = read 1 / write 4; rep movsw; count mismatches in bank 4 (2048 = the split did not act).
; Writes M8SPLIT.BIN: "M8S1", then words: mode read back, B2h and BEh as found, ticks 1, mismatches 1, ticks 2,
; mismatches 2, mismatches 3. Ticks: raw PIT channel 0 differences, as M8TIME.

        org     100h

        jmp     start

oname   db      'M8SPLIT.BIN', 0
m_ok    db      'M8SPLIT: done, results in M8SPLIT.BIN', 13, 10, '$'
m_out   db      'M8SPLIT: cannot write M8SPLIT.BIN', 13, 10, '$'
hdr     db      'M8S1'
r_mode  dw      0
r_b2    dw      0
r_be    dw      0
r_t1    dw      0
r_m1    dw      0
r_t2    dw      0
r_m2    dw      0
r_m3    dw      0
t0      dw      0
NW      equ     2048                    ; words per test (4 KB): stays inside one PIT wrap
rbuf    times NW dw 0

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

%macro  TEND 1
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
        sti
        mov     bx, [cs:t0]
        sub     bx, ax
        mov     [cs:%1], bx
%endmacro

start:
        cld
        mov     ax, 0062h
        int     10h
        mov     ah, 0Fh
        int     10h
        xor     ah, ah
        mov     [r_mode], ax
        mov     al, 0B2h
        call    xrd
        mov     [r_b2], ax
        mov     al, 0BEh
        call    xrd
        mov     [r_be], ax

        ; ---- test 1: split banking ----
        mov     al, 0BEh
        mov     ah, byte [r_be]
        or      ah, 08h
        call    xwr
        mov     cx, 0101h               ; read 1, write 1
        call    bank
        call    fillp
        mov     cx, 0102h               ; read 1, write 2
        call    bank
        push    ds
        push    es
        mov     ax, 0A000h
        mov     ds, ax
        mov     es, ax
        xor     si, si
        xor     di, di
        mov     cx, NW
        TSTART
        rep     movsw
        TEND    r_t1
        pop     es
        pop     ds
        mov     cx, 0202h
        call    bank
        call    check
        mov     [r_m1], ax

        ; ---- test 2: through RAM, split off ----
        mov     al, 0BEh
        mov     ah, byte [r_be]
        and     ah, 0F7h
        call    xwr
        mov     cx, 0303h
        call    bank
        call    zero
        TSTART
        mov     cx, 0101h
        call    bank
        push    ds
        mov     ax, 0A000h
        mov     ds, ax
        xor     si, si
        mov     di, rbuf
        mov     cx, NW
        rep     movsw
        pop     ds
        mov     cx, 0303h
        call    bank
        push    es
        mov     ax, 0A000h
        mov     es, ax
        mov     si, rbuf
        xor     di, di
        mov     cx, NW
        rep     movsw
        pop     es
        TEND    r_t2
        call    check
        mov     [r_m2], ax

        ; ---- test 3: read 1 / write 4 with the split bit clear ----
        mov     cx, 0404h
        call    bank
        call    zero
        mov     cx, 0104h
        call    bank
        push    ds
        push    es
        mov     ax, 0A000h
        mov     ds, ax
        mov     es, ax
        xor     si, si
        xor     di, di
        mov     cx, NW
        rep     movsw
        pop     es
        pop     ds
        mov     cx, 0404h
        call    bank
        call    check
        mov     [r_m3], ax

        ; ---- restore ----
        mov     al, 0B2h
        mov     ah, byte [r_b2]
        call    xwr
        mov     al, 0BEh
        mov     ah, byte [r_be]
        call    xwr
        mov     ax, 0003h
        int     10h

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 16
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

; extended register read: AL = index -> AX = value (AH 0)
xrd:    mov     dx, 1CEh
        out     dx, al
        inc     dx
        in      al, dx
        xor     ah, ah
        ret

; extended register write: AL = index, AH = value
xwr:    mov     dx, 1CEh
        out     dx, ax
        ret

; B2h from CH = read bank, CL = write bank (0-7): write bank in bits 1-4, read bank in bits 5-7; other bits as found
bank:   push    ax
        push    bx
        mov     bl, byte [r_b2]
        and     bl, 01h
        mov     al, cl
        shl     al, 1
        or      bl, al
        mov     al, ch
        and     al, 7
        push    cx
        mov     cl, 5
        shl     al, cl
        pop     cx
        or      bl, al
        mov     ah, bl
        mov     al, 0B2h
        call    xwr
        pop     bx
        pop     ax
        ret

; the pattern for word i: i XOR A5C3h
fillp:  push    es
        mov     ax, 0A000h
        mov     es, ax
        xor     di, di
        xor     bx, bx
        mov     cx, NW
.f:     mov     ax, bx
        xor     ax, 0A5C3h
        stosw
        inc     bx
        loop    .f
        pop     es
        ret

zero:   push    es
        mov     ax, 0A000h
        mov     es, ax
        xor     di, di
        xor     ax, ax
        mov     cx, NW
        rep     stosw
        pop     es
        ret

; AX = number of words in A000:0 (the current read bank) that differ from the pattern
check:  push    ds
        mov     ax, 0A000h
        mov     ds, ax
        xor     si, si
        xor     bx, bx
        xor     dx, dx
        mov     cx, NW
.k:     lodsw
        push    bx
        xor     bx, 0A5C3h
        cmp     ax, bx
        pop     bx
        je      .ok
        inc     dx
.ok:    inc     bx
        loop    .k
        pop     ds
        mov     ax, dx
        ret
