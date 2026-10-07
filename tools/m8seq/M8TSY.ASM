; M8TSY.COM - which register TEST.COM's Test Sequence 1 depends on before writing it. On the real card
; M8TSY (TEST.COM's own setup) gives TEST.COM's expected table and M8TS1 (M8SEQ5's setup, which also forces
; MEM_CNTL, GE_PITCH, GE_OFFSET, DEST_CMP_FN and the source compare) is 17 bytes off. Each pass here runs
; TS1 as M8TSY does with no earlier-stage writes, after forcing one group to M8TS1's value:
;   0   nothing (control)          3   GE_PITCH 80h, GE_OFFSET 0 (76EEh, 72EEh, 6EEEh)
;   1   92EEh 0, EAEEh 0           4   MEM_CNTL 5006h
;   2   DEST_CMP_FN (EEEEh) 0      5   nothing (control)
; Each pass starts from the state the previous one left, so a forced value lasts one pass unless TS1
; rewrites it.
;
; Tables are read from TEST.COM on disk at run time. Run from a DOS boot in a directory holding TEST.COM
; (the 43,169-byte ATI v1.46) and M8PRE.DAT. Writes M8TSY.BIN: "M8Y1", pass count (word), then per
; pass: words read (word), GP_STAT after 32 words (word), idle timeouts, data timeouts, 64 words.

        org     100h

TC_SIZE         equ     43169           ; TEST.COM, md5 bf168ffd9f68f3a27bd8963d3dd39b63
TBL_OFS         equ     29A4h - 100h    ; Test Sequence 1, 1 MB table, in the file
SETUP_OFS       equ     17E7h - 100h    ; engine setup table played before every test sequence
RD_OFS          equ     7735h - 100h    ; the 8x8 read TEST.COM plays after its fold
BUF             equ     2000h           ; TEST.COM loaded here
RES             equ     0C900h          ; results, after TEST.COM (2000h-C8A0h)
PRE             equ     0D000h          ; M8PRE.DAT
REC             equ     134
NPASS           equ     6

        jmp     start

fname   db      'TEST.COM', 0
pname   db      'M8PRE.DAT', 0
oname   db      'M8TSY.BIN', 0
m_ok    db      'M8TSY: done, results in M8TSY.BIN', 13, 10, '$'
m_nof   db      'M8TSY: cannot read TEST.COM (must be the 43,169-byte ATI v1.46)', 13, 10, '$'
m_nop   db      'M8TSY: cannot read M8PRE.DAT', 13, 10, '$'
m_bad   db      'M8TSY: TEST.COM table not where expected', 13, 10, '$'
m_out   db      'M8TSY: cannot write M8TSY.BIN', 13, 10, '$'
hdr     db      'M8Y1'
hcount  dw      NPASS
plens   dw      0, 0, 0, 0, 0, 0
pass    dw      0
recp    dw      0
to_idle db      0
to_data db      0

start:
        cld
        mov     ax, 3D00h
        mov     dx, fname
        int     21h
        jc      nofile
        mov     bx, ax
        mov     ah, 3Fh
        mov     cx, 0C000h
        mov     dx, BUF
        int     21h
        pushf
        push    ax
        mov     ah, 3Eh
        int     21h
        pop     ax
        popf
        jc      nofile
        cmp     ax, TC_SIZE
        jne     nofile
        cmp     word [BUF + TBL_OFS], 0BEE8h    ; first entry: MULTIFUNC_CNTL = 17FE
        jne     badtbl
        cmp     word [BUF + TBL_OFS + 2], 17FEh
        jne     badtbl
        cmp     word [BUF + SETUP_OFS], 0BEE8h  ; setup table: PIX_CNTL = A000
        jne     badtbl
        cmp     word [BUF + RD_OFS], 0BAE8h     ; read table: FRGD_MIX = 27
        jne     badtbl

        mov     ax, 3D00h
        mov     dx, pname
        int     21h
        jc      nopre
        mov     bx, ax
        mov     ah, 3Fh
        mov     cx, 2000h
        mov     dx, PRE
        int     21h
        pushf
        mov     ah, 3Eh
        int     21h
        popf
        jc      nopre

        mov     word [pass], 0
        mov     word [recp], RES
.p:     mov     byte [to_idle], 0
        mov     byte [to_data], 0
        mov     si, [pass]
        shl     si, 1
        mov     cx, [plens + si]
        cmp     cx, [PRE]
        jbe     .n
        mov     cx, [PRE]
.n:     call    prerun
        call    force
        call    tsreset
        mov     si, BUF + TBL_OFS
        mov     bx, 0FFFFh              ; the whole table
        call    play
        call    fold
        mov     si, BUF + RD_OFS
        mov     bx, 0FFFFh
        call    play
        call    readall
        add     word [recp], REC
        inc     word [pass]
        cmp     word [pass], NPASS
        jb      .p

        ; --- back to VGA ---
        call    idle
        mov     dx, 4AE8h
        mov     ax, 2
        out     dx, ax
        mov     dx, 2EAh                ; DAC mask: TEST.COM's setup table clears it
        mov     al, 0FFh
        out     dx, al

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      outerr
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 6
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, NPASS * REC
        mov     dx, RES
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     say
outerr: mov     dx, m_out
        jmp     say
nofile: mov     dx, m_nof
        jmp     say
nopre:  mov     dx, m_nop
        jmp     say
badtbl: mov     dx, m_bad
say:    mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; --- outw dx <- imm, used by the fixed sequences below ---
%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

; The state TEST.COM sets before every test sequence (1E21h): its 05F0h (32EE, engine reset),
; then its table at 17E7h, played from the copy of TEST.COM in memory - scissors, mixes, colours,
; patterns, masks, two full-screen fills, CUR 0,0. Without it, registers TS1 uses before writing
; them (BKGD_MIX, COLOR_CMP) keep whatever the previous program left. MEM_CNTL, GE_PITCH and
; DEST_CMP_FN are set by earlier TEST.COM stages that are not replayed.
init_clear:
        OUTW    32EEh, 0
        OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        call    idle
        OUTW    0BEE8h, 5006h           ; MEM_CNTL: 1 MB layout, as M8GLYPH
        OUTW    76EEh, 0080h            ; GE_PITCH 1024
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0EEEEh, 0               ; DEST_CMP_FN: compare off - TS1 relies on it before it writes one
        OUTW    92EEh, 0                ; source compare off: an engine reset keeps it (M8SCMP)
        OUTW    0EAEEh, 0
        mov     si, BUF + SETUP_OFS
        mov     bx, 0FFFFh              ; no trigger limit
        call    play
        call    idle
        ret

; Replay the table until CX CMD writes have gone (CX = NCMD plays all of it).
; Same rules as TEST.COM's 0641h: wait for FIFO room (9AEE bit 4) before each entry,
; FFFF = wait for idle, low byte 00 = end, ports below 1000h and 22E8 are byte writes.
replay:
        mov     si, BUF + TBL_OFS
        mov     bx, cx
; Play the table at SI, stopping before trigger BX+1.
play:
.next:  call    fifo
        lodsw
        cmp     al, 0
        je      .done
        cmp     ax, 0FFFFh
        jne     .port
        call    idle
        jmp     .next
.port:  mov     dx, ax
        mov     ax, dx
        call    trig
        jne     .val
        or      bx, bx
        jz      .done                   ; stop before the next CMD
        dec     bx
.val:   lodsw
        test    dx, 0F000h
        jz      .byte
        cmp     dx, 22E8h
        je      .byte
        out     dx, ax
        jmp     .next
.byte:  out     dx, al
        jmp     .next
.done:  ret

; Play the first CX writes of M8PRE.DAT as TEST.COM made them: FIFO room before each, byte or word as
; logged, a delay after the display and clock selects, and after a read command (CMD bit 0 clear)
; every PIX_TRANS word it offers is read and dropped.
prerun: jcxz    .done
        mov     si, PRE + 2
.l:     push    cx
        call    fifo
        mov     dx, [si]
        mov     ax, [si+2]
        mov     bl, [si+4]
        add     si, 5
        cmp     bl, 1
        jne     .w
        out     dx, al
        jmp     .after
.w:     out     dx, ax
.after: cmp     dx, 4AE8h
        je      .dly
        cmp     dx, 4AEEh
        jne     .rd
.dly:   call    delay
        jmp     .nx
.rd:    cmp     dx, 9AE8h
        jne     .nx
        test    al, 1
        jnz     .nx
        call    drain
.nx:    pop     cx
        loop    .l
.done:  ret

; Read and drop PIX_TRANS words while GP_STAT says data is ready, up to 1024.
drain:  push    si
        call    dready
        mov     cx, 1024
.d:     mov     dx, 9AE8h
        in      ax, dx
        test    ax, 100h
        jz      .e
        mov     dx, 0E2E8h
        in      ax, dx
        loop    .d
.e:     pop     si
        ret

; This pass's forced group.
force:  mov     ax, [pass]
        cmp     ax, 1
        jne     .f2
        OUTW    92EEh, 0
        OUTW    0EAEEh, 0
        ret
.f2:    cmp     ax, 2
        jne     .f3
        OUTW    0EEEEh, 0
        ret
.f3:    cmp     ax, 3
        jne     .f4
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        ret
.f4:    cmp     ax, 4
        jne     .f5
        OUTW    0BEE8h, 5006h
.f5:    ret

; TEST.COM's 1E21h: engine reset through 32EEh and 42E8h (05F0h), the setup table 17E7h, idle.
tsreset:
        OUTW    32EEh, 0
        OUTW    42E8h, 900Fh
        OUTW    42E8h, 400Fh
        mov     si, BUF + SETUP_OFS
        mov     bx, 0FFFFh
        call    play
        call    idle
        ret

; TEST.COM's fold (76D2h): seven pairs of blits halving the 1024x1024 screen into its corner. It sets
; only the coordinates and the command.
fold:   mov     cx, 7
        mov     si, 200h
.f:     call    idle
        mov     dx, 0BEE8h              ; MIN_AXIS = si - 1
        mov     ax, si
        dec     ax
        out     dx, ax
        mov     dx, 96E8h               ; MAJ_AXIS = 2si - 1
        mov     ax, si
        shl     ax, 1
        dec     ax
        out     dx, ax
        mov     dx, 82E8h               ; from (0, si)
        mov     ax, si
        out     dx, ax
        mov     dx, 86E8h
        xor     ax, ax
        out     dx, ax
        mov     dx, 8EE8h               ; to (0, 0)
        out     dx, ax
        mov     dx, 8AE8h
        out     dx, ax
        OUTW    9AE8h, 0C0F1h
        call    idle
        mov     dx, 96E8h               ; MAJ_AXIS = si - 1
        mov     ax, si
        dec     ax
        out     dx, ax
        mov     dx, 86E8h               ; from (si, 0)
        mov     ax, si
        out     dx, ax
        mov     dx, 82E8h
        xor     ax, ax
        out     dx, ax
        mov     dx, 8AE8h
        out     dx, ax
        OUTW    9AE8h, 0C0F1h
        shr     si, 1
        loop    .f
        ret

; TEST.COM's compare read (27F0h): 32 words, each after data ready; then GP_STAT, recorded; then, as
; TEST.COM does, more words while data ready stays set - up to 64 in all here.
readall:
        mov     di, [recp]
        add     di, 6
        xor     bx, bx                  ; words read
        mov     cx, 32
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .done
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        inc     bx
        loop    .r
        mov     dx, 9AE8h
        in      ax, dx
        mov     si, [recp]
        mov     [si+2], ax
.more:  cmp     bx, 64
        jae     .done
        mov     dx, 9AE8h
        in      ax, dx
        test    ax, 100h
        jz      .done
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        inc     bx
        jmp     .more
.done:  mov     si, [recp]
        mov     [si], bx
        mov     al, [to_idle]
        mov     [si+4], al
        mov     al, [to_data]
        mov     [si+5], al
        ret

; ZF set if port AX starts an operation: the 8514/A CMD, or the Mach8 extended triggers
; DEST_Y_END (blit) and SCAN_TO_X (raster draw). Checkpoints fall before each of these.
trig:   cmp     ax, 9AE8h
        je      .t
        cmp     ax, 0AEEEh
        je      .t
        cmp     ax, 0CAEEh
.t:     ret

; Bounded waits. 65,536 polls of GP_STAT is about 0.4 s on the 5160.
idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        inc     byte [to_idle]
.iok:   pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
        inc     byte [to_data]
.dok:   pop     cx
        ret

fifo:   push    cx
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
        inc     byte [to_idle]
.qok:   pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.w:     loop    .w
        pop     cx
        ret
