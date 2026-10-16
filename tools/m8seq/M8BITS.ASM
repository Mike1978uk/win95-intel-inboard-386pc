; M8BITS.COM - the 28800 extended-register bits no ATI code was found to use (docs/mach8_levers.md): A0h bits 0-3,
; ADh bit 4, AFh and BCh all bits. Owner's call, 2026-10-16: try each once.
;
; Text mode as found. A baseline record first, then per bit: read v0; write v0 XOR mask; read v1 (does it stick);
; time 256 word reads of B800:0000 (a bus or wait-state change); time 32 changes of the line counter A9h (a clock
; change); write v0 back; read v2. Each record is appended to M8BITS.BIN and the file closed before the next bit,
; so if the machine hangs the last record shows the bit before the one that hung it.
; Record (12 bytes): index, mask, v0, v1, v2, 0, mem ticks (word), line ticks (word), 2 spare. File starts "M8B1".
; Ticks are raw PIT channel 0 differences, as M8TIME; a line count of FFFFh means the counter stopped (timeout).

        org     100h

        jmp     start

oname   db      'M8BITS.BIN', 0
m_ok    db      'M8BITS: done, results in M8BITS.BIN', 13, 10, '$'
m_out   db      'M8BITS: cannot write M8BITS.BIN', 13, 10, '$'
hdr     db      'M8B1'
rec     times 12 db 0
t0      dw      0
; index, mask; index 0 = baseline (no write)
list    db      0, 0
        db      0A0h, 01h, 0A0h, 02h, 0A0h, 04h, 0A0h, 08h
        db      0ADh, 10h
        db      0AFh, 01h, 0AFh, 02h, 0AFh, 04h, 0AFh, 08h, 0AFh, 10h, 0AFh, 20h, 0AFh, 40h, 0AFh, 80h
        db      0BCh, 01h, 0BCh, 02h, 0BCh, 04h, 0BCh, 08h, 0BCh, 10h, 0BCh, 20h, 0BCh, 40h, 0BCh, 80h
NREC    equ     ($ - list) / 2

%macro  PIT 0
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     ah, al
        in      al, 40h
        xchg    al, ah
%endmacro

start:
        cld
        ; create the file with its header
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h
        mov     ah, 3Eh
        int     21h

        mov     si, list
        mov     cx, NREC
.next:  push    cx
        push    si
        mov     al, [si]
        mov     [rec], al
        mov     ah, [si + 1]
        mov     [rec + 1], ah
        mov     word [rec + 2], 0
        mov     word [rec + 4], 0
        test    al, al
        jz      .meas                   ; baseline: no write
        call    xrd
        mov     [rec + 2], al           ; v0
        mov     ah, al
        xor     ah, [rec + 1]
        mov     al, [rec]
        call    xwr
        mov     al, [rec]
        call    xrd
        mov     [rec + 3], al           ; v1
.meas:  call    tmem
        mov     [rec + 6], ax
        call    tlines
        mov     [rec + 8], ax
        mov     al, [rec]
        test    al, al
        jz      .save
        mov     ah, [rec + 2]
        call    xwr                     ; restore v0
        mov     al, [rec]
        call    xrd
        mov     [rec + 4], al           ; v2
.save:  call    append
        pop     si
        pop     cx
        add     si, 2
        loop    .next

        mov     dx, m_ok
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

xrd:    mov     dx, 1CEh
        out     dx, al
        inc     dx
        in      al, dx
        ret

xwr:    mov     dx, 1CEh
        out     dx, ax
        ret

; AX = PIT ticks for 256 word reads of B800:0000
tmem:   push    ds
        push    si
        push    cx
        mov     ax, 0B800h
        mov     ds, ax
        xor     si, si
        cli
        PIT
        mov     bx, ax
        mov     cx, 256
.m:     lodsw
        loop    .m
        PIT
        sti
        sub     bx, ax
        mov     ax, bx
        pop     cx
        pop     si
        pop     ds
        ret

; AX = PIT ticks for 32 changes of A9h; FFFFh if it does not change within the poll limit
tlines: push    cx
        push    si
        cli
        PIT
        mov     [t0], ax
        mov     dx, 1CEh
        mov     al, 0A9h
        out     dx, al
        inc     dx
        in      al, dx
        mov     bl, al
        mov     si, 32
.l:     mov     cx, 4000
.w:     in      al, dx
        cmp     al, bl
        jne     .chg
        loop    .w
        sti
        mov     ax, 0FFFFh
        jmp     .x
.chg:   mov     bl, al
        dec     si
        jnz     .l
        PIT
        sti
        mov     bx, [t0]
        sub     bx, ax
        mov     ax, bx
.x:     pop     si
        pop     cx
        ret

; append rec to M8BITS.BIN and close it
append: mov     ax, 3D01h               ; open, write
        mov     dx, oname
        int     21h
        jc      .a
        mov     bx, ax
        mov     ax, 4202h               ; seek to end
        xor     cx, cx
        xor     dx, dx
        int     21h
        mov     ah, 40h
        mov     cx, 12
        mov     dx, rec
        int     21h
        mov     ah, 3Eh
        int     21h
.a:     ret
