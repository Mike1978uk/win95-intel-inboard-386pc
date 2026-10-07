; This version clears the source compare (92EEh) and its colour (EAEEh) at the start of every
; checkpoint. The card keeps both through an engine reset, so without this each checkpoint after TS1's
; command 133 inherits the previous one's compare.
;
; M8SEQ.COM - find the first drawing command in ATI TEST.COM's Test Sequence 1 that a
; Mach8 model gets wrong, by running the same replay on the real card and in 86Box.
;
; Test Sequence 1 (1 MB card) is one port table inside TEST.COM, at file offset 28A4h
; (TEST.COM address 29A4h), played by TEST.COM's routine at 0641h. It draws, then TEST.COM
; folds the 1024x1024 screen into its top-left corner with blits (76D2h) and compares the
; 8x8 block there with a fixed table. A model that gets one command wrong fails at the
; end, which does not say which command.
;
; For k = 0 .. NCMD, this program: resets the engine, clears the card, replays the table up
; to (not including) its k-th operation trigger (CMD, DEST_Y_END or SCAN_TO_X) - so every earlier command has received all its
; pixel data - then folds and reads the 8x8 block exactly as TEST.COM does, and stores the
; 64 bytes. Checkpoint NCMD is the whole table. Diff the files from the two machines with
; m8seq_diff.py: the first checkpoint that differs names the command.
;
; The table is read from TEST.COM on disk at run time; none of ATI's data is in this file.
; Run from a DOS boot (not a Windows DOS box), in a directory holding TEST.COM.
; Writes M8SEQ5.BIN. The 8514 display is switched on while it runs (as TEST.COM does), so
; the screen goes out of sync for a moment; it is switched back to VGA at the end.

        org     100h

TC_SIZE         equ     43169           ; TEST.COM, md5 bf168ffd9f68f3a27bd8963d3dd39b63
TBL_OFS         equ     29A4h - 100h    ; Test Sequence 1, 1 MB table, in the file
SETUP_OFS       equ     17E7h - 100h    ; engine setup table played before every test sequence
BUF             equ     2000h           ; TEST.COM loaded here
RES             equ     0C900h          ; results, after TEST.COM (2000h-C8A0h)
REC             equ     68              ; per checkpoint: idle timeouts, data timeouts, 2 pad, 64 data

        jmp     start

fname   db      'TEST.COM', 0
oname   db      'M8SEQ5.BIN', 0
m_ok    db      'M8SEQ5: done, results in M8SEQ5.BIN', 13, 10, '$'
m_nof   db      'M8SEQ: cannot read TEST.COM (must be the 43,169-byte ATI v1.46)', 13, 10, '$'
m_bad   db      'M8SEQ: TEST.COM table not where expected', 13, 10, '$'
m_out   db      'M8SEQ5: cannot write M8SEQ5.BIN', 13, 10, '$'

ncmd    dw      0                       ; CMD writes in the table
kcur    dw      0                       ; checkpoint being run
recp    dw      0                       ; where its record goes
to_idle db      0
to_data db      0

start:
        cld
        ; --- load TEST.COM ---
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
        cmp     word [BUF + SETUP_OFS + 2], 0A000h
        jne     badtbl

        ; --- count CMD writes, checking the table holds nothing we do not replay ---
        mov     si, BUF + TBL_OFS
        xor     cx, cx
.cnt:   lodsw
        cmp     al, 0
        je      .cntd
        cmp     ax, 0FFFFh
        je      .cnt
        cmp     ax, 4AEEh
        je      badtbl
        cmp     ax, 4AE8h
        je      badtbl
        call    trig
        jne     .cskip
        inc     cx
.cskip: lodsw
        jmp     .cnt
.cntd:  mov     [ncmd], cx

        ; --- 8514 display on, as TEST.COM's 069Bh does ---
        mov     dx, 22E8h
        mov     ax, 53h
        out     dx, ax
        call    delay
        mov     dx, 4AE8h
        mov     ax, 7
        out     dx, ax
        call    delay

        mov     word [kcur], 0
        mov     word [recp], RES
.ckpt:  mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    init_clear
        mov     cx, [kcur]
        call    replay
        call    fold_read
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        add     word [recp], REC
        mov     ax, [kcur]
        inc     ax
        mov     [kcur], ax
        cmp     ax, [ncmd]
        jbe     .ckpt

        ; --- back to VGA ---
        call    idle
        mov     dx, 4AE8h
        mov     ax, 2
        out     dx, ax
        mov     dx, 2EAh                ; DAC mask: TEST.COM's setup table clears it
        mov     al, 0FFh
        out     dx, al

        ; --- write M8SEQ5.BIN: "M8S5", ncmd, then the records ---
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      outerr
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 6
        mov     dx, hdr
        mov     ax, [ncmd]
        mov     [hdr+4], ax
        mov     ah, 40h
        int     21h
        mov     ah, 40h
        mov     cx, [recp]
        sub     cx, RES
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
badtbl: mov     dx, m_bad
say:    mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

hdr     db      'M8S5', 0, 0

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

; TEST.COM's fold (76D2h) and its 8x8 read (7735h, 27F0h), with the state the full table
; leaves for them made explicit: full scissors, masks open, FRGD_MIX = 73h.
fold_read:
        call    idle
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 73h
        mov     cx, 7
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
        call    idle
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 7
        OUTW    0BEE8h, 7
        OUTW    9AE8h, 43F0h            ; read 8x8
        mov     di, [recp]
        add     di, 4
        mov     cx, 32
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .rz                     ; no data behind it: do not read PIX_TRANS
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        ret
.rz:    xor     ax, ax
        rep     stosw
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
