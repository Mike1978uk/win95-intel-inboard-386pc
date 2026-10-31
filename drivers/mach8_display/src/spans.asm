; Solid spans of one GDI scanline call, for m8_hw_scan_spans.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; Every port write flushes the L1, so after each one the code that follows and
; any data it touches are line fills, ~1.3 us a 16-byte line. A polygon's list
; is mostly one row a record with one x pair, three writes a record, so the
; cost is the lines run between them: the common path below is kept short and
; straight, and everything else (clipping, a short FIFO, rows that repeat,
; multi-row bands) is out of line after it. The state a record reads sits in
; one aligned line (hot_*), fetched once a record.
;
; The walk: records of x count, top, bottom, the x pairs and the count again
; (OS_POLYSCANLINE), or one record of (count, y) and x pairs (OS_SCANLINES).
; A one-row span is CUR_Y (when the row changes), CUR_X and SCAN_TO_X; a
; taller one a rectangle fill. The data path is m8_hw_fill_begin's. FIFO room
; is taken for a whole record before its first write.
;
; Registers through the walk: ES:SI GDI's list, BX clip left, BP clip right
; (exclusive), CX x pairs left in the record.

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

_DATA   segment para public 'DATA'
        extrn   _m8_sp : word
hot_t           dw      0       ; clip top
hot_b           dw      0       ; clip bottom, exclusive
hot_y           dw      0       ; the row the engine holds
hot_room        dw      0       ; FIFO entries free
hot_spans       dw      0
hot_recs        dw      0
hot_bt          dw      0       ; a band's top row
hot_br          dw      0       ; a band's rows - 1
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment para public 'CODE'
        extrn   _m8_hw_drain : near

        .386
        assume  cs:_TEXT, ds:DGROUP

M8_SPANS_SOLID  proc    near
        push    es
        pushad
        mov     ax, _m8_sp[SP_T]
        mov     hot_t, ax
        mov     ax, _m8_sp[SP_B]
        mov     hot_b, ax
        mov     ax, _m8_sp[SP_Y]
        mov     hot_y, ax
        mov     ax, _m8_sp[SP_ROOM]
        mov     hot_room, ax
        mov     hot_spans, 0
        mov     ax, _m8_sp[SP_RECS]
        mov     hot_recs, ax
        mov     bx, _m8_sp[SP_L]
        mov     bp, _m8_sp[SP_R]
        les     si, dword ptr _m8_sp[SP_W]
        cmp     word ptr _m8_sp[SP_POLY], 0
        jne     rec

        ; OS_SCANLINES: the first point is (count, y), then x pairs.
        mov     cx, _m8_sp[SP_COUNT]
        dec     cx
        jle     done
        mov     ax, es:[si + 2]
        mov     dx, ax
        inc     dx
        add     si, 4
        mov     hot_recs, 1
        jmp     have

        align   16
rec:
        mov     cx, es:[si]             ; x count
        mov     ax, es:[si + 2]         ; top
        mov     dx, es:[si + 4]         ; bottom, exclusive
        add     si, 6
        shr     cx, 1                   ; x pairs
have:
        cmp     ax, hot_t
        jl      clip_t
t_ok:
        cmp     dx, hot_b
        jg      clip_b
b_ok:
        sub     dx, ax                  ; rows
        jle     rec_skip
        jcxz    rec_end
        dec     dx
        jnz     band
        mov     di, cx
        add     di, di
        inc     di                      ; CUR_Y and two writes a span
        sub     hot_room, di
        jl      short_room
room_ok:
        add     hot_spans, cx
        cmp     ax, hot_y
        je      same_row
        mov     hot_y, ax
        mov     dx, CUR_Y
        out     dx, ax
span:
        mov     ax, es:[si]             ; x0
        mov     di, es:[si + 2]         ; x1, exclusive
        add     si, 4
        cmp     ax, bx
        jl      clip_l
l_ok:
        cmp     di, bp
        jg      clip_r
r_ok:
        cmp     ax, di
        jge     span_next
        mov     dx, CUR_X
        out     dx, ax
        mov     ax, di
        mov     dx, SCAN_TO_X
        out     dx, ax
span_next:
        dec     cx
        jnz     span
rec_end:
        add     si, 2                   ; the record's closing count
        dec     hot_recs
        jnz     rec

done:
        mov     ax, hot_y
        mov     _m8_sp[SP_Y], ax
        mov     ax, hot_room
        mov     _m8_sp[SP_ROOM], ax
        mov     ax, hot_spans
        mov     _m8_sp[SP_SPANS], ax
        popad
        pop     es
        ret

; Out of line from here.

clip_t:
        mov     ax, hot_t
        jmp     t_ok
clip_b:
        mov     dx, hot_b
        jmp     b_ok
clip_l:
        mov     ax, bx
        jmp     l_ok
clip_r:
        mov     di, bp
        jmp     r_ok

; The row is already set: give back the entry CUR_Y would have used.
same_row:
        inc     hot_room
        jmp     span

; A record outside the clip's rows: step over its pairs.
rec_skip:
        shl     cx, 2
        add     si, cx
        jmp     rec_end

; Too little room for the record: wait, and if the record is longer than the
; FIFO, draw it a span at a time.
short_room:
        add     hot_room, di
        call    drain
        sub     hot_room, di
        jge     room_ok
        add     hot_room, di
        add     hot_spans, cx
        cmp     ax, hot_y
        je      slow_span
        mov     hot_y, ax
        mov     dx, CUR_Y
        out     dx, ax
        dec     hot_room
slow_span:
        cmp     hot_room, 2
        jge     slow_room
        call    drain
slow_room:
        mov     ax, es:[si]
        mov     di, es:[si + 2]
        add     si, 4
        cmp     ax, bx
        jge     slow_l
        mov     ax, bx
slow_l:
        cmp     di, bp
        jle     slow_r
        mov     di, bp
slow_r:
        cmp     ax, di
        jge     slow_next
        mov     dx, CUR_X
        out     dx, ax
        mov     ax, di
        mov     dx, SCAN_TO_X
        out     dx, ax
        sub     hot_room, 2
slow_next:
        dec     cx
        jnz     slow_span
        jmp     rec_end

; Several rows: a rectangle fill a span, which moves the engine's row.
band:
        mov     hot_bt, ax
        mov     hot_br, dx              ; MIN_AXIS is 0: the value is rows - 1
        add     hot_spans, cx
band_span:
        cmp     hot_room, 5
        jge     band_room
        call    drain
band_room:
        mov     ax, es:[si]
        mov     di, es:[si + 2]
        add     si, 4
        cmp     ax, bx
        jge     band_l
        mov     ax, bx
band_l:
        cmp     di, bp
        jle     band_r
        mov     di, bp
band_r:
        cmp     ax, di
        jge     band_next
        sub     di, ax
        dec     di                      ; width - 1
        mov     dx, CUR_X
        out     dx, ax
        mov     ax, hot_bt
        mov     dx, CUR_Y
        out     dx, ax
        mov     ax, di
        mov     dx, MAJ_AXIS_PCNT
        out     dx, ax
        mov     ax, hot_br
        mov     dx, MULTIFUNC_CNTL
        out     dx, ax
        mov     ax, CMD_FILL
        mov     dx, CMD
        out     dx, ax
        sub     hot_room, 5
        mov     hot_y, 0FFFFh
band_next:
        dec     cx
        jnz     band_span
        jmp     rec_end
M8_SPANS_SOLID  endp

; m8_hw_drain waits for the FIFO and returns the room. Every register comes
; back as it was; C may use any of them.
drain   proc    near
        push    es
        pushad
        call    _m8_hw_drain
        mov     hot_room, ax
        popad
        pop     es
        ret
drain   endp

_TEXT   ends

        end
