; FLUSHP.COM - which accesses invalidate the 486BL3's L1, and what it costs.
;
; Each test times four passes: plain, with-op, plain, with-op. A pass re-reads a
; working set from Inboard RAM `outer` times (64 KB in all); the with-op pass
; does one access (the op) before each re-read. If the op invalidates the L1,
; the re-read comes from Inboard RAM instead of cache.
;
; The PIT reads that bracket every pass are port accesses themselves, so a plain
; pass already starts cold once. Per-op cost = (op - plain) / (outer - 1).
;
; Ops: IN 21h (8259 IMR, planar), OUT 21h (same IMR value back), read and write
; of B800:1000 (text page 1, not shown), read of F000:FFF0 (BIOS ROM), IN 22h
; (claimed inside the BL3: no bus cycle), eight IN 21h in a row (cost per access
; or per burst?), a register-only delay (control: time alone).
;
; Interrupts are masked for one test at a time only (~35 ms), so COMrade's
; serial ISR keeps up. Output: one line per test, PIT counts in hex.
;   FLUSHP > FLUSHP.TXT
;
; nasm -f bin -o FLUSHP.COM FLUSHP.ASM

        cpu     386
        org     100h

BUF     equ     8000h                   ; up to 8 KB working set, inside our segment

start:
        cld
        in      al, 21h
        mov     dl, al                  ; IMR for OUT 21h; rep lodsd clobbers EAX, not DL
        mov     ax, 0B800h
        mov     es, ax
        mov     ax, 0F000h
        mov     gs, ax
        mov     dx, hdr                 ; DX briefly; DL reloaded below
        mov     ah, 9
        int     21h
        in      al, 21h
        mov     dl, al

%macro OP_NONE 0
%endmacro
%macro OP_IN 0
        in      al, 21h
%endmacro
%macro OP_OUT 0
        mov     al, dl
        out     21h, al
%endmacro
%macro OP_MRD 0
        mov     al, [es:1000h]
%endmacro
%macro OP_MWR 0
        mov     [es:1000h], al
%endmacro
%macro OP_ROM 0
        mov     al, [gs:0FFF0h]
%endmacro
%macro OP_IN22 0
        in      al, 22h
%endmacro
%macro OP_IN8 0
        times 8 in al, 21h
%endmacro
%macro OP_SPIN 0
        mov     bx, 40
%%s:    dec     bx
        jnz     %%s
%endmacro

; PASS op, dwords, outer  -> stores the PIT delta at [rp], advances rp
%macro PASS 3
        call    pit
        mov     bp, bx
        mov     di, %3
%%top:
        %1
        mov     si, BUF
        mov     cx, %2
        rep     lodsd
        dec     di
        jnz     %%top
        call    pit
        sub     bp, bx                  ; PIT counts down
        mov     bx, [rp]
        mov     [bx], bp
        add     word [rp], 2
%endmacro

; TEST tag, op, dwords, outer
%macro TEST 4
        mov     byte [tag], %1
        cli
        PASS    OP_NONE, %3, %4
        PASS    %2, %3, %4
        PASS    OP_NONE, %3, %4
        PASS    %2, %3, %4
        sti
        call    putline
%endmacro

        TEST    'A', OP_IN,   64, 256   ; IN 21h, 256 B working set
        TEST    'B', OP_IN,  256,  64   ; IN 21h, 1 KB
        TEST    'C', OP_IN, 1024,  16   ; IN 21h, 4 KB
        TEST    'D', OP_IN, 2048,   8   ; IN 21h, 8 KB
        TEST    'E', OP_OUT, 256,  64   ; OUT 21h
        TEST    'F', OP_MRD, 256,  64   ; ISA memory read
        TEST    'G', OP_MWR, 256,  64   ; ISA memory write
        TEST    'H', OP_ROM, 256,  64   ; BIOS ROM read
        TEST    'I', OP_IN22, 256, 64   ; IN 22h, inside the BL3
        TEST    'J', OP_IN8, 256,  64   ; eight IN 21h in a row
        TEST    'K', OP_SPIN, 256, 64   ; register delay only

        mov     ax, 4C00h
        int     21h

; Latch PIT channel 0, count -> BX
pit:
        mov     al, 0
        out     43h, al
        in      al, 40h
        mov     bl, al
        in      al, 40h
        mov     bh, al
        ret

; Print "<tag> w0 w1 w2 w3" for the last four results, then reset rp
putline:
        push    dx
        mov     dl, [tag]
        mov     ah, 2
        int     21h
        mov     si, res
        mov     cx, 4
.w:     mov     dl, ' '
        mov     ah, 2
        int     21h
        lodsw
        call    puthex
        loop    .w
        mov     dl, 13
        mov     ah, 2
        int     21h
        mov     dl, 10
        int     21h
        mov     word [rp], res
        pop     dx
        ret

; AX as four hex digits
puthex:
        push    cx
        mov     cx, 4
.d:     rol     ax, 4
        push    ax
        and     al, 0Fh
        add     al, '0'
        cmp     al, '9'
        jbe     .p
        add     al, 7
.p:     mov     dl, al
        mov     ah, 2
        int     21h
        pop     ax
        loop    .d
        pop     cx
        ret

hdr     db      'FLUSHP 1: plain op plain op (PIT counts)', 13, 10, '$'
tag     db      0
rp      dw      res
res     dw      0, 0, 0, 0
