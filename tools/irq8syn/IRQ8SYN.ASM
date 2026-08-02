; IRQ8SYN.ASM -- software RTC-periodic-interrupt (IRQ8/INT 70h) synthesizer
; for Inboard 386/PC-on-XT systems with no real MC146818 RTC and hence no
; real IRQ8 line at all.
;
; Chains onto the REAL INT 08h (timer tick, IRQ0, ~18.2 Hz, driven by the
; genuine 8253/8254 PIT already present and untouched on every XT/Inboard
; board) and fires a software INT 70h every DIVISOR ticks -- simulating
; the RTC "update-ended" interrupt a real MC146818 issues once a second,
; the RTC signal most commonly depended upon. No new hardware needed: this
; just re-uses the tick that's already firing.
;
; Portable by design: doesn't assume any add-on RTC chip (SmartWatch or
; otherwise) -- works on any Inboard/XT box purely from the existing PIT.
;
; STATUS CHECK: running IRQ8SYN.COM again does NOT reinstall a second
; copy. It queries the resident instance via an INT 2Fh multiplex check
; (MULTIPLEX_ID below); if already installed, it prints the number of
; ticks recorded so far and exits normally (not resident) -- this is the
; way to confirm from the DOS prompt whether the hook survived something
; (e.g. a Windows session) without needing any other tool.
;
; Load order: after the real INT 08h owner is stable (INBRDPC.SYS/
; REVTO486.SYS/386MAX already loaded), before WIN. Simple .COM TSR --
; run once from AUTOEXEC.BAT, not a CONFIG.SYS device driver.
;
; Assemble: wasm IRQ8SYN.ASM
; Link:     wlink FORMAT DOS COM FILE IRQ8SYN.OBJ NAME IRQ8SYN.COM
;           (then strip the first 256 bytes -- see build notes in
;           WIN95_PLAN.md: wlink adds an erroneous extra 256-byte pad
;           when linking an ORG 100h source to COM format.)

        .8086
        .MODEL TINY
        .CODE
        ORG 100h

DIVISOR      EQU 18      ; IRQ0 ticks (18.2 Hz) per synthesized INT 70h
                          ; (18 ticks =~ 0.989s -- once-per-second cadence,
                          ; matching a real RTC's update-ended interrupt)
MULTIPLEX_ID EQU 0F8h    ; INT 2Fh multiplex id for the install/status check

start:
        jmp     install

; ---------------------------------------------------------------------
; resident portion
; ---------------------------------------------------------------------
old_int08       dd      0
old_int2f       dd      0
tick_count      db      DIVISOR
tick_total      dw      0       ; persistent count of synthesized INT 70h events

new_int08       proc    far
        push    ax

        dec     byte ptr cs:[tick_count]
        jnz     chain08

        mov     byte ptr cs:[tick_count], DIVISOR
        inc     word ptr cs:[tick_total]
        int     70h             ; synthesize the RTC periodic tick

chain08:
        pop     ax
        jmp     dword ptr cs:[old_int08]   ; chain: original ISR does EOI+iret
new_int08       endp

; INT 2Fh multiplex handler: AH=MULTIPLEX_ID, AL=0 -> "are you installed?"
; Answers AL=0FFh (installed) + BX=tick_total, without chaining -- any
; other AH/AL passes straight through to whatever was hooked before us.
new_int2f       proc    far
        cmp     ah, MULTIPLEX_ID
        jne     chain2f
        cmp     al, 0
        jne     chain2f

        mov     al, 0FFh
        mov     bx, cs:[tick_total]
        iret

chain2f:
        jmp     dword ptr cs:[old_int2f]
new_int2f       endp

; INT 70h: on real AT hardware this is the slave-PIC/RTC vector; on this
; system there is no real MC146818 and no real slave PIC, so nothing
; could ever have legitimately set this vector up. DON'T trust whatever
; happens to be sitting there at install time (BIOS/DOS make no promise
; of a safe default for an IRQ8-15 vector on hardware that never has
; those interrupts) -- install our own trivial safe stub instead, so
; new_int08's "int 70h" always lands somewhere sane regardless of boot
; order or what else has (or hasn't) run before us.
new_int70       proc    far
        iret
new_int70       endp

resident_end    label   byte

; ---------------------------------------------------------------------
; install / status-query portion (discarded once TSR is resident)
; ---------------------------------------------------------------------
install:
        mov     ax, ( MULTIPLEX_ID shl 8 )   ; AH=MULTIPLEX_ID, AL=0
        int     2Fh
        cmp     al, 0FFh
        je      already_installed

        mov     dx, offset banner
        mov     ah, 9
        int     21h

        mov     ah, 35h                 ; get current INT 08h vector
        mov     al, 08h
        int     21h
        mov     word ptr [old_int08], bx
        mov     word ptr [old_int08+2], es

        mov     ah, 35h                 ; get current INT 2Fh vector
        mov     al, 2Fh
        int     21h
        mov     word ptr [old_int2f], bx
        mov     word ptr [old_int2f+2], es

        mov     dx, offset new_int08     ; hook INT 08h
        mov     ax, 2508h
        int     21h

        mov     dx, offset new_int2f     ; hook INT 2Fh
        mov     ax, 252Fh
        int     21h

        mov     dx, offset new_int70     ; install our own safe INT 70h stub
        mov     ax, 2570h                ; (no save/chain -- see comment above:
        int     21h                      ;  nothing legitimate could already be there)

        mov     dx, offset resident_end  ; bytes to keep resident (from PSP)
        int     27h                      ; terminate & stay resident

already_installed:
        push    bx                       ; bx = tick_total from resident copy

        mov     dx, offset status_prefix
        mov     ah, 9
        int     21h

        pop     ax
        call    print_decimal

        mov     dx, offset status_suffix
        mov     ah, 9
        int     21h

        mov     ax, 4C00h                ; normal terminate -- NOT resident
        int     21h

; print AX as unsigned decimal, no leading zeros
print_decimal   proc    near
        push    ax
        push    bx
        push    cx
        push    dx

        mov     bx, 10
        xor     cx, cx
pd_divide:
        xor     dx, dx
        div     bx
        push    dx
        inc     cx
        test    ax, ax
        jnz     pd_divide

pd_print:
        pop     dx
        add     dl, '0'
        mov     ah, 2
        int     21h
        loop    pd_print

        pop     dx
        pop     cx
        pop     bx
        pop     ax
        ret
print_decimal   endp

banner          db      'IRQ8SYN: software RTC tick installed (INT 08h -> '
                db      'INT 70h, 1/sec)', 13, 10, '$'
status_prefix   db      'IRQ8SYN: already resident -- ticks recorded: $'
status_suffix   db      13, 10, '$'

        END     start
