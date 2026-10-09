; M8SHRL.COM - Mach8 CRT shadow sets: what does re-locking an unlocked set show?
; Two rules fit M8SHCL and M8SHC2. Per-set locks: an unlocked shadow set shows the primary set's
; values and re-locking it shows its own again. Copy: unlocking copies the primary set into it,
; so re-locking keeps the copy. They predict different H_TOTAL read-backs after each 46EEh = 3Fh
; below. Same engine as M8SHC2: each entry writes one port and then reads B2EEh twice.
; The sets keep the markers: reboot afterwards.
; Writes M8SHRL.BIN: "M8RL", B2EEh twice before any change, then per entry: port, value,
; read 1, read 2 (words).
;
;   nasm -f bin -o M8SHRL.COM M8SHRL.ASM

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

; Predicted H_TOTAL after each entry, per-set locks / copy.
ops:
        dw      05AEEh, 0001h           ; 640 mode, set 1 (locked since POST)
        dw      002E8h, 0071h           ; set 1 = 71                  71 / 71
        dw      05AEEh, 0000h
        dw      002E8h, 0080h           ; primary = 80                71 / 71
        dw      05AEEh, 0001h
        dw      046EEh, 0000h           ; unlock set 1                80 / 80
        dw      046EEh, 003Fh           ; re-lock set 1               71 / 80  <- decides
        dw      05AEEh, 0000h
        dw      002E8h, 0090h           ; primary = 90                71 / 80
        dw      05AEEh, 0001h
        dw      046EEh, 0000h           ; unlock set 1                90 / 90
        dw      046EEh, 003Fh           ; re-lock set 1               71 / 90
        dw      04AE8h, 0006h           ; 1024 mode, set 2 (locked since POST)
        dw      05AEEh, 0002h
        dw      002E8h, 0072h           ; set 2 = 72                  72 / 72
        dw      05AEEh, 0000h
        dw      002E8h, 00A0h           ; primary = A0                72 / 72
        dw      05AEEh, 0002h
        dw      046EEh, 0000h           ; unlock set 2                A0 / A0
        dw      046EEh, 003Fh           ; re-lock set 2               72 / A0  <- decides
        dw      04AE8h, 0002h           ; back to 640                 71 / 90
        dw      0

fname   db      'M8SHRL.BIN', 0
hdr     db      'M8RL'
buf:
