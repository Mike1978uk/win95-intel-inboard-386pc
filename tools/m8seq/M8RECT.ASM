; M8RECT.COM - the commands Windows/386 2.11's 8514.DRV sends when PC Paintbrush draws one filled box
; (86Box#6695: a border appears around it on 8514/A and Mach models, not on XGA), played on the card or
; the model so their pixels can be compared. M8RECT.INC comes from a MACH8_WLOG=1 bed log through
; gen_m8rect.py: the driver's engine state before the box, then every write from the box's fill to the
; last copy-back - fill, save to off-screen memory, the shape written back row by row, cursor included.
;
; Before playing it the screen is filled with FFh and the off-screen rows 300h-3FFh with 00h, so both
; machines start from the same pixels. Then four 64x16 areas around the box's corners are read with the
; driver's own row read (3318h, two pixels per word, low byte first): (E0h,94h), (1F8h,94h),
; (E0h,12Ah), (1F8h,12Ah). The 8514 display is on while it runs.
; Writes M8RECT.BIN: "M8RC", idle timeouts, data timeouts, then 4 x 1024 bytes.
;
;   nasm -f bin -o M8RECT.COM M8RECT.ASM

        org     100h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

oname   db      'M8RECT.BIN', 0
m_ok    db      'M8RECT: done', 13, 10, '$'
m_out   db      'M8RECT: cannot write M8RECT.BIN', 13, 10, '$'
hdr     db      'M8RC'
to_idle db      0
to_data db      0
areax   dw      0
areay   dw      0
areas   dw      0E0h, 94h, 1F8h, 94h, 0E0h, 12Ah, 1F8h, 12Ah

start:
        cld
        ; --- 8514 display on, as TEST.COM's 069Bh does ---
        mov     dx, 22E8h
        mov     ax, 53h
        out     dx, ax
        call    delay
        OUTW    4AE8h, 7
        call    delay

        ; --- engine reset, then the driver's state ---
        OUTW    32EEh, 0
        OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        mov     si, state
        call    play

        ; --- known pixels: screen FFh, off-screen rows 300h-3FFh 00h ---
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0FFh
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 2FFh
        OUTW    9AE8h, 40F3h
        call    idle
        OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        OUTW    82E8h, 300h
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 0FFh
        OUTW    9AE8h, 40F3h
        call    idle

        ; --- the driver's state again (the fills changed it), then the box ---
        mov     si, state
        call    play
        mov     si, window
        call    play
        call    idle

        ; --- read the four areas ---
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        mov     di, res
        mov     si, areas
        mov     bp, 4
.area:  lodsw
        mov     [areax], ax
        lodsw
        mov     [areay], ax
        push    si
        call    area_read
        pop     si
        dec     bp
        jnz     .area

        ; --- back to VGA ---
        call    idle
        OUTW    4AE8h, 2
        mov     dx, 2EAh
        mov     al, 0FFh
        out     dx, al
        mov     ax, 3
        int     10h

        mov     al, [to_idle]
        mov     [hdr2], al
        mov     al, [to_data]
        mov     [hdr2+1], al
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
        mov     cx, 2 + 4096
        mov     dx, hdr2
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

; 16 rows of 64 pixels at [areax],[areay] into DI, with the driver's read (3318h): RD_MASK FFh,
; PIX_CNTL A000h, MAJ_AXIS_PCNT 63, 32 words per row.
area_read:
        xor     bx, bx
.row:   call    idle
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    96E8h, 63
        mov     dx, 86E8h
        mov     ax, [areax]
        out     dx, ax
        mov     dx, 82E8h
        mov     ax, [areay]
        add     ax, bx
        out     dx, ax
        OUTW    9AE8h, 3318h
        mov     cx, 32
.w:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .w
        inc     bx
        cmp     bx, 16
        jb      .row
        ret

; Play "dw port, value" entries at SI until port 0. Waits for FIFO room before each, as the driver
; may; ports below 1000h and 22E8h are byte writes.
play:
.next:  call    fifo
        lodsw
        or      ax, ax
        jz      .done
        mov     dx, ax
        lodsw
        test    dx, 0F000h
        jz      .byte
        cmp     dx, 22E8h
        je      .byte
        out     dx, ax
        jmp     .next
.byte:  out     dx, al
        jmp     .next
.done:  ret

; Bounded waits. 65,536 polls of GP_STAT is about 0.4 s on the 5160.
idle:   push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        inc     byte [to_idle]
.iok:   pop     dx
        pop     cx
        ret

dready: push    cx
        push    dx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
        inc     byte [to_data]
.dok:   pop     dx
        pop     cx
        ret

fifo:   push    cx
        push    dx
        push    ax
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
        inc     byte [to_idle]
.qok:   pop     ax
        pop     dx
        pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret

%include "M8RECT.INC"

hdr2    db      0, 0
res:
