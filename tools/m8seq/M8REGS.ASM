; M8REGS.COM - dump the ATI Graphics Ultra's readable state, to compare the real card with 86Box.
; ATI extended registers A0h-BFh through 1CEh/1CFh, then one word IN from each port in the table.
; Only status, configuration and read-back ports; no PIX_TRANS or other data ports, so nothing
; is drained or started. Writes M8REGS.BIN: "M8R1", 32 extended-register bytes, then for each
; table entry the port (word) and the value read (word).
;
;   nasm -f bin -o M8REGS.COM M8REGS.ASM

        org     100h

        mov     di, buf
        mov     cx, 32
        mov     bl, 0A0h
.ext:   mov     dx, 1CEh
        mov     al, bl
        out     dx, al
        inc     dx
        in      al, dx
        stosb
        inc     bl
        loop    .ext

        mov     si, ports
.prt:   lodsw
        or      ax, ax
        jz      .save
        mov     dx, ax
        stosw
        in      ax, dx
        stosw
        jmp     .prt

.save:  mov     ah, 3Ch
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

ports   dw      02E8h, 06E8h, 22E8h, 42E8h, 9AE8h
        dw      12EEh, 16EEh, 1AEEh, 22EEh, 32EEh, 36EEh, 3AEEh, 42EEh, 46EEh, 4AEEh
        dw      52EEh, 56EEh, 5EEEh, 62EEh, 6AEEh, 72EEh, 76EEh, 7AEEh, 7EEEh
        dw      82EEh, 86EEh, 8EEEh, 92EEh, 96EEh, 9AEEh, 0A2EEh, 0B2EEh, 0B6EEh, 0BAEEh
        dw      0C2EEh, 0C6EEh, 0CAEEh, 0CEEEh, 0D6EEh, 0DAEEh, 0DEEEh, 0FAEEh
        dw      0

fname   db      'M8REGS.BIN', 0
hdr     db      'M8R1'
buf:
