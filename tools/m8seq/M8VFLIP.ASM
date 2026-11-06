; M8VFLIP.COM - can the VGA half flip pages? For someone watching the screen.
;
; Unchained mode 13h, page 0 at address 0, page 1 at 4000h, shown by CRTC 0Ch, 0Dh. Red boxes at
; the top left count the stage on both pages:
;   1 box    page 0: all light grey
;   2 boxes  page 1: all blue. Anything else: the start address does not show page 1
;   3 boxes  a swap every 36 frames, about twice a second, no split
;   4 boxes  a swap every frame, in two close greys so the screen does not strobe: an even tone,
;            no bands or rolling split
; About 24 seconds. Times 360 frames by the BIOS tick. Writes M8VFLIP.BIN: "M8VF", then words:
; BIOS ticks for 360 frames, frames timed out waiting for retrace (0 expected). DOS boot.
;
;   nasm -f bin -o M8VFLIP.COM M8VFLIP.ASM

        org     100h

PAGE1   equ     4000h
STAGE   equ     420                     ; frames, ~6 s at 70 Hz
SLOW    equ     36

        jmp     start

oname   db      'M8VFLIP.BIN', 0
m_ok    db      'M8VFLIP: done', 13, 10, '$'
m_out   db      'M8VFLIP: cannot write M8VFLIP.BIN', 13, 10, '$'
obuf    db      'M8VF'
r_ticks dw      0
r_tout  dw      0
OUTLEN  equ     $ - obuf

; AL = page: start address high byte written alone, since the low byte never changes
show:   mov     dx, 3D4h
        mov     ah, 0
        or      al, al
        jz      .w
        mov     ah, PAGE1 >> 8
.w:     mov     al, 0Ch
        out     dx, ax
        ret

; To the start of the next vertical retrace; a bounded loop, so a dead status bit cannot hang.
vbl:    mov     dx, 3DAh
        mov     cx, 0FFFFh
.a:     in      al, dx
        test    al, 8
        jz      .b
        loop    .a
        inc     word [r_tout]
        ret
.b:     mov     cx, 0FFFFh
.c:     in      al, dx
        test    al, 8
        jnz     .ok
        loop    .c
        inc     word [r_tout]
.ok:    ret

; BX = frames
frames: push    bx
        call    vbl
        pop     bx
        dec     bx
        jnz     frames
        ret

; DI = page offset, AL = colour: 16000 addresses
fillp:  mov     cx, 16000
        rep     stosb
        ret

; AL = stage: that many boxes on both pages. Box 6 rows of 2 addresses (8 pixels).
boxes:  mov     bl, al
        xor     bh, bh
        mov     si, 0
.pg:    mov     cx, bx
        mov     dx, 0
.one:   push    cx
        mov     di, si
        add     di, dx
        add     di, 80 * 2
        mov     cx, 6
        mov     al, 4
.row:   mov     [es:di], al
        mov     [es:di + 1], al
        add     di, 80
        loop    .row
        add     dx, 4
        pop     cx
        loop    .one
        cmp     si, 0
        jne     .done
        mov     si, PAGE1
        jmp     .pg
.done:  ret

; AL = entry, AH BL BH = r g b
dac:    mov     dx, 3C8h
        out     dx, al
        inc     dx
        mov     al, ah
        out     dx, al
        mov     al, bl
        out     dx, al
        mov     al, bh
        out     dx, al
        ret

start:
        cld
        mov     ax, 0013h
        int     10h
        mov     dx, 3C4h
        mov     ax, 0604h
        out     dx, ax
        mov     ax, 0F02h
        out     dx, ax
        mov     dx, 3D4h
        mov     ax, 0014h
        out     dx, ax
        mov     ax, 0E317h
        out     dx, ax

        mov     ax, 0A000h
        mov     es, ax
        mov     di, 0
        mov     al, 7
        call    fillp
        mov     di, PAGE1
        mov     al, 1
        call    fillp

        mov     al, 0                   ; 1: page 0
        call    show
        mov     al, 1
        call    boxes
        push    ds
        mov     ax, 40h
        mov     ds, ax
        mov     si, [6Ch]
        pop     ds
        mov     bx, 360
        call    frames
        push    ds
        mov     ax, 40h
        mov     ds, ax
        mov     ax, [6Ch]
        pop     ds
        sub     ax, si
        mov     [r_ticks], ax
        mov     bx, STAGE - 360
        call    frames

        mov     al, 2                   ; 2: page 1
        call    boxes
        mov     al, 1
        call    show
        mov     bx, STAGE
        call    frames

        mov     al, 3                   ; 3: a swap every 36 frames
        call    boxes
        mov     bp, STAGE / SLOW
.sl:    mov     ax, bp
        and     al, 1
        call    show
        mov     bx, SLOW
        call    frames
        dec     bp
        jnz     .sl

        mov     al, 4                   ; 4: every frame, in close greys
        call    boxes
        mov     al, 7
        mov     ah, 20
        mov     bx, 1414h
        call    dac
        mov     al, 1
        mov     ah, 16
        mov     bx, 1010h
        call    dac
        mov     bp, STAGE
.fa:    call    vbl
        mov     ax, bp
        and     al, 1
        call    show
        dec     bp
        jnz     .fa

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
