; FD08FIX.COM - route INT 13h AH=08h for floppies to INT 40h
;
; WHY THIS EXISTS  (issue #25, root-caused 2026-09-07)
;
; On this machine INT 13h AH=08h (Get Drive Parameters) answers "type 3,
; 720K, 80 cyl / 9 sect" for BOTH floppy drives. The drives are a 1.44 MB
; 3.5" (A:) and a 1.2 MB 5.25" (B:).
;
; The machine already knows the right answer. Measured on the real 5160,
; same call, same boot, both vectors:
;
;     INT 13h  A: BL=03 CX=4F09      B: BL=03 CX=4F09     <- wrong, both
;     INT 40h  A: BL=04 CX=4F12      B: BL=02 CX=4F0F     <- CORRECT
;
; INT 40h is Sergey Kiselev's Multi-Floppy BIOS at D000:10D6, and its
; EEPROM is configured correctly. It sits on INT 40h rather than INT 13h
; because its own install rule takes INT 13h only when INT 13h is still
; the stock IBM diskette handler at F000:EC59, and a fixed-disk option ROM
; had already claimed it.
;
; That is the documented arrangement, not a fault. AMIBIOS 98 Technical
; Reference, p.138, "INT 40h Revector for Floppy Functions": when a hard
; disk is present the floppy service resides at INT 40h and ALL BIOS
; floppy functions are revectored there. Something in the INT 13h chain
; answers AH=08h itself instead of forwarding, and this restores the
; forwarding for that one function.
;
; It forwards to measured-correct data. It does not synthesise geometry.
;
; SCOPE - deliberately one function
;   AH=08h and DL<80h  -> reissued on INT 40h, its result returned
;   everything else    -> chained untouched, including all hard disk I/O
;
; Reads and writes are NOT touched. They already work: DOS reads 1.44 MB
; disks, which a 1986 XT BIOS cannot do, so AH=02h/03h demonstrably reach
; Sergey's ROM already.
;
; DEPLOY
;   Last line of AUTOEXEC.BAT, after IVT68FIX.COM. Technique 38: that
;   timing is load-bearing for IVT68FIX and this is the same class of fix.
;
; WARNING - IOS SAFELIST
;   A resident INT 13h hook makes the Windows 95 I/O Supervisor refuse
;   every miniport unless it is listed. See docs/ios_safelist_howto.md.
;   XTIDEMP.MPD (#21) is in the blast radius. On the first boot after
;   deploying, confirm in BOOTLOG.TXT:
;       Init Success xtidemp.mpd
;       rmm.pdr loads and NEVER reaches INITCOMPLETE
;   If RMM reaches INITCOMPLETE, IOS has punted and C: is back on the
;   real-mode mapper - remove this and fix the SafeList entry first.

        org     100h

; ---------------------------------------------------------------- resident
start:
handler:
        cmp     ah, 08h                 ; Get Drive Parameters?
        jne     chain
        test    dl, 80h                 ; floppy? (ZF set => bit clear => floppy)
        jnz     chain                   ; hard disk - not ours
        int     40h                     ; reissue on the diskette vector.
                                        ; returns with AH/CX/DX/CF set.
;
; Return via IRET, patching only CF into the caller's stacked FLAGS.
;
; NOT `retf 2`. That was the first version and it HUNG THE MACHINE AT BOOT
; on 2026-09-07. An INT clears IF, and IRET is what restores it from the
; stacked FLAGS. `retf 2` discards those flags, so we would return with
; interrupts still disabled - no timer, no keyboard - the moment anything
; asked AH=08h during boot. Registers are untouched here, so AH/CX/DX/ES:DI
; from INT 40h reach the caller as they are.
;
        push    bp
        mov     bp, sp                  ; [bp+0]=BP [bp+2]=IP [bp+4]=CS [bp+6]=FLAGS
        jc      .setcf
        and     word [bp+6], 0FFFEh     ; CF clear -> success
        jmp     short .done
.setcf:
        or      word [bp+6], 1          ; CF set -> error
.done:
        pop     bp
        iret
chain:
        jmp     far [cs:old13]

old13:  dw      0                       ; offset of the previous INT 13h
        dw      0                       ; segment

resident_end:

; --------------------------------------------------------------- installer
install:
        mov     ax, 3513h               ; DOS get interrupt vector, INT 13h
        int     21h                     ; -> ES:BX
        mov     [old13], bx
        mov     [old13+2], es

        mov     ax, 2513h               ; DOS set interrupt vector, INT 13h
        mov     dx, handler
        int     21h

        mov     ah, 09h                 ; announce - proving it loaded is
        mov     dx, msg                 ; cheaper than wondering later
        int     21h

        mov     dx, (resident_end - $$ + 100h + 15) / 16  ; paragraphs incl. PSP
        mov     ax, 3100h               ; terminate and stay resident
        int     21h

msg:    db      'FD08FIX: INT 13h AH=08h floppy -> INT 40h', 0Dh, 0Ah, '$'
