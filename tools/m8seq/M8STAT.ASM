; M8STAT.COM - does DISP_STATUS (02E8h) bit 2, LINE_SYNC, toggle every scanline on a Mach8?
;
; Guide 8-8: bit 0 SENSE, bit 1 VSYNC, bit 2 LINE_SYNC toggles each line. The model has SENSE and
; VSYNC only. LINE_SYNC is the per-line clock a raster effect or a split between the card's two
; halves would run on. Reads 02E8h 8192 times back to back (~5.8 us each on the 5160, a few reads
; per line) with interrupts off, as the display is at DOS (8514 side not driving the monitor).
; Writes M8STAT.BIN: "M8ST", then the 8192 bytes read. Reads only.

        org     100h

N               equ     8192

        mov     di, buf
        mov     dx, 02E8h
        mov     cx, N
        cli
.r:     in      al, dx
        stosb
        loop    .r
        sti

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + N
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

oname   db      'M8STAT.BIN', 0
m_ok    db      'M8STAT: done, results in M8STAT.BIN', 13, 10, '$'
m_out   db      'M8STAT: cannot write M8STAT.BIN', 13, 10, '$'
hdr     db      'M8ST'
buf:
