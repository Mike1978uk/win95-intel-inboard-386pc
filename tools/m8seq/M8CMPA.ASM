; M8CMPA.COM - M8CMP with WRT_MASK 0Fh during the fill: does the ATI destination compare (DEST_CMP_FN)
; look only at the planes WRT_MASK enables, as guide 9-50 note 4 says?
;
; As M8CMP: for each compare colour in COLS and each code 40h,48h,...,78h, fill row 0 with 00-FFh
; (compare off, all planes), set COLOR_CMP and DEST_CMP_FN, set WRT_MASK 0Fh, fill the row with EEh,
; restore WRT_MASK FFh and DEST_CMP_FN 0, read the row back. Writes M8CMPA.BIN: "M8CA", then per case:
; colour, code, 256 pixels. Engine only; DOS boot. Compare bed and card byte for byte.

        org     100h

RES             equ     0800h
REC             equ     2 + 256
NCOL            equ     3
NCODE           equ     8

        jmp     start

oname   db      'M8CMPA.BIN', 0
m_ok    db      'M8CMPA: done, results in M8CMPA.BIN', 13, 10, '$'
m_out   db      'M8CMPA: cannot write M8CMPA.BIN', 13, 10, '$'
hdr     db      'M8CA'
cols    dw      08h, 55h, 80h
ci      dw      0
code    dw      0
recp    dw      0
pix     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        call    init
        mov     word [recp], RES
        mov     word [ci], 0
.col:   mov     word [code], 40h
.code:  call    ramp
        call    idle
        mov     bx, [ci]
        shl     bx, 1
        mov     ax, [cols + bx]
        mov     dx, 0B2E8h              ; COLOR_CMP
        out     dx, ax
        mov     dx, 0EEEEh              ; DEST_CMP_FN
        mov     ax, [code]
        out     dx, ax
        OUTW    0AAE8h, 000Fh           ; low four planes only
        OUTW    0A6E8h, 0EEh            ; fill the row with EE, subject to the compare
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0               ; MIN_AXIS: one row
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0AAE8h, 00FFh           ; all planes and compare off for the read
        OUTW    0EEEEh, 0
        mov     di, [recp]
        mov     bx, [ci]
        shl     bx, 1
        mov     al, [cols + bx]
        stosb
        mov     al, [code]
        stosb
        call    readrow
        add     word [recp], REC
        add     word [code], 8
        cmp     word [code], 80h
        jb      .code
        inc     word [ci]
        cmp     word [ci], NCOL
        jb      .col

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, NCOL * NCODE * REC
        mov     dx, RES
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
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        ret

; Row 0, x = 0..255, pixel x = x: 256 one-pixel fills.
ramp:   OUTW    0EEEEh, 0
        OUTW    0BAE8h, 27h
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        mov     word [pix], 0
.p:     call    idle
        mov     dx, 0A6E8h
        mov     ax, [pix]
        out     dx, ax
        mov     dx, 86E8h
        out     dx, ax
        OUTW    82E8h, 0
        OUTW    9AE8h, 40F3h
        inc     word [pix]
        cmp     word [pix], 256
        jb      .p
        call    idle
        ret

; Read row 0 (256 pixels) into [di] with the 16-bit read proved on both machines.
readrow:
        call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        call    idle
        OUTW    9AE8h, 53B0h
        mov     cx, 128
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
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
