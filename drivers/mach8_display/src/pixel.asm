; Pixel on the screen (screen.c). A set, when the engine already holds the
; data path for the mix: three port writes, and the colour when it changes
; and MAJ/MIN when the engine does not hold a 1x1 rectangle. A get, when it
; holds the read set-up: a one-pixel SCAN_TO_X read, three writes and the
; read (m8_hw_getpixel_fast).
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; Each call starts with the L1 empty (the last call's port writes flushed it),
; so the cost is the code and data lines touched. This path is one aligned
; run with every check before the first write; anything it does not take
; (a get, a bitmap, the cursor in the way, a cold engine, a short FIFO) goes
; to PixelC with the frame untouched. The checks are m8_hw_fill_fast's.

FRGD_COLOR      equ     0A6E8h
CUR_X           equ     86E8h
CUR_Y           equ     82E8h
MAJ_AXIS_PCNT   equ     96E8h
MULTIFUNC_CNTL  equ     0BEE8h
CMD             equ     9AE8h
EXT_FIFO_STATUS equ     9AEEh
GP_STAT         equ     9AE8h           ; read
DATA_READY      equ     0100h
SCAN_TO_X       equ     0CAEEh
PIX_TRANS       equ     0E2E8h
DE_FLAGS        equ     28              ; DIBENGINE.deFlags
PALETTE_XLAT    equ     1000h           ; bit n set: entry n + 1 holds a write
CMD_FILL        equ     40F3h
MIX_SRC_FRGD    equ     0020h

; M8_HOT (m8.h); screen.c checks these offsets at compile time.
H_SCREEN        equ     0
H_ROOM          equ     4
H_DP_MIX        equ     6
H_ROOM_OK       equ     8
H_DP_OK         equ     9
H_DP_PLAIN      equ     10
H_HW_OFF        equ     11
H_BUSY          equ     12
H_CUR_SHOWN     equ     13
H_CUR_PENDING   equ     14
H_RD_OK         equ     15
H_CUR_L         equ     16
H_CUR_T         equ     18
H_CUR_R         equ     20
H_CUR_B         equ     22
H_MM_W          equ     24
H_MM_H          equ     26
H_OK_COLOR      equ     30      ; sh_ok[M8_SH_FRGD_COLOR]
H_VAL_COLOR     equ     44      ; sh_val[M8_SH_FRGD_COLOR]

; Pascal far frame after push bp
A_DM            equ     6
A_COLOR         equ     10
A_Y             equ     14
A_X             equ     16
A_DEV           equ     18

ifdef M8_PROF
M8_TIMED_ENTRY  equ     1
endif
ifdef M8_TRACE
M8_TIMED_ENTRY  equ     1
endif

ifdef M8_TIMED_ENTRY
public  M8T_PIXEL
else
public  PIXEL
endif

_DATA   segment para public 'DATA'
        extrn   _m8_hot : byte
        extrn   _m8_scr_x : word
        extrn   _m8_scr_y : word
        extrn   _m8_mix_for_key : byte
ifdef M8_PROF
        extrn   _m8_pf : word
endif
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment para public 'CODE'
        extrn   PIXELC : far
        extrn   m8_leave_ : near
        extrn   m8_hw_engine_reset_ : near

        .386
        assume  cs:_TEXT, ds:DGROUP

        align   16
ifdef M8_TIMED_ENTRY
M8T_PIXEL       proc    far
else
PIXEL           proc    far
endif
        push    bp
        mov     bp, sp
        push    ds
        mov     ax, DGROUP
        mov     ds, ax
        mov     ax, [bp + A_DEV]
        mov     dx, [bp + A_DEV + 2]
        cmp     ax, word ptr _m8_hot[H_SCREEN]
        jne     slow
        cmp     dx, word ptr _m8_hot[H_SCREEN + 2]
        jne     slow
        or      ax, dx
        jz      slow
        les     bx, [bp + A_DM]
        mov     ax, es
        or      ax, bx
        jz      get
        mov     bx, es:[bx]             ; Rop2, 1..16
        dec     bx
        cmp     bx, 15
        ja      slow
        mov     cx, [bp + A_X]
        cmp     cx, _m8_scr_x
        jae     slow
        mov     dx, [bp + A_Y]
        cmp     dx, _m8_scr_y
        jae     slow
        cmp     _m8_hot[H_CUR_SHOWN], 0
        jne     cursor
clear:
        mov     _m8_hot[H_BUSY], 1      ; before the engine state is read
        cmp     _m8_hot[H_HW_OFF], 0
        jne     slowb
        cmp     _m8_hot[H_DP_OK], 0
        je      slowb
        cmp     _m8_hot[H_DP_PLAIN], 0
        je      slowb
        cmp     _m8_hot[H_ROOM_OK], 0
        je      slowb
        cmp     _m8_hot[H_OK_COLOR], 0
        je      slowb
        movzx   ax, _m8_mix_for_key[bx]
        or      ax, MIX_SRC_FRGD
        cmp     ax, word ptr _m8_hot[H_DP_MIX]
        jne     slowb
        cmp     word ptr _m8_hot[H_ROOM], 6
        jl      room_wait
room_ready:
        mov     bx, dx                  ; Y
        movzx   ax, byte ptr [bp + A_COLOR]
        cmp     ax, word ptr _m8_hot[H_VAL_COLOR]
        jne     colour
written:
        sub     word ptr _m8_hot[H_ROOM], 3
        mov     ax, cx
        mov     dx, CUR_X
        out     dx, ax
        mov     ax, bx
        mov     dx, CUR_Y
        out     dx, ax
        cmp     dword ptr _m8_hot[H_MM_W], 00010001h
        jne     mm_size
sized:
        mov     ax, CMD_FILL
        mov     dx, CMD
        out     dx, ax
ifdef M8_PROF
        inc     _m8_pf[6]               ; hits
endif
        cmp     _m8_hot[H_CUR_PENDING], 0
        jne     pending
        mov     _m8_hot[H_BUSY], 0
done:
        mov     ax, [bp + A_COLOR]      ; GDI does not use a set's return
        mov     dx, [bp + A_COLOR + 2]
        pop     ds
        pop     bp
        retf    16

mm_size:
        mov     dword ptr _m8_hot[H_MM_W], 00010001h
        sub     word ptr _m8_hot[H_ROOM], 2
        xor     ax, ax
        mov     dx, MAJ_AXIS_PCNT
        out     dx, ax
        mov     dx, MULTIFUNC_CNTL      ; MIN_AXIS, one row
        out     dx, ax
        jmp     sized

colour:
        mov     word ptr _m8_hot[H_VAL_COLOR], ax
        dec     word ptr _m8_hot[H_ROOM]
        mov     dx, FRGD_COLOR
        out     dx, ax
        jmp     written

cursor:
        cmp     cx, word ptr _m8_hot[H_CUR_L]
        jl      clear
        cmp     cx, word ptr _m8_hot[H_CUR_R]
        jge     clear
        cmp     dx, word ptr _m8_hot[H_CUR_T]
        jl      clear
        cmp     dx, word ptr _m8_hot[H_CUR_B]
        jge     clear
        jmp     slow
slowb:
        mov     _m8_hot[H_BUSY], 0
slow:
        pop     ds
        pop     bp
        jmp     PIXELC

pending:
        call    m8_leave_
        jmp     done

; Room 6: entry 10 free (as blt.asm's fill_wait).
room_wait:
        push    dx
        xor     bx, bx
        mov     dx, EXT_FIFO_STATUS
room_poll:
        in      ax, dx
        test    ax, 0200h
        jz      room_got
        dec     bx
        jnz     room_poll
        pop     dx
        jmp     slowb
room_got:
        mov     word ptr _m8_hot[H_ROOM], 6
        pop     dx
        jmp     room_ready
; The get. No idle wait, as ATIM8 has none: the read runs after the drawing
; queued before it, and its data is ready only then. A translated palette
; needs the reverse table: C's path.
get:
        mov     cx, [bp + A_X]
        cmp     cx, _m8_scr_x
        jae     slow
        mov     dx, [bp + A_Y]
        cmp     dx, _m8_scr_y
        jae     slow
        les     bx, dword ptr _m8_hot[H_SCREEN]
        test    word ptr es:[bx + DE_FLAGS], PALETTE_XLAT
        jnz     slow
        cmp     _m8_hot[H_CUR_SHOWN], 0
        jne     get_cursor
get_clear:
        mov     _m8_hot[H_BUSY], 1      ; before the engine state is read
        cmp     _m8_hot[H_HW_OFF], 0
        jne     slowb
        cmp     _m8_hot[H_RD_OK], 0
        je      slowb
        cmp     _m8_hot[H_ROOM_OK], 0
        je      slowb
        cmp     word ptr _m8_hot[H_ROOM], 3
        jl      get_wait
get_ready:
        mov     dword ptr _m8_hot[H_MM_W], 0    ; SCAN_TO_X changes MAJ/MIN
        mov     ax, dx
        mov     dx, CUR_Y
        out     dx, ax
        mov     ax, cx
        mov     dx, CUR_X
        out     dx, ax
        inc     ax
        mov     dx, SCAN_TO_X
        out     dx, ax
        xor     bx, bx
        mov     dx, GP_STAT
get_data:
        in      ax, dx
        test    ax, DATA_READY
        jnz     get_read
        dec     bx
        jnz     get_data
        call    m8_hw_engine_reset_     ; wedged: C's path sets up again
        jmp     slowb
get_read:
        mov     word ptr _m8_hot[H_ROOM], 15
        mov     dx, PIX_TRANS
        in      ax, dx
        xor     ah, ah
ifdef M8_PROF
        inc     _m8_pf[6]               ; hits
endif
        cmp     _m8_hot[H_CUR_PENDING], 0
        jne     get_pending
        mov     _m8_hot[H_BUSY], 0
get_done:
        mov     dx, 0FF00h              ; FF00h above the pixel, as the DIB Engine
        pop     ds
        pop     bp
        retf    16

get_pending:
        push    ax
        call    m8_leave_
        pop     ax
        jmp     get_done

get_cursor:
        cmp     cx, word ptr _m8_hot[H_CUR_L]
        jl      get_clear
        cmp     cx, word ptr _m8_hot[H_CUR_R]
        jge     get_clear
        cmp     dx, word ptr _m8_hot[H_CUR_T]
        jl      get_clear
        cmp     dx, word ptr _m8_hot[H_CUR_B]
        jge     get_clear
        jmp     slow

; Room 3: entry 13 free.
get_wait:
        push    dx
        xor     bx, bx
        mov     dx, EXT_FIFO_STATUS
get_poll:
        in      ax, dx
        test    ax, 1000h
        jz      get_got
        dec     bx
        jnz     get_poll
        pop     dx
        jmp     slowb
get_got:
        mov     word ptr _m8_hot[H_ROOM], 3
        pop     dx
        jmp     get_ready

ifdef M8_TIMED_ENTRY
M8T_PIXEL       endp
else
PIXEL           endp
endif

_TEXT   ends

        end
