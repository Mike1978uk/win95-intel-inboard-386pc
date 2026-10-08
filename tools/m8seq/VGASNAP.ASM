; VGASNAP.COM - dump the VGA half's readable registers, so a state that blanks the picture can be
; compared against a cold boot. Reads only: index selects and reads. The attribute controller is
; indexed with bit 5 set (palette address source = display), so selecting an index never blanks
; the screen, and 3DAh is read first and last to leave its flip-flop in the index state.
;
; Writes VGASNAP.BIN: "VGS1", misc (3CCh), feature (3CAh), DAC mask (3C6h), then
; sequencer 0-4, CRTC 0-18h, graphics 0-8, attribute 0-14h.

        org     100h

        mov     di, buf
        mov     dx, 3CCh
        in      al, dx
        stosb
        mov     dx, 3CAh
        in      al, dx
        stosb
        mov     dx, 3C6h
        in      al, dx
        stosb

        mov     dx, 3C4h                ; sequencer
        mov     cx, 5
        call    idx

        mov     dx, 3D4h                ; CRTC (colour, misc bit 0 set)
        mov     cx, 19h
        call    idx

        mov     dx, 3CEh                ; graphics controller
        mov     cx, 9
        call    idx

        mov     cx, 15h                 ; attribute controller
        xor     bl, bl
.a:     mov     dx, 3DAh
        in      al, dx
        mov     dx, 3C0h
        mov     al, bl
        or      al, 20h
        out     dx, al
        inc     dx
        in      al, dx
        stosb
        inc     bl
        loop    .a
        mov     dx, 3DAh
        in      al, dx

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, fname
        int     21h
        jc      .fail
        mov     bx, ax
        mov     ah, 40h
        mov     cx, di
        sub     cx, hdr
        mov     dx, hdr
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     ax, 4C00h
        int     21h
.fail:  mov     ax, 4C01h
        int     21h

; Read CX registers through index port DX / data port DX+1, from index 0.
idx:    xor     bl, bl
.i:     mov     al, bl
        out     dx, al
        inc     dx
        in      al, dx
        dec     dx
        stosb
        inc     bl
        loop    .i
        ret

fname   db      'VGASNAP.BIN', 0
hdr     db      'VGS1'
buf:
