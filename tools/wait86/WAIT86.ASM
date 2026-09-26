; WAIT86.COM - INT 15h AH=86h (wait CX:DX microseconds) for the IBM 5160 BIOS (#7).
;
; The 1986 XT BIOS diskette code (U19, F000:0976-09AF) asks INT 15h AH=86h to
; wait out the drive's motor-start time. The XT BIOS does not implement 86h, so the
; call fails and the BIOS falls back to counting DRAM-refresh ticks on DMA channel 0
; (F000:ECA0), with interrupts off. Under Windows 95, VDMAD traps port 00h, the count
; never moves, and the System VM hangs inside the floppy call: Setup's #7 stall.
;
; With this resident, AH=86h succeeds, so the BIOS never reaches the refresh loop.
; It waits on the BIOS tick count at 0040:006C, which Windows keeps advancing in every
; VM. Each tick wait is bounded, so a stopped timer shortens the wait, never hangs it.
; Every other INT 15h call is passed to the previous handler unchanged.
;
; Load from AUTOEXEC.BAT, before Windows starts.
;
; Assemble: nasm -f bin WAIT86.ASM -o WAIT86.COM

        org     100h

start:  jmp     install

sig     db      'WAIT86'
old15   dd      0

int15:  cmp     ah, 86h
        je      wait86
        jmp     far [cs:old15]

wait86: push    ax
        push    bx
        push    cx
        push    dx
        push    si
        push    ds

        ; ticks = ceil(CX:DX / 54925), one BIOS tick being 54925 us
        mov     ax, dx
        mov     dx, cx
        add     ax, 54924
        adc     dx, 0
        mov     bx, 54925
        cmp     dx, bx
        jb      .div
        mov     ax, 0FFFFh              ; quotient would overflow: wait the maximum
        jmp     short .have
.div:   div     bx
.have:  or      ax, ax
        jz      .done
        mov     cx, ax
        inc     cx                      ; the first tick edge is a partial tick

        xor     bx, bx
        mov     ds, bx
        sti
.tick:  mov     bx, [046Ch]
        mov     si, 64                  ; bound: 64 x 65536 reads per tick edge
.outer: xor     dx, dx
.spin:  cmp     bx, [046Ch]
        jne     .edge
        dec     dx
        jnz     .spin
        dec     si
        jnz     .outer
.edge:  loop    .tick

.done:  pop     ds
        pop     si
        pop     dx
        pop     cx
        pop     bx
        pop     ax
        push    bp                      ; clear CF in the caller's FLAGS, then IRET,
        mov     bp, sp                  ; so the caller's IF is restored too
        and     word [bp+6], 0FFFEh
        pop     bp
        iret

resident_end:

install:
        mov     ax, 3515h               ; ES:BX = current INT 15h vector
        int     21h
        mov     di, bx
        add     di, sig - int15         ; would-be signature if our handler is there
        cmp     bx, int15
        jne     .fresh
        mov     si, sig
        mov     cx, 6
        repe    cmpsb
        je      .already
.fresh: mov     [old15], bx
        mov     [old15+2], es
        mov     dx, int15
        mov     ax, 2515h
        int     21h

        mov     es, [2Ch]               ; free the environment copy
        mov     ah, 49h
        int     21h

        mov     dx, msg_ok
        mov     ah, 09h
        int     21h
        mov     dx, (resident_end - start + 100h + 15) / 16
        mov     ax, 3100h
        int     21h

.already:
        mov     dx, msg_dup
        mov     ah, 09h
        int     21h
        mov     ax, 4C00h
        int     21h

msg_ok  db      'WAIT86: INT 15h AH=86h wait installed', 13, 10, '$'
msg_dup db      'WAIT86: already installed', 13, 10, '$'
