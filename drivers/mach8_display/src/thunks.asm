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

; As DIBFWD, after M8PXFLUSH writes any screen-to-memory proxy back to its
; bitmap (screen.c): these entries read or write bitmap bits themselves.
PXFWD   macro   name
extrn   DIB_&name : far
public  name
name:
        call    far ptr M8PXFLUSH
        jmp     DIB_&name
        endm

_DATA   segment public 'DATA'
extrn   _m8_pdevice : dword
extrn   _m8_palettized : word
_DATA   ends

DGROUP  group   _DATA

_TEXT   segment public 'CODE'

extrn   M8PXFLUSH : far
extrn   M8PXREALIZE : far
extrn   DIB_RealizeObjectExt : far
extrn   DIB_DibBltExt : far

.386
assume  ds:nothing, es:nothing

DIBTHK  EnumObj,                _m8_pdevice
; RealizeObject(lpDestDev, iStyle, lpInObj, lpOutObj, lpTextXForm), Pascal: a
; pattern brush reads its bitmap, so M8PXREALIZE(iStyle, lpInObj) looks first.
public  RealizeObject
RealizeObject:
        push    bp
        mov     bp, sp
        push    word ptr [bp+18]
        push    word ptr [bp+16]
        push    word ptr [bp+14]
        call    far ptr M8PXREALIZE
        pop     bp
        mov     ax, DGROUP
        mov     es, ax
        assume  es:DGROUP
        pop     ecx
        push    _m8_pdevice
        push    ecx
        jmp     DIB_RealizeObjectExt
        assume  es:nothing

; DibBlt is GetDIBits / SetDIBits on a bitmap.
public  DibBlt
DibBlt:
        call    far ptr M8PXFLUSH
        mov     ax, DGROUP
        mov     es, ax
        assume  es:DGROUP
        pop     ecx
        push    _m8_palettized
        push    ecx
        jmp     DIB_DibBltExt
        assume  es:nothing
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
PXFWD   BitmapBits
DIBFWD  Inquire

_TEXT   ends

end
