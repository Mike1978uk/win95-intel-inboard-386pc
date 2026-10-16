; Forwards to the DIB Engine for the entry points that never touch the screen.
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
; The macros are Michal Necasek's, from boxv9x/vmdisp9x dibthunk.asm (MIT, see
; LICENSE-MIT.txt).

; DIB_xxxExt takes one extra last parameter: pop the return address, push the
; parameter, put the return address back, jump. AX, ECX and ES carry no
; Pascal arguments, so they are free.
DIBTHK  macro   name, param
extrn   DIB_&name&Ext : far
public  name
name:
        mov     ax, DGROUP
        mov     es, ax
        assume  es:DGROUP
        pop     ecx
        push    param
        push    ecx
        jmp     DIB_&name&Ext
        endm

DIBFWD  macro   name
extrn   DIB_&name : far
public  name
name:
        jmp     DIB_&name
        endm

_DATA   segment public 'DATA'
extrn   _m8_pdevice : dword
extrn   _m8_palettized : word
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment public 'CODE'

.386
assume  ds:nothing, es:nothing

DIBTHK  EnumObj,                _m8_pdevice
DIBTHK  RealizeObject,          _m8_pdevice
DIBTHK  DibBlt,                 _m8_palettized
DIBTHK  GetPalette,             _m8_pdevice
DIBTHK  SetPaletteTranslate,    _m8_pdevice
DIBTHK  GetPaletteTranslate,    _m8_pdevice

; The driver's SetCursor is M8SetCursor in C: windows.h declares USER's
; SetCursor with another signature.
extrn   M8SETCURSOR : far
public  SetCursor
SetCursor:
        jmp     M8SETCURSOR

DIBFWD  ColorInfo
DIBFWD  EnumDFonts
DIBFWD  DeviceMode
DIBFWD  GetCharWidth
DIBFWD  DeviceBitmap
DIBFWD  SetAttribute
DIBFWD  CreateDIBitmap
DIBFWD  SelectBitmap
DIBFWD  BitmapBits
DIBFWD  Inquire

_TEXT   ends

end
