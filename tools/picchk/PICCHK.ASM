; PICCHK.COM - check that the 8259 PIC reads back what was written, and that
; interrupts arrive on the vectors they should (#10, hypothesis H1).
;
; On a 5150 the 8259's data pins share the CPU's AD lines, which the Inboard drives
; through the 8088 socket; on a 5160 they sit behind the external-bus buffer. A
; timing or float fault on that path shows as a wrong read here.
;
;   1. Mask register: 20 patterns written to port 21h and read back, 4,096 rounds,
;      interrupts off within a round. Every bit is driven 0 and 1.
;   2. OCW3: ISR read at port 20h must be 0 at the DOS prompt, IMR must read the
;      saved mask, 4,096 times.
;   3. Vectors 08h-0Fh counted for 5 seconds (91 timer ticks). IRQ 7 entries with
;      ISR bit 7 clear are spurious: the 8259 found no request at INTA.
;
;   PICCHK      run the checks
;   PICCHK /T   test the test: phase 1 expects the wrong value, so it must
;               report mask_err=65535. If it does not, the result is worthless.
;
; Each run appends one line to C:\PICCHK.TXT. Run at the DOS prompt, not in Windows.
; Counts stop at 65535. Expected on a healthy machine: all *_err 0, irq0 near 91,
; spur7 0.
;
; Assemble: fasm PICCHK.ASM PICCHK.COM

        org     100h
        use16

ROUNDS  = 4096
NPAT    = 20

start:  mov     si, 81h
scan:   lodsb
        cmp     al, 0Dh
        je      scanned
        cmp     al, '/'
        jne     scan
        lodsb
        and     al, 0DFh
        cmp     al, 'T'
        jne     scan
        mov     byte [flip], 0FFh
        mov     byte [tchar], 'T'
        jmp     scan

scanned:
        in      al, 21h
        mov     [imr], al

; Phase 1: mask register write and read back.
        mov     bp, ROUNDS
p1r:    cli
        mov     si, pats
        mov     cx, NPAT
p1p:    mov     al, [si]
        out     21h, al
        jmp     short $+2
        jmp     short $+2
        in      al, 21h
        mov     ah, [si]
        xor     ah, [flip]
        cmp     al, ah
        je      p1ok
        mov     bx, mskerr
        call    bump
        cmp     byte [haveb], 0
        jne     p1ok
        mov     [badp], ah
        mov     [badg], al
        mov     byte [haveb], 1
p1ok:   inc     si
        loop    p1p
        mov     al, [imr]
        out     21h, al
        sti
        dec     bp
        jnz     p1r

; Phase 2: ISR through OCW3, then IMR, interleaved.
        mov     bp, ROUNDS
p2r:    cli
        mov     al, 0Bh
        out     20h, al
        jmp     short $+2
        jmp     short $+2
        in      al, 20h
        test    al, al
        jz      p2a
        mov     bx, isrerr
        call    bump
        cmp     byte [isrv], 0
        jne     p2a
        mov     [isrv], al
p2a:    in      al, 21h
        cmp     al, [imr]
        je      p2b
        mov     bx, imrerr
        call    bump
p2b:    mov     al, 0Ah                 ; leave OCW3 reading IRR, as the BIOS does
        out     20h, al
        sti
        dec     bp
        jnz     p2r

; Phase 3: count vectors 08h-0Fh for 91 timer ticks.
        mov     cx, 8
        mov     bl, 08h
        mov     di, old
        mov     si, stubtab
hook:   mov     ah, 35h
        mov     al, bl
        int     21h
        mov     [di], bx
        mov     [di+2], es
        mov     bl, al                  ; AL is preserved by AH=35h
        mov     dx, [si]
        mov     ah, 25h
        int     21h
        inc     bl
        add     di, 4
        add     si, 2
        loop    hook

        push    ds
        mov     ax, 40h
        mov     es, ax
        mov     ax, [es:6Ch]
        mov     [tick0], ax
        mov     dx, 1000                ; fallback if the timer never ticks
wait1:  mov     ax, [es:6Ch]
        sub     ax, [tick0]
        cmp     ax, 91
        jae     waited
        mov     cx, 0FFFFh
spin:   loop    spin
        dec     dx
        jnz     wait1
        mov     byte [tout], 'Y'
waited: mov     [ticks], ax
        pop     ds

        mov     cx, 8
        mov     bl, 08h
        mov     si, old
unhook: push    ds
        mov     ah, 25h
        mov     al, bl
        lds     dx, [si]
        int     21h
        pop     ds
        inc     bl
        add     si, 4
        loop    unhook

; Report.
        mov     al, [imr]
        mov     di, h_imr
        call    hex
        mov     ax, [mskerr]
        mov     di, d_msk
        call    dec5
        mov     al, [badp]
        mov     di, h_badp
        call    hex
        mov     al, [badg]
        mov     di, h_badg
        call    hex
        mov     ax, [isrerr]
        mov     di, d_isr
        call    dec5
        mov     al, [isrv]
        mov     di, h_isrv
        call    hex
        mov     ax, [imrerr]
        mov     di, d_imr
        call    dec5
        mov     si, cnt
        mov     di, d_irq
        mov     cx, 8
rirq:   lodsw
        call    dec5
        inc     di                      ; skip the separator
        loop    rirq
        mov     ax, [spur]
        mov     di, d_spur
        call    dec5
        mov     ax, [ticks]
        mov     di, d_tick
        call    dec5

        mov     dx, line
        mov     ah, 09h
        int     21h
        mov     ax, [mskerr]
        or      ax, [isrerr]
        or      ax, [imrerr]
        or      ax, [spur]
        mov     dx, v_ok
        jz      say
        mov     dx, v_bad
say:    mov     ah, 09h
        int     21h

        mov     ax, 3D01h
        mov     dx, fname
        int     21h
        jnc     opened
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, fname
        int     21h
        jc      done
opened: mov     bx, ax
        mov     ax, 4202h
        xor     cx, cx
        xor     dx, dx
        int     21h
        mov     ah, 40h
        mov     cx, line_len
        mov     dx, line
        int     21h
        mov     ah, 3Eh
        int     21h
done:   mov     ax, 4C00h
        int     21h

; Interrupt stubs: count, then chain to the handler that was there.
iv0:    inc     word [cs:cnt+0]
        jmp     far dword [cs:old+0]
iv1:    inc     word [cs:cnt+2]
        jmp     far dword [cs:old+4]
iv2:    inc     word [cs:cnt+4]
        jmp     far dword [cs:old+8]
iv3:    inc     word [cs:cnt+6]
        jmp     far dword [cs:old+12]
iv4:    inc     word [cs:cnt+8]
        jmp     far dword [cs:old+16]
iv5:    inc     word [cs:cnt+10]
        jmp     far dword [cs:old+20]
iv6:    inc     word [cs:cnt+12]
        jmp     far dword [cs:old+24]
; A spurious IRQ 7 gets no EOI: nothing is in service.
iv7:    push    ax
        mov     al, 0Bh
        out     20h, al
        jmp     short $+2
        in      al, 20h
        mov     ah, al
        mov     al, 0Ah
        out     20h, al
        test    ah, 80h
        pop     ax
        jnz     real7
        inc     word [cs:spur]
        iret
real7:  inc     word [cs:cnt+14]
        jmp     far dword [cs:old+28]

; BX -> word counter, incremented up to 65535
bump:   cmp     word [bx], 0FFFFh
        je      bumped
        inc     word [bx]
bumped: ret

; AL -> two hex digits at DI
hex:    push    ax
        shr     al, 4
        call    nib
        pop     ax
        and     al, 0Fh
nib:    add     al, '0'
        cmp     al, '9'
        jbe     putdig
        add     al, 7
putdig: mov     [di], al
        inc     di
        ret

; AX -> five decimal digits at DI, DI advanced past them
dec5:   push    cx
        push    dx
        push    bx
        add     di, 4
        mov     bx, 10
        mov     cx, 5
dloop:  xor     dx, dx
        div     bx
        add     dl, '0'
        mov     [di], dl
        dec     di
        loop    dloop
        add     di, 6
        pop     bx
        pop     dx
        pop     cx
        ret

pats    db      00h, 0FFh, 55h, 0AAh, 01h, 02h, 04h, 08h, 10h, 20h
        db      40h, 80h, 0FEh, 0FDh, 0FBh, 0F7h, 0EFh, 0DFh, 0BFh, 7Fh
stubtab dw      iv0, iv1, iv2, iv3, iv4, iv5, iv6, iv7
fname   db      'C:\PICCHK.TXT', 0
flip    db      0
imr     db      0
haveb   db      0
badp    db      0
badg    db      0
isrv    db      0
mskerr  dw      0
isrerr  dw      0
imrerr  dw      0
spur    dw      0
tick0   dw      0
ticks   dw      0
cnt     dw      8 dup 0
old     dd      8 dup 0

line    db      'PICCHK imr='
h_imr   db      '00 mask_err='
d_msk   db      '00000 first='
h_badp  db      '00/'
h_badg  db      '00 isr_err='
d_isr   db      '00000 isr='
h_isrv  db      '00 imr_err='
d_imr   db      '00000 irq0-7='
d_irq   db      '00000,00000,00000,00000,00000,00000,00000,00000 spur7='
d_spur  db      '00000 ticks='
d_tick  db      '00000 timeout='
tout    db      'N mode='
tchar   db      '-', 0Dh, 0Ah
line_len = $ - line
        db      '$'
v_ok    db      'All PIC reads matched; no spurious IRQ 7.', 0Dh, 0Ah, '$'
v_bad   db      'MISMATCH or spurious IRQ 7: send C:\PICCHK.TXT.', 0Dh, 0Ah, '$'
