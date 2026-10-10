; M8PF.COM - the extended polygon fill and SCAN_TO_X's fill flag, the two ATI fill modes no earlier
; probe exercised.
;
; Polygon fill (guide 7-9, 9-26): draw an outline off-screen, then one blit with DP_CONFIG POLY_FILL_MODE
; (bit 1) from it: the fill flag clears at each row start and toggles at each outline pixel (source
; pixel OR ~RD_MASK all ones, so FFh with RD_MASK FFh). DP_CONFIG 2213h = ATI's SCAN_TO_X fill (2211h,
; M8CONF) plus bit 1. Outline rows 500-503: edges at x 10/20, 5/30, 10/20/30, none. Blit x 0-64 into
; rows 0-3 with FRGD_COLOR 77h.
; Fill flag (guide 9-61): CUR_X 10, CUR_Y 10, then SCAN_TO_X 20, 30, 40, 50, FRGD_COLOR 55h. Row 11:
; CUR_X 8, one SCAN_TO_X 16, as a control.
; Destinations cleared first. Writes M8PF.BIN: "M8PF", rows 0-3, 10, 11, 256 pixels each.
; Engine only; DOS boot. Compare bed and card byte for byte.

        org     100h

RES             equ     0800h

        jmp     start

oname   db      'M8PF.BIN', 0
m_ok    db      'M8PF: done, results in M8PF.BIN', 13, 10, '$'
m_out   db      'M8PF: cannot write M8PF.BIN', 13, 10, '$'
hdr     db      'M8PF'
pix     dw      0
px      dw      0
py      dw      0
pc      dw      0
row     dw      0
edges   dw      500, 10, 500, 20, 501, 5, 501, 30, 502, 10, 502, 20, 502, 30, 0
rows    dw      0, 1, 2, 3, 10, 11, 500, 501, 502, 503, 0FFFFh

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        call    init
        OUTW    0DAEEh, 0
        OUTW    0DEEEh, 0
        OUTW    0E2EEh, 03FFh
        OUTW    0E6EEh, 03FFh

        mov     si, rows                ; clear every row used
.cl:    lodsw
        cmp     ax, 0FFFFh
        je      .cld
        mov     [row], ax
        push    si
        call    clear
        pop     si
        jmp     .cl
.cld:
        mov     si, edges               ; outline pixels, FFh
        mov     word [pc], 0FFh
.ed:    lodsw
        or      ax, ax
        jz      .edd
        mov     [py], ax
        lodsw
        mov     [px], ax
        push    si
        call    plot
        pop     si
        jmp     .ed
.edd:   call    idle

        ; extended polygon fill: one blit from the outline rows
        OUTW    0AEE8h, 00FFh           ; RD_MASK
        OUTW    0CEEEh, 2213h
        OUTW    0BAEEh, 0007h
        OUTW    0A6E8h, 0077h           ; FRGD_COLOR
        OUTW    8EE8h, 0                ; SRC_X
        OUTW    8AE8h, 500              ; SRC_Y
        OUTW    0B2EEh, 0
        OUTW    0BEEEh, 64
        OUTW    0C2EEh, 1
        OUTW    86E8h, 0
        OUTW    0A6EEh, 0
        OUTW    0AAEEh, 64
        OUTW    82E8h, 0
        OUTW    0AEEEh, 4
        call    idle

        ; SCAN_TO_X fill flag
        OUTW    0CEEEh, 2211h
        OUTW    0BAEEh, 0007h
        OUTW    0A6E8h, 0055h
        OUTW    82E8h, 10
        OUTW    86E8h, 10
        OUTW    0CAEEh, 20
        call    idle
        OUTW    0CAEEh, 30
        call    idle
        OUTW    0CAEEh, 40
        call    idle
        OUTW    0CAEEh, 50
        call    idle
        OUTW    82E8h, 11               ; control: one span
        OUTW    86E8h, 8
        OUTW    0CAEEh, 16
        call    idle

        mov     di, RES
        mov     si, rows
.rd:    lodsw
        cmp     ax, 500
        jae     .rdd
        mov     [row], ax
        push    si
        call    readrow
        pop     si
        jmp     .rd
.rdd:
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
        mov     cx, 6 * 256
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


; One pixel [pc] at ([px], [py]) by an 8514/A 1x1 fill, paint.
plot:   call    idle
        OUTW    0BAE8h, 27h
        OUTW    96E8h, 0
        OUTW    0BEE8h, 0
        mov     dx, 0A6E8h
        mov     ax, [pc]
        out     dx, ax
        mov     dx, 86E8h
        mov     ax, [px]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [py]
        out     dx, ax
        OUTW    9AE8h, 40F3h
        ret

; Row [row], x 0-255, to 00.
clear:  call    idle
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, [row]
        out     dx, ax
        OUTW    96E8h, 255
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        ret

; Read row [row] (256 pixels) into [di], the 16-bit read proved on both machines (M8CMP).
readrow:
        call    idle
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, [row]
        out     dx, ax
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
