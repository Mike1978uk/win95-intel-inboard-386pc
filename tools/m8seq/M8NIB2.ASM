; M8NIB2.COM - which axis is the width in 8514/A command 4 ("rectangle, Y direction, using nibbles").
;
; M8NIB drew only square rectangles (MAJ = MIN), so it cannot tell. Windows 3.1's 8514.DRV fills
; text backgrounds with CMD 80B3h, MAJ_AXIS_PCNT = width - 1, MIN_AXIS_PCNT = height - 1, PIX_CNTL
; A000h, FRGD_MIX 07h (BKGD_COLOR, replace), no CPU data. This draws non-square rectangles both ways,
; each into its own cleared 32x16 area at x = 20h, row 600 + 16*case, engine reset between cases,
; and reads each area back. Writes M8NIB2.BIN: "M8N2" + 5 x 512 pixels.
;   case 0  80B3h  CUR_X 30h  MAJ 9 MIN 2   (8514.DRV's command, BKGD_COLOR 5Ah, no CPU data)
;   case 1  80B3h  CUR_X 30h  MAJ 2 MIN 9
;   case 2  80B3h  CUR_X 31h  MAJ 9 MIN 2   (not aligned to 4)
;   case 3  83F5h  CUR_X 30h  MAJ 9 MIN 2   (CPU data 01, 02, 03 ... as M8NIB, FRGD_MIX 47h)
;   case 4  83F5h  CUR_X 30h  MAJ 2 MIN 9
; Background 00. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8NIB2.BIN', 0
m_ok    db      'M8NIB2: done, results in M8NIB2.BIN', 13, 10, '$'
m_out   db      'M8NIB2: cannot write M8NIB2.BIN', 13, 10, '$'
hdr     db      'M8N2'
buf     times 5 * 512 db 0
dat     dw      0201h, 0403h, 0605h, 0807h, 0A09h, 0C0Bh, 0E0Dh, 100Fh
        dw      1211h, 1413h, 1615h, 1817h, 1A19h, 1C1Bh, 1E1Dh, 201Fh
cases   dw      80B3h, 30h, 9, 2
        dw      80B3h, 30h, 2, 9
        dw      80B3h, 31h, 9, 2
        dw      83F5h, 30h, 9, 2
        dw      83F5h, 30h, 2, 9
cn      dw      0
ybase   dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        mov     word [cn], 0
.c:     call    init
        mov     ax, [cn]
        mov     cl, 4
        shl     ax, cl
        add     ax, 600
        mov     [ybase], ax
        ; clear 32x16 at (20h, ybase)
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, 20h
        mov     dx, 82E8h
        mov     ax, [ybase]
        out     dx, ax
        OUTW    96E8h, 31
        OUTW    0BEE8h, 15
        OUTW    9AE8h, 40F3h
        call    idle
        ; the command 4 rectangle
        mov     bx, [cn]
        shl     bx, 1
        shl     bx, 1
        shl     bx, 1                   ; 8 bytes per case
        OUTW    0A2E8h, 5Ah             ; BKGD_COLOR for the 8514.DRV cases
        mov     ax, 07h                 ; foreground mix: BKGD_COLOR, replace (8514.DRV)
        test    word [cases + bx], 0100h
        jz      .m
        mov     ax, 47h                 ; foreground: CPU data
.m:     mov     dx, 0BAE8h
        out     dx, ax
        mov     dx, 86E8h
        mov     ax, [cases + bx + 2]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [ybase]
        add     ax, 2
        out     dx, ax
        mov     dx, 96E8h
        mov     ax, [cases + bx + 4]
        out     dx, ax
        mov     dx, 0BEE8h
        mov     ax, [cases + bx + 6]
        out     dx, ax
        mov     dx, 9AE8h
        mov     ax, [cases + bx]
        out     dx, ax
        test    word [cases + bx], 0100h
        jz      .nw
        mov     dx, 0E2E8h
        mov     si, dat
        mov     cx, 16
.w:     lodsw
        out     dx, ax
        loop    .w
.nw:    call    idle
        ; read the area back
        call    init_soft
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 20h
        mov     dx, 82E8h
        mov     ax, [ybase]
        out     dx, ax
        OUTW    96E8h, 31
        OUTW    0BEE8h, 15
        call    idle
        OUTW    9AE8h, 53B0h
        mov     ax, [cn]
        mov     cl, 9
        shl     ax, cl
        add     ax, buf
        mov     di, ax
        mov     cx, 256
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        inc     word [cn]
        cmp     word [cn], 5
        jb      .c

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 5 * 512
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

; Engine reset (clears any operation still waiting for data), pitch, scissors, masks.
init:   OUTW    42E8h, 9000h
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
init_soft:
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        OUTW    0CEEEh, 0
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

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
.dok:   pop     cx
        ret
