; IVT68FIX.COM - real-hardware translation of the emulator's [patchint68] fix
; (see 86box_full/src/cpu/386_dynarec.c and docs/windows95_on_inboard386pc_writeup.md,
; section 3, for the full root-cause writeup).
;
; Windows 95's OSR1 real-mode loader (segment 0EAF on this specific install) executes
; INT 68h - a private, non-standard multiplex call - whose IVT vector (offset 0x68*4 =
; 0x1A0) is never initialized by anything earlier in the boot. Real x86 hardware, faced
; with an uninitialized vector, faithfully jumps to whatever garbage bytes sit at that
; address (0000:0000, the start of the IVT table itself) and starts executing them as
; code - an "accidental sled" that happens to land on a real CALL FAR opcode elsewhere
; in the table, jumping into unrelated real code (the floppy driver, relocated by
; IO.SYS) with a completely bogus calling context.
;
; Fix: give INT 68h's vector a harmless, real destination - a single IRET, so the call
; behaves as a clean no-op instead of derailing into garbage. This program does exactly
; the same 5 physical-memory writes as the emulator's own [patchint68] debug hook
; (386_dynarec.c), by hand, from real mode, so it works with no emulator involved.
;
; Timing matters as much as the bytes: firing this at DOS boot start was already proven
; (in the emulator) to get clobbered before INT 68h ever runs - ordinary BIOS POST and
; DOS's own low-memory init rewrite this region first. Only firing it as late as
; possible survived. On real DOS there's no CPU hook to time this precisely, so this is
; called from the very LAST line of AUTOEXEC.BAT, as close as real DOS boot gets to the
; emulator's proven-working timing (immediately before WIN.COM auto-launches).
;
; Assemble: nasm -f bin IVT68FIX.ASM -o IVT68FIX.COM   (26 bytes)

BITS 16
ORG 0x100

start:
    db      0x33, 0xC0                 ; xor ax,ax - pinned to this exact encoding (rather than
                                        ; NASM's default 31 C0, functionally identical) so this
                                        ; source reassembles byte-for-byte to the binary that was
                                        ; actually hand-assembled, deployed, and tested working
    mov     es, ax                     ; ES = 0000 (real-mode segment 0, for absolute IVT addressing)

    mov     byte [es:0x03C0], 0xCF     ; write IRET (0xCF) at 0000:03C0 - borrowed scratch space,
                                        ; the otherwise-unused offset-low byte of INT F0h's own
                                        ; vector slot (nothing on this boot ever calls INT F0h)

    mov     word [es:0x01A0], 0x03C0   ; INT 68h vector offset  -> 0x03C0
    mov     word [es:0x01A2], 0x0000   ; INT 68h vector segment -> 0x0000

    int     0x20                       ; terminate, TSR not needed - the IVT write persists
