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
; the same physical-memory writes as the emulator's own [patchint68] hook
; (386_dynarec.c), by hand, from real mode, so it works with no emulator involved.
;
; 2026-08-23 - point the vector at the BIOS's own IRET at F000:FF53, instead of writing
; a 0xCF stub into INT F0h's vector slot (0000:03C0) and pointing there. Suggested by
; Michal Necasek on 86Box PR #7749 and verified against the ROM images: the byte at file
; offset 0x7F53 of the U18/F800 chip is 0xCF (IRET) in BOTH 1986 revisions, 09MAY86
; (59X7268/62X0890) and 10JAN86 (62X0851) - the only BIOSes the Inboard accepts, since
; the 1982 ROMs fail INBRDPC.SYS's signature check at F000:E05B. Strictly better: this
; version injects no code at all and drops the assumption that INT F0h is unused. The
; program shrinks from 26 to 20 bytes and from 5 memory writes to 4.
;
; This file and the emulator's [patchint68] hook MUST be kept in step - they are the same
; fix on two targets. If one changes, change the other.
;
; Timing matters as much as the bytes: firing this at DOS boot start was already proven
; (in the emulator) to get clobbered before INT 68h ever runs - ordinary BIOS POST and
; DOS's own low-memory init rewrite this region first. Only firing it as late as
; possible survived. On real DOS there's no CPU hook to time this precisely, so this is
; called from the very LAST line of AUTOEXEC.BAT, as close as real DOS boot gets to the
; emulator's proven-working timing (immediately before WIN.COM auto-launches).
;
; Assemble: nasm -f bin IVT68FIX.ASM -o IVT68FIX.COM   (20 bytes)
;
; NOTE: no assembler was available on the build host for the 2026-08-23 revision, so the
; binary was hand-assembled and verified by disassembly (capstone, 16-bit mode). Both
; instructions below reuse encodings byte-for-byte from the previously deployed and
; hardware-tested 26-byte build; only the two 16-bit immediates differ.

BITS 16
ORG 0x100

start:
    db      0x33, 0xC0                 ; xor ax,ax - pinned to this exact encoding (rather than
                                        ; NASM's default 31 C0, functionally identical) so this
                                        ; source reassembles byte-for-byte to the binary that was
                                        ; actually deployed and tested working
    mov     es, ax                     ; ES = 0000 (real-mode segment 0, for absolute IVT addressing)

    mov     word [es:0x01A0], 0xFF53   ; INT 68h vector offset  -> 0xFF53
    mov     word [es:0x01A2], 0xF000   ; INT 68h vector segment -> 0xF000 (BIOS's own IRET)

    int     0x20                       ; terminate, TSR not needed - the IVT write persists
