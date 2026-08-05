BITS 16

; ============================================================================
; INBRDPC.SYS INT 15h AH=88h BIOS-shim patch
;
; Real IBM 5160 XT BIOS predates the AT convention where INT 15h AH=88h
; ("Get Extended Memory Size") exists at all - real XT BIOS's INT 15h only
; ever handles cassette I/O (AH=0-2) and does a bare flags-unchanged IRET for
; anything else. Any DOS/Windows component that calls AH=88h and trusts
; "carry clear = success" gets misled by whatever garbage was already in AX,
; not a real answer. This patch makes INBRDPC.SYS answer that call correctly
; itself, since it's the one resident driver that's actually specific to this
; hardware.
;
; The extended-memory value is determined by GENUINE RUNTIME PROBING (write/
; readback test walking 64KB blocks from 1MB upward via INT 15h AH=87h's
; BIOS block-move service), NOT hardcoded or config-file-based - this works
; identically on the real 5160+Inboard and on any 86Box config, matching this
; project's hardware-fidelity priority.
;
; The GDT descriptor layout used here is not guessed - it's the exact format
; live-captured (2026-08-03, [int1587gdt] hook) from Microsoft's own stock
; HIMEM.SYS calling this same BIOS service on a genuine, working AT profile:
;   descriptor 0,1,4,5: unused, all zero
;   descriptor 2 (source):      limit=FFFF base_low base_mid access=93 rsv=00 base_high
;   descriptor 3 (destination): limit=FFFF base_low base_mid access=93 rsv=00 base_high
; base = base_high:base_mid:base_low, a 24-bit physical address (the real,
; historical 80286-era ceiling for this BIOS service - this driver only
; needs to stay well under it since no configuration of this hardware
; approaches 16MB).
;
; Appended to INBRDPC.SYS starting at file/load offset 0xC6A3 (the current
; end of the file, right after the existing INT06h-fix append at
; 0xC695-0xC6A2). Flat real-mode .SYS driver: file offset == load offset,
; no relocation, so all internal references here are ordinary label offsets
; assembled by NASM with ORG 0xC6A3.
;
; INIT-hook insertion: replaces the 3-byte "MOV DX,0x670" at file offset
; 0xA701 (INIT's very first instruction) with a same-length "CALL init_detour"
; (also 3 bytes) so no other offset in the file shifts. init_detour replicates
; the original instruction, then calls the hook installer, then returns to
; 0xA704 exactly where the original flow would have continued.
; ============================================================================

ORG 0xC6A3

; ---------------------------------------------------------------------------
; init_detour: same-length replacement for "CALL 0xA6DE" at file offset
; 0xA731 (NOT INIT's first instruction - deliberately placed after the
; self-test-table loop at 0xA70B-0xA722, and after whatever residency/
; duplicate-load check INBRDPC.SYS's own INIT does near its start. Live
; testing (2026-08-03) showed installing the hook at the very first
; instruction made INBRDPC.SYS immediately report "There's more than one
; DEVICE=iNBRDPC.SYS command" on a genuinely single, fresh load - consistent
; with a residency check that inspects INT 15h's current vector and seeing
; it already pointing into this driver's own segment because the hook had
; just installed it moments earlier. Moving the hook-install past that point
; avoids the false positive without needing to prove the exact mechanism.
; ---------------------------------------------------------------------------
init_detour:
    call 0xa6de             ; replicate the original instruction we replaced
    call install_int15_hook
    ret                     ; returns to 0xA734, right after the original instr

; ---------------------------------------------------------------------------
; install_int15_hook: save the current INT 15h vector, install our own.
; ---------------------------------------------------------------------------
install_int15_hook:
    push ax
    push bx
    push es
    xor ax, ax
    mov es, ax
    cli
    mov ax, word [es:0x54]
    mov bx, word [es:0x56]
    mov word [cs:old_int15_off], ax
    mov word [cs:old_int15_seg], bx
    mov word [es:0x54], new_int15_handler
    mov word [es:0x56], cs
    sti
    pop es
    pop bx
    pop ax
    ret

; ---------------------------------------------------------------------------
; new_int15_handler: services AH=88h ourselves (lazy-probing on first call,
; caching the result), chains everything else to the original vector
; untouched - the interrupt frame (flags/return CS:IP) on the stack was
; pushed by the original INT instruction and is exactly what the old
; handler's own IRET expects, so a plain far jump correctly preserves it.
; ---------------------------------------------------------------------------
new_int15_handler:
    cmp ah, 0x88
    je short do_ah88
    jmp dword [cs:old_int15_off]

do_ah88:
    push bx
    push cx
    push dx
    push si
    push di
    push ds
    push es
    mov bx, cs
    mov ds, bx
    cmp word [probed_flag], 0
    jne short have_result
    call probe_extended_memory
    mov word [probed_flag], 1
have_result:
    mov ax, word [ext_mem_kb]
    pop es
    pop ds
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ; NOTE: clc here would only clear the LIVE eflags register, which IRET
    ; then immediately discards in favour of the FLAGS word already sitting
    ; on the stack (pushed by the original INT instruction, at [bp+6] below) -
    ; that stacked copy is what the caller actually sees, so it must be
    ; cleared directly, not the live register.
    push bp
    mov bp, sp
    and word [bp + 6], 0xfffe        ; clear carry bit in the stacked FLAGS
    pop bp
    iret

; ---------------------------------------------------------------------------
; probe_extended_memory: walk 64KB blocks above 1MB, write/readback-verify
; each one via INT 15h AH=87h, stop at first failure or the 240-block
; (~15MB) cap. Leaves the result (KB above 1MB) in ext_mem_kb and AX.
; Assumes DS=CS on entry (new_int15_handler sets this up before calling).
; ---------------------------------------------------------------------------
probe_extended_memory:
    push bx
    push cx
    push dx
    push si
    push di
    push es

    ; one-time: compute the physical base of our two local scratch words,
    ; since they don't move during the loop (only the extended candidate does)
    mov ax, cs
    mov dx, 16
    mul dx                       ; dx:ax = cs*16 (dx stays small - a real-mode
                                  ; driver segment is always well under 1MB)
    mov bx, ax
    mov cx, dx

    mov ax, bx
    add ax, probe_src_data
    mov dl, cl
    adc dl, 0
    mov word [write_gdt_desc2 + 2], ax
    mov byte [write_gdt_desc2 + 4], dl
    mov byte [write_gdt_desc2 + 7], 0

    mov ax, bx
    add ax, probe_readback
    mov dl, cl
    adc dl, 0
    mov word [read_gdt_desc3 + 2], ax
    mov byte [read_gdt_desc3 + 4], dl
    mov byte [read_gdt_desc3 + 7], 0

    xor di, di                   ; di = verified 64KB-block count so far

probe_next_block:
    cmp di, 0xf0                 ; cap at 240 blocks (~15MB above 1MB),
    jae probe_done                ; stays well under the 16MB / 24-bit ceiling

    ; distinct-per-block test pattern - avoids an all-0/all-1 false positive
    ; from a stuck or floating bus
    mov ax, di
    xor ax, 0x5aa5
    mov word [probe_src_data], ax
    mov word [probe_readback], 0

    ; this block's candidate physical base = (0x10+di) << 16 - fits in one
    ; byte since di < 0xf0
    mov ax, di
    add ax, 0x10
    mov byte [write_gdt_desc3 + 4], al   ; write-table dest = extended candidate
    mov byte [read_gdt_desc2 + 4], al    ; read-table  source = same candidate

    ; ---- move 1: probe_src_data (local) -> extended candidate ----
    mov ax, cs
    mov es, ax
    mov si, write_gdt
    mov cx, 1
    mov ah, 0x87
    int 0x15
    jc probe_done                 ; BIOS itself reports failure - stop here

    ; ---- move 2: extended candidate -> probe_readback (local) ----
    mov ax, cs
    mov es, ax
    mov si, read_gdt
    mov cx, 1
    mov ah, 0x87
    int 0x15
    jc probe_done

    ; ---- verify ----
    mov ax, word [probe_readback]
    cmp ax, word [probe_src_data]
    jne probe_done

    inc di
    jmp probe_next_block

probe_done:
    mov ax, di
    mov cx, 64
    mul cx                        ; ax = di*64 = KB above 1MB (di<240 so this
                                   ; fits in 16 bits with dx=0)
    mov word [ext_mem_kb], ax

    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

; ---------------------------------------------------------------------------
; data
; ---------------------------------------------------------------------------
old_int15_off:  dw 0
old_int15_seg:  dw 0
probed_flag:    dw 0
ext_mem_kb:     dw 0
probe_src_data: dw 0
probe_readback: dw 0

; write direction: descriptor 2 = local (computed at runtime),
;                  descriptor 3 = extended candidate (base_mid patched per block)
write_gdt:
    dw 0, 0, 0, 0
    dw 0, 0, 0, 0
write_gdt_desc2:
    dw 0xffff, 0
    db 0, 0x93, 0, 0
write_gdt_desc3:
    dw 0xffff, 0
    db 0, 0x93, 0, 0
    dw 0, 0, 0, 0
    dw 0, 0, 0, 0

; read direction: descriptor 2 = extended candidate (base_mid patched per block),
;                 descriptor 3 = local (computed at runtime)
read_gdt:
    dw 0, 0, 0, 0
    dw 0, 0, 0, 0
read_gdt_desc2:
    dw 0xffff, 0
    db 0, 0x93, 0, 0
read_gdt_desc3:
    dw 0xffff, 0
    db 0, 0x93, 0, 0
    dw 0, 0, 0, 0
    dw 0, 0, 0, 0
