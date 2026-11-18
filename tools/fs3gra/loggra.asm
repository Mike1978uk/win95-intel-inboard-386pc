; LOGGRA - the tail of a logging CUSTOM.GRA for Microsoft Flight Simulator 3.0.
;
; build.py appends this to a copy of the owner's PS21.GRA (not in the repo), points the
; header's JMP at log_init and fixes the length word. The picture is PS21's own; this only
; counts what FS3 asks of a graphics driver, to confirm the interface in
; docs/indy500_fs_renderer_2026_11_18.md before a card driver is written.
;
; FS3 far-calls the GRA's init with its own DS, and init writes 17 far pointers into FS3's
; DS at 47Eh-4BEh. log_init runs the original init, keeps those pointers and puts a counting
; stub in each slot. Each stub keeps the caller's registers and flags and jumps on to the
; original, so return values (CF from 4B6h) are the original's. At shutdown (4BAh) the
; counters are written to FS3LOG.BIN in the current directory.
;
; Assemble: nasm -f bin -o loggra.bin -DBASE=<PS21 file length> -DOINIT=<original init>

        cpu     386
        bits    16
        org     BASE

NSLOT   equ     17
SLOTS   equ     047Eh                   ; FS3 DS: first far pointer
NFRAME  equ     512                     ; frames kept, 8 words each
NVERT   equ     512                     ; first vertices kept, 4 words each

log_init:
        push    cs
        call    OINIT                   ; the original init returns with RETF
        pushf
        push    ax
        push    si
        push    di
        xor     si, si
        mov     di, stubs
.slot:  mov     ax, [si + SLOTS]
        mov     [cs:orig + si], ax
        mov     ax, [si + SLOTS + 2]
        mov     [cs:orig + si + 2], ax
        mov     [si + SLOTS], di
        mov     [si + SLOTS + 2], cs
        add     si, 4
        add     di, STUBLEN
        cmp     si, NSLOT * 4
        jb      .slot
        mov     word [SLOTS + 15 * 4], shut_stub ; 4BAh: the log is written after it
        inc     word [cs:inits]
        pop     di
        pop     si
        pop     ax
        popf
        retf

; One stub a slot, all the same length: count, then on to the original.
%macro STUB 1
        pushf
        add     dword [cs:cnt + %1 * 4], 1
        call    near hook
.ret:   popf
        jmp     far [cs:orig + %1 * 4]
%endmacro

stubs:
%assign i 0
%rep NSLOT
stub_ %+ i:
        STUB    i
%assign i i + 1
%endrep
STUBLEN equ     (stub_1 - stub_0)

; Per-call extras. The stub's own return address says which slot called: (ret - stubs -
; offset of the call's end in a stub) / STUBLEN. Keeps every register.
hook:
        push    bp
        mov     bp, sp
        push    ax
        push    bx
        push    si
        mov     si, [bp + 2]                    ; return address inside the stub
        sub     si, stubs + (hook_ret_off)
        mov     ax, si
        xor     si, si
.div:   cmp     ax, STUBLEN
        jb      .got
        sub     ax, STUBLEN
        inc     si
        jmp     .div
.got:   mov     ax, [bp - 2]                    ; caller's AX back
        cmp     si, 11                          ; 4AAh: polygon vertex DX, CX, AX
        je      .vert
        cmp     si, 12                          ; 4AEh: polygon close
        je      .close
        cmp     si, 0                           ; 47Eh: present = one frame
        je      .frame
        cmp     si, 4                           ; 48Eh: mode set
        je      .mode
        jmp     .out
.vert:  inc     word [cs:poly_verts]
        mov     bx, [cs:nvert]
        cmp     bx, NVERT
        jae     .out
        shl     bx, 3
        mov     [cs:verts + bx], ax
        mov     [cs:verts + bx + 2], cx
        mov     [cs:verts + bx + 4], dx
        mov     ax, [cs:cnt + 12 * 4]
        mov     [cs:verts + bx + 6], ax         ; polygons closed so far
        inc     word [cs:nvert]
        jmp     .out
.close: mov     bx, [cs:poly_verts]
        cmp     bx, 31
        jbe     .h
        mov     bx, 31
.h:     shl     bx, 1
        inc     word [cs:vhist + bx]
        mov     word [cs:poly_verts], 0
        jmp     .out
.mode:  mov     bx, [cs:nmode]
        cmp     bx, 8
        jae     .out
        shl     bx, 2
        mov     [cs:modes + bx], ax
        mov     ax, [bp - 4]
        mov     [cs:modes + bx + 2], ax         ; BX
        inc     word [cs:nmode]
        jmp     .out
