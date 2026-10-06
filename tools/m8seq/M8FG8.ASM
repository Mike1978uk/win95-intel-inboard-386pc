; M8FG8.COM - is DP_CONFIG bit 11 with foreground source 6 a race? M8FG7 found the host data always
; starts 4 pixels late after a tight write loop, whatever PATT_INDEX/LENGTH/CUR_X; TS1 op 112 sees 8.
; Here each case waits WAIT loop passes before the first data word and between words; the 4th
; field is reused as WAIT. All at CUR_X 0, PATT_LENGTH 1F, PATT_INDEX 0 (row 600 + 2*case):
;   0 CA11 0     1 CA11 50     2 CA11 200     3 CA11 1000
;   4 C211 0     5 C211 50     6 C211 200     7 C211 1000
; Read back (0,600)-(127,615); writes M8FG8.BIN: "M8H1", 8 timeout bytes, 2048 pixels.
;
; ---- M8FG7's notes follow ----
;
; M8FG7.COM - DP_CONFIG bit 11 with foreground source 6: what sets how many pixels pass before the
; host data starts (4 in M8FG6 case 0, 8 in TS1 op 112).
;
; ---- M8FG6's notes follow ----
;
; M8FG6.COM - what DP_CONFIG foreground source 6 does on a Mach8. The Mach32 guide lists sources
; 0-3 and 5; TEST.COM's Test Sequence 1 uses 6 (ops 112-113, DP_CONFIG CA11h and C211h) and the two
; differ only in bits 11:10, yet index the colour pattern differently.
;
; The colour pattern holds 80h + n in byte n (n = 0..31), so each pixel read back names the pattern
; byte it took. Each case clears row 600 + 2*case, x 0-31, then: PATT_LENGTH 1Fh, PATT_INDEX 0,
; DP_CONFIG (case value), CUR (0, row), SCAN_TO_X 16, and - "with data" - eight PIX_TRANS words
; whose bytes, high byte first, are 01 02 04 08 10 20 40 80 03 0C 30 C0 05 0A 50 A0. Then a bounded
; idle wait, whose timeout is recorded. The engine is reset between cases.
;   case 0  CA11h  with data      case 4  C011h  with data
;   case 1  C211h  with data      case 5  CA11h  no data
;   case 2  C611h  with data      case 6  C211h  no data
;   case 3  CE11h  with data      case 7  A011h  with data (source 5, the documented colour pattern)
; Then (0,600)-(31,615) is read back with the 8514/A 16-bit read. Writes M8FG6.BIN: "M8F1",
; 8 timeout bytes (1 = the case never went idle), 512 pixels. Run from a DOS boot; engine only.

        org     100h

        jmp     start

oname   db      'M8FG8.BIN', 0
m_ok    db      'M8FG8: done, results in M8FG8.BIN', 13, 10, '$'
m_out   db      'M8FG8: cannot write M8FG8.BIN', 13, 10, '$'
hdr     db      'M8H1'
tout    times 8 db 0
buf     times 2048 db 0
dat     dw      0102h, 0408h, 1020h, 4080h, 030Ch, 30C0h, 050Ah, 50A0h
cases   dw      0CA11h, 1Fh, 0, 0
        dw      0CA11h, 1Fh, 0, 50
        dw      0CA11h, 1Fh, 0, 200
        dw      0CA11h, 1Fh, 0, 1000
        dw      0C211h, 1Fh, 0, 0
        dw      0C211h, 1Fh, 0, 50
        dw      0C211h, 1Fh, 0, 200
        dw      0C211h, 1Fh, 0, 1000
waitn   dw      0
cn      dw      0
row     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

start:
        cld
        mov     word [cn], 0
.c:     call    init
        mov     ax, [cn]
        shl     ax, 1
        add     ax, 600
        mov     [row], ax
        ; clear x 0-31 of this row
        OUTW    0BAE8h, 27h
        OUTW    0A6E8h, 0
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, [row]
        out     dx, ax
        OUTW    96E8h, 127
        OUTW    0BEE8h, 0
        OUTW    9AE8h, 40F3h
        call    idle
        ; colour pattern: byte n = 80h + n
        OUTW    82EEh, 0
        mov     dx, 8EEEh
        mov     ax, 8180h
        mov     cx, 16
.p:     out     dx, ax
        add     ax, 0202h
        loop    .p
        mov     bx, [cn]
        shl     bx, 1
        shl     bx, 1
        shl     bx, 1                   ; 8 bytes per case
        mov     dx, 0D2EEh              ; PATT_LENGTH
        mov     ax, [cases + bx + 2]
        out     dx, ax
        mov     dx, 0D6EEh              ; PATT_INDEX
        mov     ax, [cases + bx + 4]
        out     dx, ax
        OUTW    0BAEEh, 7               ; ALU_FG_FN: paint
        OUTW    0B6EEh, 7               ; ALU_BG_FN: paint
        mov     dx, 0CEEEh
        mov     ax, [cases + bx]
        out     dx, ax
        mov     ax, [cases + bx + 6]
        mov     [waitn], ax
        OUTW    86E8h, 0
        mov     dx, 82E8h
        mov     ax, [row]
        out     dx, ax
        OUTW    0CAEEh, 16              ; SCAN_TO_X
        mov     dx, 0E2E8h
        mov     si, dat
        mov     cx, 8
.w:     push    cx
        mov     cx, [waitn]
        jcxz    .nw
.dl:    loop    .dl                     ; WAIT passes before each word
.nw:    pop     cx
        lodsw
        out     dx, ax
        loop    .w
.nd:    call    idle
        mov     bx, [cn]
        mov     al, [to_flag]
        mov     [tout + bx], al
        inc     word [cn]
        cmp     word [cn], 8
        jb      .c

        ; read back (0,600)-(31,615)
        call    init
        OUTW    0BAE8h, 47h
        OUTW    86E8h, 0
        OUTW    82E8h, 600
        OUTW    96E8h, 127
        OUTW    0BEE8h, 15
        call    idle
        OUTW    9AE8h, 53B0h
        mov     di, buf
        mov     cx, 1024
.r:     call    dready
        mov     dx, 0E2E8h
        in      ax, dx
        stosw
        loop    .r
        call    idle
        OUTW    0BAE8h, 27h

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4 + 8 + 2048
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

; Engine reset (clears any operation still waiting for data), pitch, scissors, masks, mixes.
init:   OUTW    42E8h, 9000h
        OUTW    42E8h, 5000h
        call    idle
        OUTW    0BEE8h, 5006h
        OUTW    76EEh, 0080h
        OUTW    72EEh, 0
        OUTW    6EEEh, 0
        OUTW    0BEE8h, 1000h
        OUTW    0BEE8h, 2000h
        OUTW    0BEE8h, 33FFh
        OUTW    0BEE8h, 43FFh
        OUTW    0AAE8h, 00FFh
        OUTW    0AEE8h, 00FFh
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        OUTW    0CEEEh, 0
        ret

to_flag db      0

; Wait for the engine to go idle, at most 65,536 polls; to_flag = 1 if it never did.
idle:   push    cx
        mov     byte [to_flag], 0
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
        mov     byte [to_flag], 1
.iok:   pop     cx
        ret

dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
.dok:   pop     cx
        ret
