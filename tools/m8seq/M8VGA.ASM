; M8VGA.COM - the 28800 VGA half's registers, read only, in three BIOS modes.
;
; Card and bed are compared byte for byte; the probe does not judge. Per mode (03h text, 13h, 63h
; ATI 800x600x256), after INT 10h AH=00h: the mode INT 10h AH=0Fh reports, Misc output (3CCh),
; feature control (3CAh), sequencer 0-4, CRTC 0-18h, graphics controller 0-8, attribute controller
; 0-14h, DAC mask (3C6h) and state (3C7h), then ATI extended indexes 80h-FFh through 01CEh/01CFh.
; The attribute controller is read the standard way: 3DAh resets the flip-flop, the index goes out
; with bit 5 set so the screen stays on. Nothing is written except the mode sets and those indexes.
; Writes M8VGA.BIN: "M8VG", then per mode REC bytes. Ends in mode 03h. DOS boot.

        org     100h

REC     equ     2 + 2 + 5 + 25 + 9 + 21 + 2 + 128

        jmp     start

oname   db      'M8VGA.BIN', 0
m_ok    db      'M8VGA: done, results in M8VGA.BIN', 13, 10, '$'
m_out   db      'M8VGA: cannot write M8VGA.BIN', 13, 10, '$'
hdr     db      'M8VG'
modes   db      03h, 13h, 63h
mi      dw      0

start:
        cld
        mov     di, buf
        mov     word [mi], 0
.mode:  mov     bx, [mi]
        mov     al, [modes + bx]
        stosb                           ; mode asked for
        xor     ah, ah
        int     10h
        mov     ah, 0Fh
        int     10h
        stosb                           ; mode reported
        mov     dx, 3CCh                ; Misc output
        in      al, dx
        stosb
        mov     dx, 3CAh                ; feature control
        in      al, dx
        stosb
        mov     dx, 3C4h                ; sequencer 0-4
        mov     cx, 5
        call    rdidx
        mov     dx, 3D4h                ; CRTC 0-18h
        mov     cx, 25
        call    rdidx
        mov     dx, 3CEh                ; graphics controller 0-8
        mov     cx, 9
        call    rdidx
        xor     bl, bl                  ; attribute controller 0-14h
.att:   mov     dx, 3DAh
        in      al, dx
        mov     dx, 3C0h
        mov     al, bl
        or      al, 20h
        out     dx, al
        inc     dx                      ; 3C1h reads the data
        in      al, dx
        stosb
        inc     bl
        cmp     bl, 21
        jb      .att
        mov     dx, 3DAh
        in      al, dx
        mov     dx, 3C0h
        mov     al, 20h
        out     dx, al
        mov     dx, 3C6h                ; DAC mask
        in      al, dx
        stosb
        inc     dx                      ; 3C7h DAC state
        in      al, dx
        stosb
        mov     bl, 80h                 ; ATI extended 80h-FFh
.ati:   mov     dx, 1CEh
        mov     al, bl
        out     dx, al
        inc     dx
        in      al, dx
        stosb
        inc     bl
        jnz     .ati
        inc     word [mi]
        cmp     word [mi], 3
        jb      .mode

        mov     ax, 0003h
        int     10h
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
        mov     cx, 3 * REC
        mov     dx, buf
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

; Read CX indexed registers at DX (index) / DX+1 (data) from index 0 into [di].
rdidx:  xor     bl, bl
.r:     mov     al, bl
        out     dx, al
        inc     dx
        in      al, dx
        dec     dx
        stosb
        inc     bl
        loop    .r
        ret

buf:
