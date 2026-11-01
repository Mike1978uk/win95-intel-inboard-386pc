; BitBlt's solid fills and screen-to-screen copies on the screen (screen.c,
; m8_blt_short), when the engine already holds the data path for the mix.
; MAJ/MIN go only when the size changes (M8_MM_FORGET).
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; As pixel.asm: each call starts with the L1 empty, so the cost is the lines
; touched. Every check comes before the first write; anything this does not
; take goes to BitBltC with the frame untouched, which runs m8_blt_short and
; then the full path. The checks are m8_blt_short's, m8_hw_fill_fast's and
; m8_hw_copy_fast's.

FRGD_COLOR      equ     0A6E8h
CUR_X           equ     86E8h
CUR_Y           equ     82E8h
DESTX_DIASTP    equ     8EE8h
DESTY_AXSTP     equ     8AE8h
MAJ_AXIS_PCNT   equ     96E8h
MULTIFUNC_CNTL  equ     0BEE8h
CMD             equ     9AE8h
CMD_FILL        equ     40F3h
CMD_BLIT_XY     equ     0C0F3h          ; M8_CMD_BLIT, x and y increasing
CMD_INC_X       equ     0020h
CMD_INC_Y       equ     0080h
MIX_SRC_FRGD    equ     0020h
MIX_COPY        equ     0067h           ; M8_MIX_SRC_VRAM | M8_MIX_REPLACE

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
H_CUR_L         equ     16
H_CUR_T         equ     18
H_CUR_R         equ     20
H_CUR_B         equ     22
H_MM_W          equ     24
H_MM_H          equ     26
H_OK_COLOR      equ     30
H_VAL_COLOR     equ     44

DE_FLAGS        equ     28              ; DIBENGINE.deFlags
PALETTE_XLAT    equ     1000h
BR_FLAGS        equ     0               ; DIB_Brush8
BR_STYLE        equ     2
BR_BITS         equ     78
COLORSOLID      equ     01h

; Pascal far frame after push bp
A_DM            equ     6
A_BRUSH         equ     10
A_ROP           equ     14
A_H             equ     18
A_W             equ     20
A_SY            equ     22
A_SX            equ     24
A_SRC           equ     26
A_DY            equ     30
A_DX            equ     32
A_DEV           equ     34
L_CMD           equ     -8              ; below the saved ds, si, di

ifdef M8_PROF
M8_TIMED_ENTRY  equ     1
endif
ifdef M8_TRACE
M8_TIMED_ENTRY  equ     1
endif

ifdef M8_TIMED_ENTRY
public  M8T_BITBLT
else
public  BITBLT
endif

_DATA   segment para public 'DATA'
        extrn   _m8_hot : byte
        extrn   _m8_mix_for_key : byte
ifdef M8_PROF
        extrn   _m8_pf : word
endif
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment para public 'CODE'
        extrn   BITBLTC : far
        extrn   m8_leave_ : near

        .386
        assume  cs:_TEXT, ds:DGROUP

        align   16
ifdef M8_TIMED_ENTRY
M8T_BITBLT      proc    far
else
BITBLT          proc    far
endif
        push    bp
        mov     bp, sp
        push    ds
        push    si
        push    di
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
        mov     si, [bp + A_W]
        test    si, si
        jle     slow
        mov     di, [bp + A_H]
        test    di, di
        jle     slow
        les     bx, dword ptr _m8_hot[H_SCREEN]
        test    word ptr es:[bx + DE_FLAGS], PALETTE_XLAT
        jnz     slow
        mov     al, [bp + A_ROP + 2]
        cmp     al, 0CCh
        je      copy
        mov     ah, al                  ; a source other than the screen
        shr     ah, 2
        xor     ah, al
        and     ah, 33h
        jnz     slow
        xor     cl, cl                  ; the colour, 0 with no pattern
        mov     ah, al
        shr     ah, 4
        xor     ah, al
        and     ah, 0Fh
        jz      key
        les     bx, [bp + A_BRUSH]      ; a pattern: only a solid one
        mov     dx, es
        or      dx, bx
        jz      slow
        test    byte ptr es:[bx + BR_FLAGS], COLORSOLID
        jz      slow
        mov     dx, es:[bx + BR_STYLE]
        cmp     dx, 1
        je      slow
        cmp     dx, 3
        ja      slow
        mov     cl, es:[bx + BR_BITS]
key:
        mov     ah, al                  ; (P,D) results: rop bits 0, 1, 4, 5
        and     ah, 3
        shr     al, 2
        and     al, 0Ch
        or      al, ah
        movzx   bx, al
        movzx   bx, _m8_mix_for_key[bx]
        or      bx, MIX_SRC_FRGD        ; bx = the mix
        mov     ax, [bp + A_DX]
        mov     dx, [bp + A_DY]
        cmp     _m8_hot[H_CUR_SHOWN], 0
        jne     fcursor
fclear:
        mov     _m8_hot[H_BUSY], 1      ; before the engine state is read
        call    engine
        jne     slowb
        cmp     bx, word ptr _m8_hot[H_DP_MIX]
        jne     slowb
        cmp     _m8_hot[H_OK_COLOR], 0
        je      slowb
        cmp     word ptr _m8_hot[H_ROOM], 6
        jl      slowb
        mov     bx, ax                  ; x
        movzx   ax, cl
        cmp     ax, word ptr _m8_hot[H_VAL_COLOR]
        jne     colour
fwritten:
        sub     word ptr _m8_hot[H_ROOM], 3
        mov     cx, dx                  ; y
        mov     ax, bx
        mov     dx, CUR_X
        out     dx, ax
        mov     ax, cx
        mov     dx, CUR_Y
        out     dx, ax
        call    mm_size
        mov     ax, CMD_FILL
        mov     dx, CMD
        out     dx, ax
leave_:
ifdef M8_PROF
        inc     _m8_pf[6]               ; hits
endif
        cmp     _m8_hot[H_CUR_PENDING], 0
        jne     pending
        mov     _m8_hot[H_BUSY], 0
done:
        mov     ax, 1
        pop     di
        pop     si
        pop     ds
        pop     bp
        retf    32

colour:
        mov     word ptr _m8_hot[H_VAL_COLOR], ax
        dec     word ptr _m8_hot[H_ROOM]
        push    dx
        mov     dx, FRGD_COLOR
        out     dx, ax
        pop     dx
        jmp     fwritten

fcursor:                                ; l = x, t = y, r = x + w, b = y + h
        cmp     ax, word ptr _m8_hot[H_CUR_R]
        jge     fclear
        push    ax
        add     ax, si
        cmp     word ptr _m8_hot[H_CUR_L], ax
        pop     ax
        jge     fclear
        cmp     dx, word ptr _m8_hot[H_CUR_B]
        jge     fclear
        push    dx
        add     dx, di
        cmp     word ptr _m8_hot[H_CUR_T], dx
        pop     dx
        jge     fclear
        jmp     slow
slowb:
        mov     _m8_hot[H_BUSY], 0
slow:
        pop     di
        pop     si
        pop     ds
        pop     bp
        jmp     BITBLTC

pending:
        call    m8_leave_
        jmp     done

; ZF set when the engine holds m8_plain's data path with counted room.
engine:
        cmp     _m8_hot[H_HW_OFF], 0
        jne     engine_x
        cmp     _m8_hot[H_DP_OK], 0
        je      engine_no
        cmp     _m8_hot[H_DP_PLAIN], 0
        je      engine_no
        cmp     _m8_hot[H_ROOM_OK], 0
        je      engine_no
        cmp     al, al
engine_x:
        retn
engine_no:
        or      sp, sp                  ; ZF clear: sp is never 0
        retn

