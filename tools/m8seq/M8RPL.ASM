; M8RPL.COM - replay the first N writes of M8PRE.DAT (TEST.COM's accelerator writes before Test
; Sequence 1), then do M8TSX's exit (4AE8h = 2, 4AEEh = 1850h, DAC mask FFh). Bisects which write
; leaves the real card's screen black after M8TSX: run with a different N after each cold boot and
; look at the monitor.
;
;   M8RPL n         n = decimal count of writes to play (larger than the file plays them all)
;   M8RPL n S       the same, but skip every write to a port of 8000h or above (the drawing engine),
;                   so only the setup registers below 8000h are replayed
;
; Writes nothing to disk. Prints the number of writes played and the last port and value.

        org     100h

%macro  OUTW 2
        mov     dx, %1
        mov     ax, %2
        out     dx, ax
%endmacro

        jmp     start

fname   db      'M8PRE.DAT', 0
m_open  db      'M8RPL: cannot read M8PRE.DAT', 13, 10, '$'
m_done  db      'M8RPL: played $'
m_last  db      ' writes, last port $'
m_val   db      ' value $'
m_crlf  db      13, 10, '$'
want    dw      0
setup   db      0
count   dw      0
lastp   dw      0
lastv   dw      0

start:  cld
        mov     si, 81h                 ; decimal argument from the command tail
        xor     bx, bx
.sk:    lodsb
        cmp     al, ' '
        je      .sk
.dg:    cmp     al, '0'
        jb      .ok
        cmp     al, '9'
        ja      .ok
        sub     al, '0'
        xor     ah, ah
        xchg    ax, bx
        mov     cx, 10
        mul     cx
        add     bx, ax
        lodsb
        jmp     .dg
.ok:    mov     [want], bx
.s2:    cmp     al, ' '                 ; optional 'S' after the count
        jne     .s3
        lodsb
        jmp     .s2
.s3:    and     al, 0DFh
        cmp     al, 'S'
        jne     .s4
        mov     byte [setup], 1
.s4:

        mov     ax, 3D00h
        mov     dx, fname
        int     21h
        jc      openerr
        mov     bx, ax
        mov     ah, 3Fh
        mov     cx, 8000h
        mov     dx, BUF
        int     21h
        mov     ah, 3Eh
        int     21h

        mov     si, BUF
        lodsw                           ; entry count in the file
        mov     cx, ax
        cmp     cx, [want]
        jbe     .n
        mov     cx, [want]
.n:     jcxz    .exit
.w:     push    cx                      ; as M8TSX's prerun: FIFO room, byte or word as logged,
        call    fifo                    ; delay after the display and clock selects, drain after
        lodsw                           ; a read command
        mov     dx, ax                  ; port
        lodsw                           ; value
        mov     bl, [si]                ; width
        inc     si
        cmp     byte [setup], 0
        je      .pl
        test    dx, 8000h
        jz      .pl
        pop     cx                      ; drawing-engine write: skipped, not counted
        loop    .w
        jmp     .exit
.pl:    mov     [lastp], dx
        mov     [lastv], ax
        cmp     bl, 1
        jne     .ww
        out     dx, al
        jmp     short .af
.ww:    out     dx, ax
.af:    inc     word [count]
        cmp     dx, 4AE8h
        je      .dl
        cmp     dx, 4AEEh
        jne     .rd
.dl:    call    delay
        jmp     short .nx
.rd:    cmp     dx, 9AE8h
        jne     .nx
        test    al, 1
        jnz     .nx
        call    drain
.nx:    pop     cx
        loop    .w

.exit:  OUTW    4AE8h, 2                ; M8TSX's exit, unchanged
        OUTW    4AEEh, 1850h
        mov     dx, 2EAh
        mov     al, 0FFh
        out     dx, al

        mov     dx, m_done
        call    say
        mov     ax, [count]
        call    hex4
        mov     dx, m_last
        call    say
        mov     ax, [lastp]
        call    hex4
        mov     dx, m_val
        call    say
        mov     ax, [lastv]
        call    hex4
        mov     dx, m_crlf
        call    say
        mov     ax, 4C00h
        int     21h

openerr:
        mov     dx, m_open
        call    say
        mov     ax, 4C01h
        int     21h

; Read and drop PIX_TRANS words while GP_STAT says data is ready, up to 1024.
drain:  push    si
        call    dready
        mov     cx, 1024
.d:     mov     dx, 9AE8h
        in      ax, dx
        test    ax, 100h
        jz      .e
        mov     dx, 0E2E8h
        in      ax, dx
        loop    .d
.e:     pop     si
        ret

; Bounded waits, as M8TSX.
dready: push    cx
        mov     dx, 9AE8h
        xor     cx, cx
.d:     in      ax, dx
        test    ax, 100h
        jnz     .dok
        loop    .d
.dok:   pop     cx
        ret

fifo:   push    cx
        mov     dx, 9AEEh
        xor     cx, cx
.q:     in      ax, dx
        test    ax, 10h
        jz      .qok
        loop    .q
.qok:   pop     cx
        ret

delay:  push    cx
        mov     cx, 8000h
.dw:    loop    .dw
        pop     cx
        ret

say:    mov     ah, 9
        int     21h
        ret

hex4:   mov     cx, 4
.h:     rol     ax, 4
        push    ax
        and     al, 0Fh
        add     al, '0'
        cmp     al, '9'
        jbe     .p
        add     al, 7
.p:     mov     dl, al
        mov     ah, 2
        int     21h
        pop     ax
        loop    .h
        ret

BUF:
