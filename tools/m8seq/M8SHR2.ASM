; M8SHR2.COM - Mach8 CRT shadow sets, set 2: re-locking, and what a SHADOW_SET write does in 1024 mode.
; M8SHRL showed per-set locks for set 1, and that a 5AEEh write in 1024 mode moved the read-back
; to set 1. Here set 2 is changed in 640 mode and 1024 mode is selected only to read it. Same
; engine as M8SHC2: each entry writes one port and then reads B2EEh twice.
; The sets keep the markers: reboot afterwards.
; Writes M8SHR2.BIN: "M8R2", B2EEh twice before any change, then per entry: port, value,
; read 1, read 2 (words).
;
;   nasm -f bin -o M8SHR2.COM M8SHR2.ASM

        org     100h

        mov     dx, 4AE8h
        mov     ax, 2
        out     dx, ax
        mov     di, buf
        call    read2

        mov     si, ops
.op:    lodsw
        or      ax, ax
        jz      .done
        mov     dx, ax
        stosw
        lodsw
        stosw
        out     dx, ax
        call    read2
        jmp     .op

.done:  mov     dx, 5AEEh
        xor     ax, ax
        out     dx, ax
        mov     dx, 46EEh
        out     dx, ax
        mov     dx, 4AE8h
        mov     ax, 2
        out     dx, ax
        mov     ax, 3
        int     10h

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

read2:  mov     dx, 0B2EEh
        in      ax, dx
        stosw
        in      ax, dx
        stosw
        ret

; Predicted H_TOTAL in 1024 mode, per-set locks / copy.
ops:
        dw      05AEEh, 0002h           ; 640 mode throughout the writes
        dw      002E8h, 0073h           ; set 2 = 73 (locked)
        dw      05AEEh, 0000h
        dw      002E8h, 00B0h           ; primary = B0
        dw      04AE8h, 0006h           ; read set 2                  73 / 73
        dw      04AE8h, 0002h
        dw      05AEEh, 0002h
        dw      046EEh, 0000h           ; unlock set 2
        dw      04AE8h, 0006h           ; read                        B0 / B0
        dw      04AE8h, 0002h
        dw      046EEh, 003Fh           ; re-lock set 2 (pointer at 2)
        dw      04AE8h, 0006h           ; read                        73 / B0  <- decides
        dw      05AEEh, 0000h           ; SHADOW_SET write in 1024 mode
        dw      05AEEh, 0000h           ; again, to see it is stable
        dw      046EEh, 0000h           ; SHADOW_CTL write (pointer 0)
        dw      04AE8h, 0006h           ; ADVFUNC again
        dw      04AE8h, 0002h
        dw      0

fname   db      'M8SHR2.BIN', 0
hdr     db      'M8R2'
buf:
