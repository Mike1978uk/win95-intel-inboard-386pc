; M8FLIP.COM - switching the monitor between the 28800 VGA and the 8514 side (worklist C1).
;
; Both halves are set to 640x480 60 Hz: VGA mode 12h, and the 8514 CRTC programmed with IBM's
; 640x480 values on the 8514/A-compatible 25.175 MHz clock (CLOCK_SEL 0, ADVFUNC bit 2 clear).
; The VGA screen is black, the 8514 screen white (index 3Fh through the shared DAC), so a clean
; switch is an instant black/white change and a resync shows as a blank or a monitor message.
; The OWNER WATCHES: the probe cannot see the monitor.
;   1 frame length of each half, in status polls (same rate = same timing)
;   2 five slow switches, ~1 s each way: polls from the switch to the next vsync of the new half
;   3 120 switches, one per VGA vsync (~2 s): does the monitor hold lock while flipping?
; Writes M8FLIP.BIN: "M8FL", VGA frame (2 words), 8514 frame (2 words), 5 x (to-8514 polls,
; to-VGA polls), fast switches done (word). FFFFh = timed out. DOS boot; ends in text mode.

        org     100h

RES             equ     0800h
VGASTAT         equ     3DAh            ; bit 3 vertical retrace
DISPSTAT        equ     02E8h           ; read: bit 1 vsync; write: H_TOTAL

        jmp     start

oname   db      'M8FLIP.BIN', 0
m_go    db      'M8FLIP: watch the monitor - 5 slow switches, then 2 s of fast ones', 13, 10, '$'
m_ok    db      'M8FLIP: done, results in M8FLIP.BIN', 13, 10, '$'
m_out   db      'M8FLIP: cannot write M8FLIP.BIN', 13, 10, '$'
hdr     db      'M8FL'
recp    dw      0
cyc     dw      0

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

%macro  PUT 0                           ; store AX at [recp]
        mov     di, [recp]
        stosw
        mov     [recp], di
%endmacro

start:
        cld
        mov     dx, m_go
        mov     ah, 9
        int     21h
        mov     cx, 36                  ; ~2 s to read the message
        call    ticks
        mov     ax, 0012h               ; VGA 640x480x16, 60 Hz
        int     10h
        mov     word [recp], RES

        call    init
        call    white
        call    crtc640

; ---- 1: frame lengths ----
        mov     dx, VGASTAT
        mov     bl, 08h
        call    period
        PUT
        call    period
        PUT
        OUTW    4AE8h, 3                ; 8514 on, 25.175 MHz
        mov     dx, DISPSTAT
        mov     bl, 02h
        call    period
        PUT
        call    period
        PUT
        OUTW    4AE8h, 2                ; back to VGA
        mov     cx, 18
        call    ticks

; ---- 2: slow switches ----
        mov     word [cyc], 5
.slow:  OUTW    4AE8h, 3
        mov     dx, DISPSTAT
        mov     bl, 02h
        call    toset
        PUT
        mov     cx, 18
        call    ticks
        OUTW    4AE8h, 2
        mov     dx, VGASTAT
        mov     bl, 08h
        call    toset
        PUT
        mov     cx, 18
        call    ticks
        dec     word [cyc]
        jnz     .slow

; ---- 3: one switch per VGA vsync ----
        xor     si, si
.fast:  mov     dx, VGASTAT
        mov     bl, 08h
        call    toclr
        call    toset
        cmp     ax, 0FFFFh
        je      .fdone
        mov     ax, si
        and     ax, 1
        xor     ax, 3                   ; 3, 2, 3, 2 ...
        mov     dx, 4AE8h
        out     dx, ax
        inc     si
        cmp     si, 120
        jb      .fast
.fdone: OUTW    4AE8h, 2
        mov     ax, si
        PUT

        mov     ax, 0003h
        int     10h

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
        mov     ah, 40h
        mov     cx, [recp]
        sub     cx, RES
        mov     dx, RES
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

; ---- helpers ----
; polls (AX) until (in DX) & BL is set / clear; FFFFh on timeout
toset:  xor     cx, cx
.s:     in      al, dx
        test    al, bl
        jnz     .sok
        loop    .s
        mov     ax, 0FFFFh
        ret
.sok:   mov     ax, cx
        neg     ax
        ret
toclr:  xor     cx, cx
.c:     in      al, dx
        test    al, bl
        jz      .cok
        loop    .c
        mov     ax, 0FFFFh
        ret
.cok:   mov     ax, cx
        neg     ax
        ret

; polls from one rising edge of (DX) & BL to the next
period: call    toclr
        call    toset
        cmp     ax, 0FFFFh
        je      .pto
        call    toclr
        cmp     ax, 0FFFFh
        je      .pto
        mov     [half], ax
        call    toset
        cmp     ax, 0FFFFh
        je      .pto
        add     ax, [half]
.pto:   ret
half    dw      0

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
        ret

; 8514 rows 0-479 x 0-1023 = 3Fh
white:  call    idle
        OUTW    0A6E8h, 3Fh
        OUTW    0BAE8h, 27h
        OUTW    86E8h, 0
        OUTW    82E8h, 0
        OUTW    96E8h, 1023
        OUTW    0BEE8h, 479
        OUTW    9AE8h, 40F3h
        call    idle
        ret

; IBM 8514/A 640x480 60 Hz, negative syncs, on the compatible clock
crtc640:
        OUTW    4AEEh, 0                ; CLOCK_SEL: 8514/A-compatible timing
        OUTW    22E8h, 0043h            ; DISP_CNTL: display off, MEMCFG 4, odd bank
        OUTW    02E8h, 0063h            ; H_TOTAL
        OUTW    06E8h, 004Fh            ; H_DISP
        OUTW    0AE8h, 0052h            ; H_SYNC_STRT
        OUTW    0EE8h, 002Ch            ; H_SYNC_WID, negative
        OUTW    12E8h, 0418h            ; V_TOTAL
        OUTW    16E8h, 03BBh            ; V_DISP
        OUTW    1AE8h, 03D2h            ; V_SYNC_STRT
        OUTW    1EE8h, 0022h            ; V_SYNC_WID, negative
        OUTW    22E8h, 0023h            ; display on
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
