; RSOAK.COM - does the planar's 64 KB hold its contents at a slower DRAM refresh?
;
; Refresh divisor 64 (PIT channel 1) frees 3-6% of the bus (docs/mach8_card_coverage.md) but
; refreshes the planar DRAM 3.5x less often than IBM's 18. The planar holds 0-64 KB here (the
; Inboard serves the rest), which is DOS, so it cannot be pattern-tested. Instead:
;   1. checksum each 1 KB block of 0000:0000-FFFF twice, 2 s apart, at divisor 18;
;      blocks that match are "static" (any 256 bytes covers every DRAM row)
;   2. control arm: divisor 18 for ARMT ticks, re-check the static blocks every STEP ticks
;   3. test arm: divisor 64, the same
;   4. divisor 18 back, always
; A change in a static block during the control arm is an ordinary write; decay shows as changes
; in the test arm that the control arm did not have. Writes RSOAK.BIN:
;   "RSK1", static[64], then per arm: checks (word), events (word), changed[64].

        org     100h
        cpu     386

ARMT            equ     5461            ; 5 minutes of 18.2 Hz ticks
STEP            equ     182             ; 10 s

        jmp     start

oname   db      'RSOAK.BIN', 0
m_ok    db      'RSOAK: done, divisor 18 restored, results in RSOAK.BIN', 13, 10, '$'
m_out   db      'RSOAK: cannot write RSOAK.BIN', 13, 10, '$'
hdr     db      'RSK1'
stmask  times 64 db 0
arm18   dw      0, 0
chg18   times 64 db 0
arm64   dw      0, 0
chg64   times 64 db 0
snapA   times 64 dd 0
snapB   times 64 dd 0
t0      dw      0
tnext   dw      0

start:  cld
        mov     al, 18
        call    setdiv
        mov     di, snapA
        call    snap
        mov     cx, 36                  ; ~2 s
        call    delay
        mov     di, snapB
        call    snap
        xor     ebx, ebx
.st:    mov     eax, [snapA + ebx*4]
        cmp     eax, [snapB + ebx*4]
        jne     .dyn
        mov     byte [stmask + bx], 1
.dyn:   inc     bx
        cmp     bx, 64
        jb      .st

        mov     al, 18
        mov     si, arm18
        call    arm
        mov     al, 64
        mov     si, arm64
        call    arm
        mov     al, 18
        call    setdiv

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 64 + 2 * (4 + 64)
        mov     dx, hdr
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; AL = divisor: PIT channel 1, LSB only, mode 2.
setdiv: push    ax
        mov     al, 54h
        out     43h, al
        pop     ax
        out     41h, al
        ret

; One arm. AL = divisor, SI -> {checks, events, changed[64]}.
; Base snapshot in snapB; every STEP ticks re-snapshot into snapA and compare static blocks.
arm:    call    setdiv
        mov     di, snapB
        call    snap
        call    ticks
        mov     [t0], ax
        add     ax, STEP
        mov     [tnext], ax
.loop:  call    ticks
        mov     dx, ax
        sub     dx, [t0]
        cmp     dx, ARMT
        jae     .done
        cmp     ax, [tnext]
        js      .loop                   ; signed: survives the 16-bit wrap of the tick word
        add     word [tnext], STEP
        mov     di, snapA
        call    snap
        inc     word [si]
        xor     ebx, ebx
.cmp:   cmp     byte [stmask + bx], 0
        je      .next
        mov     eax, [snapA + ebx*4]
        cmp     eax, [snapB + ebx*4]
        je      .next
        inc     word [si + 2]
        mov     byte [si + 4 + bx], 1
        mov     [snapB + ebx*4], eax     ; count each change once
.next:  inc     bx
        cmp     bx, 64
        jb      .cmp
        jmp     .loop
.done:  ret

; AX = low word of the BIOS tick count (0040:006C).
ticks:  push    es
        push    word 40h
        pop     es
        mov     ax, [es:6Ch]
        pop     es
        ret

; CX ticks.
delay:   call    ticks
        mov     dx, ax
.w:     call    ticks
        sub     ax, dx
        cmp     ax, cx
        jb      .w
        ret

; 64 checksums of 0000:0000-FFFF, 1 KB each, into [di] (dwords).
snap:   push    si
        push    ds
        push    di
        xor     ax, ax
        mov     ds, ax
        xor     si, si
        mov     bp, 64
.blk:   xor     edx, edx
        mov     cx, 256
.w4:    lodsd
        rol     edx, 1
        add     edx, eax
        loop    .w4
        mov     [cs:di], edx
        add     di, 4
        dec     bp
        jnz     .blk
        pop     di
        pop     ds
        pop     si
        ret