; MAJ/MIN = si x di, written only when the engine holds another size.
mm_size:
        cmp     si, word ptr _m8_hot[H_MM_W]
        jne     mm_size_w
        cmp     di, word ptr _m8_hot[H_MM_H]
        je      mm_size_x
mm_size_w:
        mov     word ptr _m8_hot[H_MM_W], si
        mov     word ptr _m8_hot[H_MM_H], di
        sub     word ptr _m8_hot[H_ROOM], 2
        lea     ax, [si - 1]
        mov     dx, MAJ_AXIS_PCNT
        out     dx, ax
        lea     ax, [di - 1]
        mov     dx, MULTIFUNC_CNTL      ; MIN_AXIS
        out     dx, ax
mm_size_x:
        retn

copy:
        mov     ax, [bp + A_SRC]
        cmp     ax, word ptr _m8_hot[H_SCREEN]
        jne     slow
        mov     ax, [bp + A_SRC + 2]
        cmp     ax, word ptr _m8_hot[H_SCREEN + 2]
        jne     slow
        cmp     _m8_hot[H_CUR_SHOWN], 0
        jne     ccursor
cclear:
        mov     _m8_hot[H_BUSY], 1      ; before the engine state is read
        call    engine
        jne     slowb
        cmp     word ptr _m8_hot[H_DP_MIX], MIX_COPY
        jne     slowb
        cmp     word ptr _m8_hot[H_ROOM], 7
        jl      slowb
        mov     ax, [bp + A_DX]
        mov     dx, [bp + A_DY]
        mov     cx, [bp + A_SX]
        mov     bx, [bp + A_SY]
        push    CMD_BLIT_XY
        cmp     dx, bx
        jle     notdown
        and     word ptr [bp + L_CMD], not CMD_INC_Y    ; bottom up
        add     bx, di
        add     dx, di
        dec     bx
        dec     dx
        jmp     dir
notdown:
        jne     dir
        cmp     ax, cx
        jle     dir
        and     word ptr [bp + L_CMD], not CMD_INC_X    ; right to left
        add     cx, si
        add     ax, si
        dec     cx
        dec     ax
dir:
        sub     word ptr _m8_hot[H_ROOM], 5
        push    dx                      ; destination y
        xchg    ax, cx                  ; ax = source x, cx = destination x
        mov     dx, CUR_X
        out     dx, ax
        mov     ax, bx
        mov     dx, CUR_Y
        out     dx, ax
        mov     ax, cx
        mov     dx, DESTX_DIASTP
        out     dx, ax
        pop     ax
        mov     dx, DESTY_AXSTP
        out     dx, ax
        call    mm_size
        pop     ax
        mov     dx, CMD
        out     dx, ax
        jmp     leave_

ccursor:                                ; the union of source and destination
        mov     ax, [bp + A_DX]
        mov     cx, [bp + A_SX]
        mov     dx, ax
        cmp     cx, ax
        jge     cx_max
        mov     ax, cx                  ; ax = left
        jmp     cx_done
cx_max:
        mov     dx, cx                  ; dx = the larger x
cx_done:
        cmp     ax, word ptr _m8_hot[H_CUR_R]
        jge     cclear
        add     dx, si
        cmp     word ptr _m8_hot[H_CUR_L], dx
        jge     cclear
        mov     ax, [bp + A_DY]
        mov     cx, [bp + A_SY]
        mov     dx, ax
        cmp     cx, ax
        jge     cy_max
        mov     ax, cx
        jmp     cy_done
cy_max:
        mov     dx, cx
cy_done:
        cmp     ax, word ptr _m8_hot[H_CUR_B]
        jge     cclear
        add     dx, di
        cmp     word ptr _m8_hot[H_CUR_T], dx
        jge     cclear
        jmp     slow
ifdef M8_TIMED_ENTRY
M8T_BITBLT      endp
else
BITBLT          endp
endif

_TEXT   ends

        end
