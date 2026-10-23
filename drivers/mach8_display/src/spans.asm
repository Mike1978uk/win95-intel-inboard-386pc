; Solid spans of one GDI scanline call, for m8_hw_scan_spans.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; Every port write flushes the L1, so after each one the next memory access is
; a line fill. In C this loop keeps its counters, clip and row on the stack and
; refetches them after every write; here they live in registers and the only
; memory read per span is GDI's x pair. The walk is the C loop's: records of
; x count, top, bottom, the x pairs and the count again (OS_POLYSCANLINE), or
; one record of (count, y) and x pairs (OS_SCANLINES). A one-row span is
; CUR_Y (when the row changes), CUR_X and SCAN_TO_X; a taller one a rectangle
; fill. The data path is m8_hw_fill_begin's.
;
; Registers through the loop:
;   EBX     clip left (low), clip right (high), right exclusive
;   DI      the row the engine holds; EDI high: spans drawn
;   BP      FIFO entries free; EBP high: rows in the band - 1
;   ES:SI   GDI's list; ESI high: the band's top row
;   CX      x pairs left in the record; ECX high: records left
;   EDX     high: offset of the next record

CUR_Y           equ     82E8h
CUR_X           equ     86E8h
MAJ_AXIS_PCNT   equ     96E8h
CMD             equ     9AE8h
MULTIFUNC_CNTL  equ     0BEE8h
SCAN_TO_X       equ     0CAEEh
CMD_FILL        equ     40F3h

; m8_sp (m8hw.c), word offsets
SP_W            equ     0       ; far pointer to the list
SP_RECS         equ     4
SP_POLY         equ     6
SP_COUNT        equ     8
SP_L            equ     10
SP_T            equ     12
SP_R            equ     14
SP_B            equ     16
SP_Y            equ     18
SP_ROOM         equ     20
SP_SPANS        equ     22

public  M8_SPANS_SOLID

_DATA   segment word public 'DATA'
        extrn   _m8_sp : word
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment byte public 'CODE'
        extrn   _m8_hw_drain : near

        .386
        assume  cs:_TEXT, ds:DGROUP

M8_SPANS_SOLID  proc    near
        push    es
        pushad
        les     si, dword ptr _m8_sp[SP_W]
        mov     bx, _m8_sp[SP_R]
        shl     ebx, 16
        mov     bx, _m8_sp[SP_L]
        xor     edi, edi
        mov     di, _m8_sp[SP_Y]
        xor     ebp, ebp
        mov     bp, _m8_sp[SP_ROOM]
        movzx   ecx, word ptr _m8_sp[SP_RECS]
        shl     ecx, 16
        cmp     word ptr _m8_sp[SP_POLY], 0
        jne     rec_loop

        ; OS_SCANLINES: the first point is (count, y), then x pairs.
        mov     cx, _m8_sp[SP_COUNT]
        dec     cx
        add     cx, cx
        mov     ax, es:[si + 2]
        mov     dx, ax
        inc     dx
        add     si, 4
        jmp     short have_rec

rec_loop:
        mov     cx, es:[si]             ; x count
        mov     dx, cx
        add     dx, dx
        add     dx, si
        add     dx, 8
        shl     edx, 16                 ; next record
        mov     ax, es:[si + 2]         ; top
        mov     dx, es:[si + 4]         ; bottom, exclusive
        add     si, 6

have_rec:
        cmp     ax, _m8_sp[SP_T]
        jge     top_ok
        mov     ax, _m8_sp[SP_T]
top_ok:
        cmp     dx, _m8_sp[SP_B]
        jle     bottom_ok
        mov     dx, _m8_sp[SP_B]
bottom_ok:
        cmp     ax, dx
        jge     rec_next
        sub     dx, ax
        dec     dx
        ror     esi, 16
        mov     si, ax
        ror     esi, 16
        ror     ebp, 16
        mov     bp, dx
        ror     ebp, 16
        shr     cx, 1
        jz      rec_next

pair_loop:
        mov     ax, es:[si]             ; x0, clipped to the left
        cmp     ax, bx
        jge     left_ok
        mov     ax, bx
left_ok:
        mov     dx, es:[si + 2]         ; x1, clipped to the right
        ror     ebx, 16
        cmp     dx, bx
        jle     right_ok
        mov     dx, bx
right_ok:
        ror     ebx, 16
        add     si, 4
        cmp     ax, dx
        jge     pair_next
        cmp     bp, 6
        jge     room_ok
        call    drain
room_ok:
        add     edi, 10000h
        test    ebp, 0FFFF0000h
        jnz     band

        ; One row: x1 to EAX high while AX carries the row and x0.
        ror     eax, 16
        mov     ax, dx
        ror     eax, 16
        ror     esi, 16
        cmp     di, si
        je      same_row
        mov     di, si
        ror     esi, 16
        xchg    ax, di
        mov     dx, CUR_Y
        out     dx, ax
        xchg    ax, di
        dec     bp
        jmp     short row_set
same_row:
        ror     esi, 16
row_set:
        mov     dx, CUR_X
        out     dx, ax
        ror     eax, 16
        mov     dx, SCAN_TO_X
        out     dx, ax
        sub     bp, 2
        jmp     short pair_next

        ; Several rows: a rectangle fill, which moves the engine's row.
band:
        sub     dx, ax
        dec     dx
        ror     eax, 16
        mov     ax, dx                  ; EAX high: width - 1
        ror     eax, 16
        mov     dx, CUR_X
        out     dx, ax
        ror     esi, 16
        mov     ax, si
        ror     esi, 16
        mov     dx, CUR_Y
        out     dx, ax
        ror     eax, 16
        mov     dx, MAJ_AXIS_PCNT
        out     dx, ax
        mov     eax, ebp
        shr     eax, 16                 ; MIN_AXIS is 0: the value is rows - 1
        mov     dx, MULTIFUNC_CNTL
        out     dx, ax
        mov     ax, CMD_FILL
        mov     dx, CMD
        out     dx, ax
        mov     di, 0FFFFh
        sub     bp, 5

pair_next:
        dec     cx
        jnz     pair_loop

rec_next:
        cmp     word ptr _m8_sp[SP_POLY], 0
        je      done
        mov     eax, edx
        shr     eax, 16
        mov     si, ax
        sub     ecx, 10000h
        test    ecx, 0FFFF0000h
        jnz     rec_loop

done:
        mov     _m8_sp[SP_Y], di
        mov     _m8_sp[SP_ROOM], bp
        shr     edi, 16
        mov     _m8_sp[SP_SPANS], di
        popad
        pop     es
        ret
M8_SPANS_SOLID  endp

; Room for a span: m8_hw_drain waits for the FIFO and returns the room. Every
; register but BP's low word comes back as it was; C may use any of them.
drain   proc    near
        push    es
        pushad
        call    _m8_hw_drain
        mov     bp, sp
        mov     ss:[bp + 8], ax         ; the saved EBP's low word
        popad
        pop     es
        ret
drain   endp

_TEXT   ends

        end