.frame: call    frame_rec
.out:   pop     si
        pop     bx
        pop     ax
        pop     bp
        ret
hook_ret_off equ (stub_0.ret - stub_0)

; A frame's counts since the last present: lines, vertices, polygons, pixels, text,
; row fills, horizon, mode/target switches.
frame_rec:
        push    di
        push    cx
        mov     di, [cs:nframe]
        cmp     di, NFRAME
        jae     .done
        shl     di, 4
        add     di, frames
        mov     ax, [cs:cnt + 9 * 4]            ; 4A2h line
        sub     ax, [cs:last + 0]
        mov     [cs:di + 0], ax
        mov     ax, [cs:cnt + 11 * 4]           ; 4AAh vertex
        sub     ax, [cs:last + 2]
        mov     [cs:di + 2], ax
        mov     ax, [cs:cnt + 12 * 4]           ; 4AEh polygon
        sub     ax, [cs:last + 4]
        mov     [cs:di + 4], ax
        mov     ax, [cs:cnt + 10 * 4]           ; 4A6h pixel
        sub     ax, [cs:last + 6]
        mov     [cs:di + 6], ax
        mov     ax, [cs:cnt + 8 * 4]            ; 49Eh text
        sub     ax, [cs:last + 8]
        mov     [cs:di + 8], ax
        mov     ax, [cs:cnt + 1 * 4]            ; 482h + 48Ah row fills
        add     ax, [cs:cnt + 3 * 4]
        sub     ax, [cs:last + 10]
        mov     [cs:di + 10], ax
        mov     ax, [cs:cnt + 2 * 4]            ; 486h horizon
        sub     ax, [cs:last + 12]
        mov     [cs:di + 12], ax
        mov     ax, [cs:cnt + 5 * 4]            ; 492h + 496h draw target
        add     ax, [cs:cnt + 6 * 4]
        sub     ax, [cs:last + 14]
        mov     [cs:di + 14], ax
        inc     word [cs:nframe]
.done:  mov     ax, [cs:cnt + 9 * 4]
        mov     [cs:last + 0], ax
        mov     ax, [cs:cnt + 11 * 4]
        mov     [cs:last + 2], ax
        mov     ax, [cs:cnt + 12 * 4]
        mov     [cs:last + 4], ax
        mov     ax, [cs:cnt + 10 * 4]
        mov     [cs:last + 6], ax
        mov     ax, [cs:cnt + 8 * 4]
        mov     [cs:last + 8], ax
        mov     ax, [cs:cnt + 1 * 4]
        add     ax, [cs:cnt + 3 * 4]
        mov     [cs:last + 10], ax
        mov     ax, [cs:cnt + 2 * 4]
        mov     [cs:last + 12], ax
        mov     ax, [cs:cnt + 5 * 4]
        add     ax, [cs:cnt + 6 * 4]
        mov     [cs:last + 14], ax
        pop     cx
        pop     di
        ret

; The shutdown slot (4BAh) has its own stub, installed by log_init: the original runs first
; (text mode back), then the log is written, so DOS is called with the screen restored.
shut_stub:
        add     dword [cs:cnt + 15 * 4], 1
        call    far [cs:orig + 15 * 4]
        pushf
        pusha
        push    ds
        push    cs
        pop     ds
        mov     ah, 3Ch                         ; create
        xor     cx, cx
        mov     dx, logname
        int     21h
        jc      .fail
        mov     bx, ax
        mov     ah, 40h                         ; write the whole log block
        mov     cx, logend - logstart
        mov     dx, logstart
        int     21h
        mov     ah, 3Eh
        int     21h
.fail:  pop     ds
        popa
        popf
        retf

logname db      "FS3LOG.BIN", 0

; ---- the log block, written as it stands ----
        align   16
logstart:
magic   db      "FS3GRA01"
inits   dw      0
nframe  dw      0
nvert   dw      0
nmode   dw      0
poly_verts dw   0
orig    times NSLOT dd 0                       ; the original far pointers, slot order 47Eh..4BEh
cnt     times NSLOT dd 0                       ; calls a slot
last    times 8 dw 0
vhist   times 32 dw 0                          ; polygons by vertex count (31 = 31 or more)
modes   times 8 dd 0                           ; AX, BX at each mode set
verts   times NVERT * 4 dw 0                   ; AX, CX, DX, polygons closed before it
frames  times NFRAME * 8 dw 0
logend:
