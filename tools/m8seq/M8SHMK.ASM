; M8SHMK.COM - the rule for the Mach8's CRT shadow sets: which set a write lands in and which
; set B2EEh reads back. The display stays on the VGA (4AE8h = 2) throughout.
;
; Phase 1: SHADOW_CTL = 0; for SHADOW_SET n = 0, 1, 2 write H_TOTAL (02E8h) = 70h + n.
; Phase 2: read B2EEh for SHADOW_CTL 0 and 3Fh x SHADOW_SET 0, 1, 2.
; Phase 3: SHADOW_CTL = 3Fh; for SHADOW_SET n write H_TOTAL = 80h + n.
; Phase 4: read as phase 2.
; The sets keep the markers: reboot afterwards so the ROM reloads them.
; Writes M8SHMK.BIN: "M8K1", then 12 words (phase 2 then phase 4, CTL-major, set-minor).
;
;   nasm -f bin -o M8SHMK.COM M8SHMK.ASM

        org     100h

        mov     dx, 4AE8h
        mov     ax, 2
        out     dx, ax
        mov     di, buf

        xor     bl, bl                  ; phase 1: unlocked, markers 70h + n
        mov     bh, 70h
        call    mark
        call    readall
        mov     bl, 3Fh                 ; phase 3: locked, markers 80h + n
        mov     bh, 80h
        call    mark
        call    readall

        mov     dx, 5AEEh
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

; BL = SHADOW_CTL, BH = first marker. Writes H_TOTAL = BH + n into SHADOW_SET n.
mark:   xor     cx, cx
.m:     mov     dx, 46EEh
        xor     ah, ah
        mov     al, bl
        out     dx, ax
        mov     dx, 5AEEh
        mov     ax, cx
        out     dx, ax
        mov     dx, 02E8h
        mov     al, bh
        add     al, cl
        xor     ah, ah
        out     dx, ax
        inc     cx
        cmp     cx, 3
        jb      .m
        ret

; Reads B2EEh for SHADOW_CTL 0 and 3Fh x SHADOW_SET 0..2 into [DI].
readall:
        mov     si, ctls
.c:     lodsb
        cmp     al, 0FFh
        je      .r
        xor     cx, cx
.s:     mov     dx, 46EEh
        xor     ah, ah
        out     dx, ax
        push    ax
        mov     dx, 5AEEh
        mov     ax, cx
        out     dx, ax
        mov     dx, 0B2EEh
        in      ax, dx
        stosw
        pop     ax
        inc     cx
        cmp     cx, 3
        jb      .s
        jmp     .c
.r:     ret

ctls    db      0, 3Fh, 0FFh
fname   db      'M8SHMK.BIN', 0
hdr     db      'M8K1'
buf:
