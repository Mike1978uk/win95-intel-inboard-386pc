; RSOAK2.COM - does memory hold its contents at a slower DRAM refresh?
;
; Refresh divisor 64 (PIT channel 1) frees 3-6% of the bus (docs/mach8_card_coverage.md) but
; refreshes DRAM 3.5x less often than IBM's 18. The Inboard has no refresh of its own: every bank's
; RAS is enabled on DACK0, the XT's refresh DMA cycle (RonnyRoy's reproduction, U71.pld), so the
; divisor sets refresh for all of the Inboard's memory as well as the planar's 64 KB.
;
; Three regions, each checked every STEP ticks in a control arm (divisor 18) and a test arm (64):
;   planar 0000:0000-FFFF   DOS lives there: 1 KB blocks found static at 18 are re-checksummed
;   conventional            free memory above this program, filled with a known pattern
;   XMS                     all free XMS (up to 8 MB), filled in 64 KB chunks through a buffer
; Divisor 18 is restored at the end. Changes at 18 are ordinary writes; decay shows at 64 only.
; Writes RSOAK2.BIN: "RSK2", conv KB (w), XMS KB (w), static[64],
;   then per arm: checks (w), planar events (w), conv errors (d), XMS errors (d), changed[64].

        org     100h
        cpu     386

ARMT            equ     5461            ; 5 minutes of 18.2 Hz ticks
STEP            equ     182             ; 10 s
XMSMAX          equ     8192

        jmp     start

oname   db      'RSOAK2.BIN', 0
m_ok    db      'RSOAK2: done, divisor 18 restored, results in RSOAK2.BIN', 13, 10, '$'
m_out   db      'RSOAK2: cannot write RSOAK2.BIN', 13, 10, '$'
hdr     db      'RSK2'
convkb  dw      0
xmskb   dw      0
stmask  times 64 db 0
arm18   dw      0, 0
        dd      0, 0
chg18   times 64 db 0
arm64   dw      0, 0
        dd      0, 0
chg64   times 64 db 0
RECLEN  equ     12 + 64

snapA   times 64 dd 0
snapB   times 64 dd 0
t0      dw      0
tnext   dw      0
xment   dd      0
xmh     dw      0
nchunk  dw      0
bufseg  dw      0
convs   dw      0
conve   dw      0
mv:                                     ; XMS move structure
mvlen   dd      65536
mvsh    dw      0
mvso    dd      0
mvdh    dw      0
mvdo    dd      0
errs    dd      0

start:  cld
        mov     ax, cs
        add     ax, 1000h
        mov     [bufseg], ax            ; 64 KB transfer buffer
        add     ax, 1000h
        mov     [convs], ax
        mov     ax, [2]                 ; PSP: top of our memory
        mov     [conve], ax
        sub     ax, [convs]
        shr     ax, 6
        mov     [convkb], ax

        call    xmsinit
        call    convfill
        call    xmsfill

        mov     al, 18
        call    setdiv
        mov     di, snapA
        call    snap
        mov     cx, 36
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

        cmp     word [xmh], 0
        je      .nofree
        mov     ah, 0Ah
        mov     dx, [xmh]
        call    far [xment]
.nofree:
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 4 + 64 + 2 * RECLEN
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

; One arm. AL = divisor, SI -> {checks, planar events, conv errors, XMS errors, changed[64]}.
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
        js      .loop
        add     word [tnext], STEP
        inc     word [si]
        mov     di, snapA
        call    snap
        xor     ebx, ebx
.cmp:   cmp     byte [stmask + bx], 0
        je      .next
        mov     eax, [snapA + ebx*4]
        cmp     eax, [snapB + ebx*4]
        je      .next
        inc     word [si + 2]
        mov     byte [si + 12 + bx], 1
        mov     [snapB + ebx*4], eax
.next:  inc     bx
        cmp     bx, 64
        jb      .cmp
        call    convchk
        add     [si + 4], eax
        call    xmschk
        add     [si + 8], eax
        jmp     .loop
.done:  ret

; Pattern word for segment/chunk BX, word index CX -> AX. Never all-0 or all-1 by construction.
pat:    mov     ax, bx
        imul    ax, ax, 9E37h
        add     ax, cx
        xor     ax, 5AA5h
        ret

; Conventional: from convs to conve, one 64 KB window per segment step of 1000h.
convfill:
        mov     bx, [convs]
.seg:   cmp     bx, [conve]
        jae     .done
        call    convwords               ; DX = words in this window
        mov     es, bx
        xor     cx, cx
        xor     di, di
.w:     call    pat
        stosw
        inc     cx
        cmp     cx, dx
        jb      .w
        add     bx, 1000h
        jmp     .seg
.done:  push    cs
        pop     es
        ret

; EAX = mismatched words in the conventional pattern.
convchk:
        push    si
        xor     ebp, ebp
        mov     bx, [convs]
.seg:   cmp     bx, [conve]
        jae     .done
        call    convwords
        mov     es, bx
        xor     cx, cx
        xor     si, si
.w:     call    pat
        cmp     ax, [es:si]
        je      .ok
        inc     ebp
.ok:    add     si, 2
        inc     cx
        cmp     cx, dx
        jb      .w
        add     bx, 1000h
        jmp     .seg
.done:  push    cs
        pop     es
        mov     eax, ebp
        pop     si
        ret

; DX = words in the window at segment BX (max 8000h).
convwords:
        mov     dx, [conve]
        sub     dx, bx
        cmp     dx, 1000h
        jae     .full
        shl     dx, 3                   ; paragraphs -> words
        ret
.full:  mov     dx, 8000h
        ret

; XMS: driver entry, allocate all free (capped), chunks of 64 KB.
xmsinit:
        mov     ax, 4300h
        int     2Fh
        cmp     al, 80h
        jne     .none
        mov     ax, 4310h
        int     2Fh
        mov     word [xment], bx
        mov     word [xment + 2], es
        push    cs
        pop     es
        mov     ah, 08h
        call    far [xment]             ; AX = largest free block, KB
        and     ax, 0FFC0h              ; whole 64 KB chunks
        cmp     ax, XMSMAX
        jbe     .sz
        mov     ax, XMSMAX
.sz:    or      ax, ax
        jz      .none
        mov     dx, ax
        push    dx
        mov     ah, 09h
        call    far [xment]
        pop     cx
        cmp     ax, 1
        jne     .none
        mov     [xmh], dx
        mov     [xmskb], cx
        shr     cx, 6
        mov     [nchunk], cx
.none:  push    cs
        pop     es
        ret

; Fill the buffer with chunk BX's pattern.
genbuf: mov     es, [bufseg]
        xor     cx, cx
        xor     di, di
.w:     call    pat
        stosw
        inc     cx
        cmp     cx, 8000h
        jb      .w
        push    cs
        pop     es
        ret

; Move 64 KB: buffer -> XMS chunk BX (AL=0) or XMS chunk BX -> buffer (AL=1).
xmsmove:
        push    si
        movzx   edx, bx
        shl     edx, 16
        mov     cx, [bufseg]
        or      al, al
        jnz     .back
        mov     word [mvsh], 0
        mov     word [mvso], 0
        mov     word [mvso + 2], cx
        mov     ax, [xmh]
        mov     [mvdh], ax
        mov     [mvdo], edx
        jmp     .go
.back:  mov     ax, [xmh]
        mov     [mvsh], ax
        mov     [mvso], edx
        mov     word [mvdh], 0
        mov     word [mvdo], 0
        mov     word [mvdo + 2], cx
.go:    mov     ah, 0Bh
        mov     si, mv
        call    far [xment]
        pop     si
        ret

xmsfill:
        xor     bx, bx
.c:     cmp     bx, [nchunk]
        jae     .done
        call    genbuf
        xor     al, al
        call    xmsmove
        inc     bx
        jmp     .c
.done:  ret

; EAX = mismatched words across all XMS chunks.
xmschk: push    si
        xor     ebp, ebp
        xor     bx, bx
.c:     cmp     bx, [nchunk]
        jae     .done
        mov     al, 1
        call    xmsmove
        mov     es, [bufseg]
        xor     cx, cx
        xor     si, si
.w:     call    pat
        cmp     ax, [es:si]
        je      .ok
        inc     ebp
.ok:    add     si, 2
        inc     cx
        cmp     cx, 8000h
        jb      .w
        push    cs
        pop     es
        inc     bx
        jmp     .c
.done:  mov     eax, ebp
        pop     si
        ret

; AX = low word of the BIOS tick count (0040:006C).
ticks:  push    es
        push    word 40h
        pop     es
        mov     ax, [es:6Ch]
        pop     es
        ret

; CX ticks.
delay:  call    ticks
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
        push    bp
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
        pop     bp
        pop     di
        pop     ds
        pop     si
        ret
