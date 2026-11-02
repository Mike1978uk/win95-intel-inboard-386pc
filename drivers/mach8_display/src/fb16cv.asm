; A row of 5-6-5 pixels dithered to the card's 256 colours (fb16.c).
;
; Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
; GUI rows are mostly runs of one colour, and the 4x4 dither repeats every
; 4 pixels along a row, so a run of 8 or more costs four lookups and then
; dword stores of the 4-pixel pattern. Shorter runs take a lookup a pixel.
;
; M8_FB16_CONV: DS:SI = source words (DGROUP), ES:DI = destination bytes,
; CX = pixels (> 0), DS:BX = the row's tables (M8_FB16_CV in fb16.c),
; DX = phase of the first pixel (x & 3).
;
; M8_FB16_CV: +0 red, +8 green, +16 blue, +24 grey: a near pointer per phase
; to that phase's 32- or 64-entry part table; +32 the cube's index table;
; +34 8 bytes of scratch for the pattern.

public  M8_FB16_CONV
public  M8_FB16_ROWDIFF

_TEXT   segment para public 'CODE'

        .386
        assume  cs:_TEXT

; AL = the index for colour AX at phase DX; nothing else changes.
pixel   proc    near
        push    cx
        push    dx
        push    si
        push    di
        mov     si, dx
        shl     si, 1
        mov     cx, ax
        mov     di, cx
        and     di, 31                  ; blue
        mov     dx, cx
        shr     dx, 11                  ; red
        shr     cx, 5
        and     cx, 63                  ; green
        cmp     dx, di
        jne     colour
        mov     ax, cx
        shr     ax, 1
        cmp     ax, dx
        jne     colour
        mov     di, [bx+si+24]          ; a grey: red = blue = green / 2
        add     di, cx
        mov     al, [di]
        jmp     done
colour:
        push    di
        mov     di, [bx+si+0]
        add     di, dx
        mov     al, [di]
        mov     di, [bx+si+8]
        add     di, cx
        add     al, [di]
        pop     cx
        mov     di, [bx+si+16]
        add     di, cx
        add     al, [di]
        xor     ah, ah
        mov     di, [bx+32]
        add     di, ax
        mov     al, [di]
done:
        pop     di
        pop     si
        pop     dx
        pop     cx
        ret
pixel   endp

M8_FB16_CONV proc near
        push    eax
        push    ecx
        push    bp
        cld
        mov     bp, cx                  ; pixels left (BP is a count, not an address)
next:
        mov     cx, bp
        mov     ax, [si]
        ; The run: words equal to AX from SI on, at most CX.
        push    es
        push    di
        push    cx
        push    ds
        pop     es
        mov     di, si
        repe    scasw
        pop     di                      ; DI = the count before the scan
        je      whole
        inc     cx                      ; the scan stopped one past the run
whole:
        sub     di, cx                  ; DI = the run
        mov     cx, di
        pop     di
        pop     es
        sub     bp, cx                  ; CX = the run
        cmp     cx, 8
        jb      short_run

        ; Long run: the pattern for all four phases, twice over, then dword
        ; stores from the phase of its first pixel.
        push    dx
        push    cx
        mov     cx, 4
        xor     dx, dx
pat:
        mov     ax, [si]
        call    pixel
        push    si
        mov     si, dx
        mov     [bx+si+34], al
        mov     [bx+si+38], al
        pop     si
        inc     dx
        loop    pat
        pop     cx
        pop     dx
        push    si
        mov     si, dx
        mov     eax, [bx+si+34]
        pop     si
        push    cx
        shr     cx, 2
        rep     stosd
        pop     cx
        push    cx
        and     cx, 3
        jz      tail_done
tail:
        stosb
        shr     eax, 8
        loop    tail
tail_done:
        pop     cx
        jmp     advance

short_run:
        push    cx
        mov     ax, [si]
one:
        call    pixel
        stosb
        mov     ax, [si]
        inc     dx
        and     dx, 3
        loop    one
        pop     cx
        sub     dx, cx                  ; undone below, so both paths agree
advance:
        add     dx, cx
        and     dx, 3
        add     si, cx
        add     si, cx
        or      bp, bp
        jnz     next
        pop     bp
        pop     ecx
        pop     eax
        ret
M8_FB16_CONV endp

; M8_FB16_ROWDIFF: one row's pieces against their fingerprints, in one call.
; DS:BX = M8_FB16_RD (fb16.c): +0 frame buffer selector, +2 offset of the
; first piece (32-bit), +6/+8 selector:offset of its fingerprint, +10 pieces
; (> 0), +12 first changed piece (-1 if none), +14 last. A changed piece's
; fingerprint is updated. A fingerprint is FNV-1a over the piece's 16
; dwords, never 0: 0 marks a piece the card may not show as the frame buffer
; holds it. The multiply keeps it non-linear; a rotate-and-XOR hash is not,
; so an even number of fully flipped dwords (black text on white) would
; leave it unchanged.
M8_FB16_ROWDIFF proc near
        push    es
        push    fs
        push    esi
        push    eax
        push    ecx
        push    edx
        push    di
        mov     ax, [bx]
        mov     fs, ax
        mov     esi, [bx+2]
        mov     ax, [bx+6]
        mov     es, ax
        mov     di, [bx+8]
        mov     word ptr [bx+12], -1
        xor     dx, dx
rd_piece:
        mov     eax, 811C9DC5h
        mov     ecx, 16
rd_dw:
        xor     eax, fs:[esi]
        imul    eax, eax, 01000193h
        add     esi, 4
        dec     ecx
        jnz     rd_dw
        or      eax, 1
        cmp     eax, es:[di]
        je      rd_same
        mov     es:[di], eax
        cmp     word ptr [bx+12], -1
        jne     rd_last
        mov     [bx+12], dx
rd_last:
        mov     [bx+14], dx
rd_same:
        add     di, 4
        inc     dx
        cmp     dx, [bx+10]
        jb      rd_piece
        pop     di
        pop     edx
        pop     ecx
        pop     eax
        pop     esi
        pop     fs
        pop     es
        ret
M8_FB16_ROWDIFF endp

_TEXT   ends

        end
