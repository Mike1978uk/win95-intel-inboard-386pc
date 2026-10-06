; M8NIB.COM - what 8514/A command 4 ("rectangle, Y direction, using nibbles") does on a Mach8.
;
; TEST.COM's Test Sequence 1 op 95 (83F5h, 3x3 at (32h,20h), data 0102 0304 0506) gives a
; result with no obvious rule: columns 32h and 33h both 01 02 03 down, column 34h 04 05 06 up.
; This draws command 4 six ways, each into its own cleared 32x16 area at x = 20h, row 600 + 16*case,
; fed with incrementing bytes 01, 02, 03 ... (16 words), engine reset between cases. Then each
; area is read back with the 16-bit 8514/A read. Writes M8NIB.BIN: "M8N1" + 6 x 512 pixels.
;   case 0  83F5h  CUR_X 32h  MAJ 2 MIN 2   (TEST.COM's op 95)
;   case 1  83F5h  CUR_X 30h  MAJ 2 MIN 2   (aligned to 4)
;   case 2  83F5h  CUR_X 30h  MAJ 3 MIN 3
;   case 3  83F1h  CUR_X 30h  MAJ 3 MIN 3   (LAST_PIXEL on)
;   case 4  81F5h  CUR_X 30h  MAJ 3 MIN 3   (BYTE_SEQ, bit 9, clear)
;   case 5  83F5h  CUR_X 31h  MAJ 3 MIN 3
; FRGD_MIX 47h (CPU data), PIX_CNTL A000h. Background 00. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8NIB.BIN', 0
m_ok    db      'M8NIB: done, results in M8NIB.BIN', 13, 10, '$'
m_out   db      'M8NIB: cannot write M8NIB.BIN', 13, 10, '$'
hdr     db      'M8N1'
buf     times 6 * 512 db 0
dat     dw      0201h, 0403h, 0605h, 0807h, 0A09h, 0C0Bh, 0E0Dh, 100Fh
        dw      1211h, 1413h, 1615h, 1817h, 1A19h, 1C1Bh, 1E1Dh, 201Fh
cases   dw      83F5h, 32h, 2, 2
        dw      83F5h, 30h, 2, 2
        dw      83F5h, 30h, 3, 3
        dw      83F1h, 30h, 3, 3
        dw      81F5h, 30h, 3, 3
        dw      83F5h, 31h, 3, 3
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
        OUTW    0BAE8h, 47h             ; foreground: CPU data
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
        mov     dx, 0E2E8h
        mov     si, dat
        mov     cx, 16
.w:     lodsw
        out     dx, ax
        loop    .w
        call    idle
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
        cmp     word [cn], 6
        jb      .c

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 6 * 512
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
