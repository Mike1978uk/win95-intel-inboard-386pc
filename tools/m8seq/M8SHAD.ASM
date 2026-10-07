; M8SHAD.COM - which CRT register set the Mach8 read-back ports show.
; For 4AE8h (ADVFUNC_CNTL) = 2 and 6 (display left on the VGA, 640 and 1024 select), SHADOW_CTL
; (46EEh) = 0 and 3Fh, and SHADOW_SET (5AEEh) = 0, 1, 2, reads B2EEh (H_TOTAL/H_DISP), B6EEh
; (H_SYNC_STRT), BAEEh (H_SYNC_WID), C2EEh (V_TOTAL), C6EEh (V_DISP), CAEEh (V_SYNC_STRT).
; Writes no CRT register, only the pointer, the locks and 4AE8h; restores 5AEEh=0, 46EEh=0,
; 4AE8h=2 (what the ROM leaves) and sets text mode 3. Writes M8SHAD.BIN: "M8S1", then per
; combination the three settings (bytes) and six words.
;
;   nasm -f bin -o M8SHAD.COM M8SHAD.ASM

        org     100h

        mov     di, buf
        mov     si, advs
.adv:   lodsb
        or      al, al
        jz      .done
        mov     [cur_adv], al
        mov     bx, ctls
.ctl:   mov     al, [bx]
        cmp     al, 0FFh
        je      .adv
        mov     [cur_ctl], al
        xor     cx, cx                  ; shadow set 0..2
.set:   mov     dx, 4AE8h
        xor     ah, ah
        mov     al, [cur_adv]
        out     dx, ax
        mov     dx, 46EEh
        mov     al, [cur_ctl]
        out     dx, ax
        mov     dx, 5AEEh
        mov     ax, cx
        out     dx, ax
        mov     al, [cur_adv]
        stosb
        mov     al, [cur_ctl]
        stosb
        mov     al, cl
        stosb
        push    si
        mov     si, rports
.rd:    lodsw
        or      ax, ax
        jz      .rdend
        mov     dx, ax
        in      ax, dx
        stosw
        jmp     .rd
.rdend: pop     si
        inc     cx
        cmp     cx, 3
        jb      .set
        inc     bx
        jmp     .ctl

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

advs    db      2, 6, 0
ctls    db      0, 3Fh, 0FFh
rports  dw      0B2EEh, 0B6EEh, 0BAEEh, 0C2EEh, 0C6EEh, 0CAEEh, 0
cur_adv db      0
cur_ctl db      0
fname   db      'M8SHAD.BIN', 0
hdr     db      'M8S1'
buf:
