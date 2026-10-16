; M8FLIP4.COM - why M8FLIP3 stayed black when M8SHOW showed stripes (worklist L4c). Watch-only, no file.
;
; Hypothesis: a VGA mode set through INT 10h leaves the 8514 CRT controller in reset. DISP_CNTL (22E8h) bits 6:5,
; ENA_DISPLAY: 01 = enabled, 10/11 = reset (Mach32 guide p. 7530); the ROM's engine bring-up writes 60h, a reset.
; Before each arm the 8514 screen (1024x768) is filled with colour 3Fh (white through the shared DAC), and the probe
; waits for a key. Each arm shows the 8514 side for 3 s through ADVFUNC_CNTL = 3 (shadow set 1), then the VGA again.
;   A  text mode as found (the control: M8SHOW worked like this)
;   B  after INT 10h mode 12h (M8FLIP3's case)
;   C  after INT 10h mode 12h, then DISP_CNTL = 23h (enabled, line control 01 - IBM's 640x480 value)
;   D  text mode, then DISP_CNTL = 60h (reset, as the ROM writes)
; THE OWNER WATCHES: for each arm, white, black, or a monitor message.

        org     100h

        jmp     start

m_a     db      13, 10, 'M8FLIP4 arm A (text mode): expect 3 s of white. Press a key.', 13, 10, '$'
m_b     db      13, 10, 'Arm B (after mode 12h): white or black? Press a key.', 13, 10, '$'
m_c     db      13, 10, 'Arm C (mode 12h + DISP_CNTL 23h): white or black? Press a key.', 13, 10, '$'
m_d     db      13, 10, 'Arm D (text + DISP_CNTL 60h reset): white or black? Press a key.', 13, 10, '$'
m_ok    db      13, 10, 'M8FLIP4: done - note A, B, C, D', 13, 10, '$'

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

%macro  SAY 1
        mov     dx, %1
        mov     ah, 9
        int     21h
        xor     ah, ah
        int     16h
%endmacro

start:
        ; A: text mode
        SAY     m_a
        call    white
        call    show

        ; B: after mode 12h
        SAY     m_b
        mov     ax, 0012h
        int     10h
        call    white
        call    show
        mov     ax, 0003h
        int     10h

        ; C: mode 12h, then display enabled
        SAY     m_c
        mov     ax, 0012h
        int     10h
        call    white
        OUTW    22E8h, 0023h
        call    show
        mov     ax, 0003h
        int     10h

        ; D: text mode, then the ROM's reset value
        SAY     m_d
        call    white
        OUTW    22E8h, 0060h
        call    show
        OUTW    22E8h, 0023h            ; leave the 8514 CRTC enabled again

        mov     ax, 0003h
        int     10h
        mov     dx, m_ok
        mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; 8514 side for ~3 s (54 BIOS ticks), then back to the VGA
show:   OUTW    4AE8h, 3
        mov     cx, 54
        call    ticks
        OUTW    4AE8h, 2
        ret

; engine reset, then rows 0-767 x 0-1023 = 3Fh
white:  OUTW    42E8h, 9000h
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
        OUTW    0BEE8h, 0A000h
        OUTW    0EEEEh, 0
        OUTW    0CEEEh, 0
        OUTW    0A6E8h, 3Fh
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 1023
        OUTW    0BEE8h, 767
        OUTW    9AE8h, 40F3h
        call    idle
        ret

idle:   push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.i:     in      ax, dx
        test    ax, 200h
        jz      .iok
        loop    .i
.iok:   pop     cx
        ret

; wait CX BIOS ticks (55 ms each)
ticks:  push    es
        xor     ax, ax
        mov     es, ax
.t0:    mov     ax, [es:046Ch]
.t1:    cmp     ax, [es:046Ch]
        je      .t1
        loop    .t0
        pop     es
        ret
