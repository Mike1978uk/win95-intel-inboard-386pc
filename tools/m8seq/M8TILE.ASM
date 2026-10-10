; M8TILE.COM - does an ATI extended blit move its source independently of its destination?
;
; Guide 9-42..44: the source pointer runs from SRC_X to SRC_X_END, then restarts at SRC_X_START on the
; next source row, whatever the destination's shape. If so, one blit can tile a texture across a
; rectangle or unpack a linear sprite into one. The blit setup is the VRAM-to-VRAM shape ATI's
; Win 3.11 driver used on this card (M8CONF: DP_CONFIG 6211h), with paint (7) as the mix.
;
; Source: row 100 x 0-7 = 10h-17h, row 101 x 0-7 = 20h-27h, row 102 x 0-15 = 30h-3Fh.
; A (tile): source x 0-8 from row 100, top-to-bottom, into x 0-40, rows 0-1.
; B (unpack): source x 0-16 from row 102, into x 0-4, rows 200-203.
; Destinations cleared to 00 first. Writes M8TILE.BIN: "M8TL", then rows 0, 1, 200-203, 256 pixels
; each. Engine only; DOS boot.

        org     100h

RES             equ     0800h

        jmp     start

oname   db      'M8TILE.BIN', 0
m_ok    db      'M8TILE: done, results in M8TILE.BIN', 13, 10, '$'
m_out   db      'M8TILE: cannot write M8TILE.BIN', 13, 10, '$'
hdr     db      'M8TL'
pix     dw      0
px      dw      0
py      dw      0
pc      dw      0
row     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        call    init
        OUTW    0DAEEh, 0               ; extended scissors: whole 1024x1024
        OUTW    0DEEEh, 0
        OUTW    0E2EEh, 03FFh
        OUTW    0E6EEh, 03FFh

        mov     word [row], 0           ; clear rows 0-1 and 200-203, x 0-255
        call    clear
        mov     word [row], 1
        call    clear
        mov     word [row], 200
.cl:    call    clear
        inc     word [row]
        cmp     word [row], 204
        jb      .cl

        mov     word [py], 100          ; sources
        mov     word [pc], 10h
        mov     word [px], 0
.s1:    call    plot
        inc     word [pc]
        inc     word [px]
        cmp     word [px], 8
        jb      .s1
        mov     word [py], 101
        mov     word [pc], 20h
        mov     word [px], 0
.s2:    call    plot
        inc     word [pc]
        inc     word [px]
        cmp     word [px], 8
        jb      .s2
        mov     word [py], 102
        mov     word [pc], 30h
        mov     word [px], 0
.s3:    call    plot
        inc     word [pc]
        inc     word [px]
        cmp     word [px], 16
        jb      .s3
        call    idle

        ; A: tile 8 source pixels across 40, two rows
        OUTW    0CEEEh, 6211h
        OUTW    0BAEEh, 0007h
        OUTW    8EE8h, 0                ; SRC_X
        OUTW    8AE8h, 100              ; SRC_Y
        OUTW    0B2EEh, 0               ; SRC_X_START
        OUTW    0BEEEh, 8               ; SRC_X_END
        OUTW    0C2EEh, 1               ; SRC_Y_DIR top-to-bottom
        OUTW    86E8h, 0                ; CUR_X
        OUTW    0A6EEh, 0               ; DEST_X_START
        OUTW    0AAEEh, 40              ; DEST_X_END
        OUTW    82E8h, 0                ; CUR_Y
        OUTW    0AEEEh, 2               ; DEST_Y_END: go
        call    idle

        ; B: unpack 16 source pixels into 4 x 4
        OUTW    0CEEEh, 6211h
        OUTW    0BAEEh, 0007h
        OUTW    8EE8h, 0
        OUTW    8AE8h, 102
        OUTW    0B2EEh, 0
        OUTW    0BEEEh, 16
        OUTW    0C2EEh, 1
        OUTW    86E8h, 0
        OUTW    0A6EEh, 0
        OUTW    0AAEEh, 4
        OUTW    82E8h, 200
        OUTW    0AEEEh, 204
        call    idle

        mov     di, RES
        mov     word [row], 0
        call    readrow
        mov     word [row], 1
        call    readrow
        mov     word [row], 200
.rd:    call    readrow
        inc     word [row]
        cmp     word [row], 204
        jb      .rd

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
