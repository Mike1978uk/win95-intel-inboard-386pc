; M8ROW.COM - what TEST.COM Test Sequence 1's operations 66-71 leave in rows 100h-10Fh,
; read back directly (no fold). Same replay as M8SEQ.COM (version M8S3: checkpoints before each
; CMD, DEST_Y_END or SCAN_TO_X write), checkpoints 66-72 only (M8R2; M8R1 was 60-65). Each reads (40h,100h)-(7Fh,10Fh),
; 64x16, with the 16-bit read (53B0h) proved on both machines. Op 62 is a VRAM blit that keeps
; the source trajectory of op 61; the row it writes shows which source row each machine used.
;
; ---- M8SEQ's own notes follow ----
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
; Writes M8SEQ.BIN. The 8514 display is switched on while it runs (as TEST.COM does), so
; the screen goes out of sync for a moment; it is switched back to VGA at the end.

        org     100h

TC_SIZE         equ     43169           ; TEST.COM, md5 bf168ffd9f68f3a27bd8963d3dd39b63
TBL_OFS         equ     29A4h - 100h    ; Test Sequence 1, 1 MB table, in the file
BUF             equ     2000h           ; TEST.COM loaded here
RES             equ     0C900h          ; results, after TEST.COM (2000h-C8A0h); 7 x 1028 bytes
REC             equ     1028            ; per checkpoint: idle timeouts, data timeouts, 2 pad, 1024 data
K_FIRST         equ     66
K_LAST          equ     72

        jmp     start

fname   db      'TEST.COM', 0
oname   db      'M8ROW.BIN', 0
m_ok    db      'M8ROW: done, results in M8ROW.BIN', 13, 10, '$'
m_nof   db      'M8ROW: cannot read TEST.COM (must be the 43,169-byte ATI v1.46)', 13, 10, '$'
m_bad   db      'M8ROW: TEST.COM table not where expected', 13, 10, '$'
m_out   db      'M8ROW: cannot write M8ROW.BIN', 13, 10, '$'

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

        mov     word [kcur], K_FIRST
        mov     word [recp], RES
.ckpt:  mov     byte [to_idle], 0
        mov     byte [to_data], 0
        call    init_clear
        mov     cx, [kcur]
        call    replay
        call    area_read
        mov     di, [recp]
        mov     al, [to_idle]
        mov     [di], al
        mov     al, [to_data]
        mov     [di+1], al
        add     word [recp], REC
        mov     ax, [kcur]
        inc     ax
        mov     [kcur], ax
        cmp     ax, K_LAST
        jbe     .ckpt

        ; --- back to VGA ---
        call    idle
        mov     dx, 4AE8h
        mov     ax, 2
        out     dx, ax

        ; --- write M8SEQ.BIN: "M8S1", ncmd, then the records ---
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

hdr     db      'M8R2', 0, 0

; --- outw dx <- imm, used by the fixed sequences below ---
%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

; Engine reset and a 1024x1024 clear to colour 0 (TEST.COM's 083Fh does the same clear).
init_clear:
        OUTW    42E8h, 9000h            ; reset, chip test normal
        OUTW    42E8h, 5000h            ; enable
        call    idle
        OUTW    0BEE8h, 5006h           ; MEM_CNTL: 1 MB layout, as M8GLYPH
        OUTW    76EEh, 0080h            ; GE_PITCH 1024
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0BEE8h, 1000h           ; scissors 0,0 - 1023,1023
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh           ; WRT_MASK
        OUTW    0AEE8h, 00FFh           ; RD_MASK
        OUTW    0BEE8h, 0A000h          ; PIX_CNTL
        OUTW    0EEEEh, 0               ; DEST_CMP_FN: compare off - TS1 relies on it before it writes one
        call    idle
        OUTW    0A6E8h, 0               ; FRGD_COLOR
        OUTW    0BAE8h, 27h             ; FRGD_MIX: colour, source
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 3FFh
        OUTW    0BEE8h, 3FFh            ; MIN_AXIS
        OUTW    9AE8h, 40F3h            ; fill
        call    idle
        ret

; Replay the table until CX CMD writes have gone (CX = NCMD plays all of it).
; Same rules as TEST.COM's 0641h: wait for FIFO room (9AEE bit 4) before each entry,
; FFFF = wait for idle, low byte 00 = end, ports below 1000h and 22E8 are byte writes.
replay:
        mov     si, BUF + TBL_OFS
        mov     bx, cx
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

; Read (40h,100h)-(7Fh,10Fh) straight back: full scissors, masks open, 53B0h as M8GLYPH.
area_read:
        call    idle
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0BAE8h, 47h             ; FRGD_MIX: PC data, source
        OUTW    86E8h, 40h
        OUTW    82E8h, 100h
        OUTW    96E8h, 63
        OUTW    0BEE8h, 15              ; MIN_AXIS
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, [recp]
        add     di, 4
        mov     cx, 512
.r:     call    dready
        cmp     byte [to_data], 0
        jne     .rz
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h
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
