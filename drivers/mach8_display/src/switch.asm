; INT 2Fh hook for DOS full-screen switching.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
; Follows Michal Necasek's sswhook.asm in boxv9x/vmdisp9x (MIT, see LICENSE-MIT.txt).
;
; Windows issues INT 2Fh 4001h when a DOS session takes the screen and 4002h
; when Windows gets it back. Anything else goes to the previous handler with
; registers untouched, which is why its address lives in the code segment:
; the jump is CS-relative and needs no data segment.

SCREEN_SWITCH_OUT   equ     4001h
SCREEN_SWITCH_IN    equ     4002h

public  _m8_old_int2f
public  M8_INT2F_HOOK

extrn   _m8_switch_out : near
extrn   _m8_switch_in : near

_DATA   segment public 'DATA'
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment public 'CODE'

_m8_old_int2f   dd      0

        .386
M8_INT2F_HOOK   proc    far
        assume  ds:nothing, es:nothing
        cmp     ax, SCREEN_SWITCH_OUT
        je      handle
        cmp     ax, SCREEN_SWITCH_IN
        je      handle
        jmp     cs:_m8_old_int2f

handle:
        push    ds
        push    es
        pushad
        mov     bx, DGROUP
        mov     ds, bx
        assume  ds:DGROUP
        ; Carry clear in the caller's flags: the switch is allowed. The flags
        ; sit above pushad (32), ES, DS (4) and the return address (4).
        mov     bp, sp
        and     word ptr ss:[bp + 40], NOT 1
        cmp     ax, SCREEN_SWITCH_OUT
        jne     going_in
        call    _m8_switch_out
        jmp     done
going_in:
        call    _m8_switch_in
done:
        popad
        pop     es
        pop     ds
        iret
M8_INT2F_HOOK   endp

_TEXT   ends

        end
