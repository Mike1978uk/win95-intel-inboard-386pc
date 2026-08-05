BITS 16
ORG 0x100

; ============================================================================
; test_ah87_repeat.COM - minimal reproduction test for the OSR1 INBRDPC.SYS
; investigation (2026-08-04).
;
; PURPOSE
; -------
; Boots to a bare DOS prompt (CONFIG.SYS loading only INBRDPC.SYS, no
; HIMEM.SYS/Windows at all) and calls INT 15h AH=87h (Extended Memory Block
; Move) N times in a row with a trivial, safe LOCAL->LOCAL 2-byte move (no
; real extended-memory access needed - this only tests whether the CALL
; ITSELF returns promptly or hangs/slows on repeated invocation).
;
; This exists because a live [ah87caller] trace hook (386_dynarec.c,
; CS:PC=0206:044E, INBRDPC.SYS's actual INT15h AH=87h/88h dispatcher entry)
; showed ZERO hits during 200+ seconds of the OSR1 XT+Inboard patched-VxD
; boot test stalling with CS:PC cycling through INBRDPC.SYS's self-test-and-
; report code (rt=0x6DE-0x774, reached via port 0x670 "housekeeping" writes)
; - meaning whatever's cycling through that code is NOT reached via external
; callers going through the normal INT 15h vector. This program directly
; tests the "normal" external-caller path in isolation, fast (seconds, not
; minutes) and reproducibly, without needing a full Windows boot each time -
; replacing the ad-hoc live-hook/full-boot investigation with a checked-in,
; rerunnable test.
;
; GDT LAYOUT: reused verbatim from inbrdpc_patch_work/int15_shim.asm's
; probe_extended_memory routine (2026-08-03), itself live-captured from
; genuine, working HIMEM.SYS AH=87h calls on a validated AT profile - not
; guessed. descriptor 2 = source, descriptor 3 = destination, both pointing
; at local scratch words in this same segment (physical address computed at
; runtime from CS, exactly as the reference code does).
;
; USAGE: assemble with NASM (test_harness/inbrdpc/nasm.exe or the copy in
; vm_win311/inbrdpc_patch_work/nasm-2.16.03/), deploy as C:\T87.COM on a
; CONFIG.SYS-only-INBRDPC.SYS boot, run from the DOS prompt.
; ============================================================================

ITERATIONS equ 10

start:
    mov ax, cs
    mov ds, ax
    mov es, ax

    mov dx, msg_banner
    call print_str

    ; one-time: compute physical base of this segment (cs*16) for patching
    ; the GDT descriptors' base fields, same technique as int15_shim.asm
    mov ax, cs
    mov dx, 16
    mul dx                      ; dx:ax = cs*16
    mov word [phys_lo], ax
    mov byte [phys_hi], dl

    mov ax, word [phys_lo]
    add ax, src_word
    mov dl, byte [phys_hi]
    adc dl, 0
    mov word [gdt_desc2 + 2], ax
    mov byte [gdt_desc2 + 4], dl

    mov ax, word [phys_lo]
    add ax, dst_word
    mov dl, byte [phys_hi]
    adc dl, 0
    mov word [gdt_desc3 + 2], ax
    mov byte [gdt_desc3 + 4], dl

    mov byte [iter_count], ITERATIONS

test_loop:
    mov dx, msg_calling
    call print_str
    mov al, byte [iter_count]
    call print_hex_byte
    mov dx, msg_nl
    call print_str

    mov word [src_word], 0x5AA5   ; distinct test pattern each call would be
    mov word [dst_word], 0        ; nicer, but a fixed pattern is fine here -
                                   ; this test is about hang/no-hang, not data
                                   ; integrity (int15_shim.asm already proved
                                   ; the move+verify logic works).

    mov ax, cs
    mov es, ax
    mov si, gdt
    mov cx, 1
    mov ah, 0x87
    int 0x15

    jc short call_failed
    mov dx, msg_ok
    call print_str
    jmp short next_iter

call_failed:
    mov dx, msg_fail
    call print_str
    call print_hex_byte         ; AH = BIOS error code on failure

next_iter:
    dec byte [iter_count]
    jnz test_loop

    mov dx, msg_done
    call print_str

    mov ax, 0x4c00
    int 0x21

; ----------------------------------------------------------------------------
; print_str: DS:DX -> '$'-terminated string, via INT 21h AH=09h
; ----------------------------------------------------------------------------
print_str:
    push ax
    mov ah, 0x09
    int 0x21
    pop ax
    ret

; ----------------------------------------------------------------------------
; print_hex_byte: prints AL as 2 hex digits
; ----------------------------------------------------------------------------
print_hex_byte:
    push ax
    push bx
    push dx
    mov bl, al
    mov al, bl
    shr al, 4
    call print_nibble
    mov al, bl
    and al, 0x0f
    call print_nibble
    pop dx
    pop bx
    pop ax
    ret

print_nibble:
    cmp al, 10
    jb short digit
    add al, 'A' - 10 - '0'
digit:
    add al, '0'
    mov dl, al
    push ax
    mov ah, 0x02
    int 0x21
    pop ax
    ret

; ----------------------------------------------------------------------------
; data
; ----------------------------------------------------------------------------
msg_banner:  db 'test_ah87_repeat: calling INT15h AH=87h ', ITERATIONS + '0', ' times', 13, 10, '$'
msg_calling: db 'call #$'
msg_ok:      db ' -> OK', 13, 10, '$'
msg_fail:    db ' -> FAIL, AH=$'
msg_nl:      db '$'
msg_done:    db 'done.', 13, 10, '$'

iter_count:  db 0
phys_lo:     dw 0
phys_hi:     db 0

src_word:    dw 0
dst_word:    dw 0

gdt:
    dw 0, 0, 0, 0
    dw 0, 0, 0, 0
gdt_desc2:                       ; source
    dw 0xffff, 0
    db 0, 0x93, 0, 0
gdt_desc3:                       ; destination
    dw 0xffff, 0
    db 0, 0x93, 0, 0
    dw 0, 0, 0, 0
    dw 0, 0, 0, 0
