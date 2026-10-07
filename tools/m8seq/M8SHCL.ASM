; M8SHCL.COM - the Mach8 CRT shadow-set rule, one change at a time.
; Plays M8SHCL.INC: each entry writes one port (SHADOW_SET 5AEEh, SHADOW_CTL 46EEh or
; H_TOTAL 02E8h) and then reads B2EEh twice. Part A changes only the pointer and the lock;
; parts B and C write one H_TOTAL marker per pass (70h+n unlocked, 80h+n locked) and sweep.
; The display stays on the VGA (4AE8h = 2). The sets keep the markers: reboot afterwards.
; Writes M8SHCL.BIN: "M8C1", B2EEh twice before any change, then per entry: port, value,
; read 1, read 2 (words).
;
;   nasm -f bin -o M8SHCL.COM M8SHCL.ASM

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

ops:
%include "M8SHCL.INC"

fname   db      'M8SHCL.BIN', 0
hdr     db      'M8C1'
buf:
