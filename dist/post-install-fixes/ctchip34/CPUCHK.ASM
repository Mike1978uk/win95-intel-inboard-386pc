; CPUCHK.COM - checks the Blue Lightning registers CPUSET.BAT writes, one line of output.
; Reads only (RDMSR, real mode; no EMM386 on this machine). Exit code 0 = all as set, 1 = not.
; Build: wasm -q CPUCHK.ASM, then wlink format dos com file CPUCHK.OBJ name CPUCHK.COM
        .386
_TEXT   segment use16 public 'CODE'
        assume  cs:_TEXT, ds:_TEXT
        org     100h
start:
        mov     si, offset table
check:
        mov     cx, [si]                ; MSR number, 0 ends the table
        jcxz    good
        movzx   ecx, cx
        db      0Fh, 32h                ; rdmsr: edx:eax
        mov     bl, [si+2]              ; byte 0-7 of edx:eax
        cmp     bl, 4
        jb      low4
        mov     eax, edx
        sub     bl, 4
low4:   mov     cl, bl
        shl     cl, 3
        shr     eax, cl
        cmp     al, [si+3]
        jne     bad
        add     si, 4
        jmp     check
good:
        mov     dx, offset msg_ok
        mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h
bad:
        push    ax
        mov     dx, offset msg_bad
        mov     ah, 9
        int     21h
        mov     ax, [si]
        call    hex16
        mov     dl, 'h'
        call    putc
        mov     dl, ':'
        call    putc
        mov     dl, [si+2]
        add     dl, '0'
        call    putc
        mov     dl, '='
        call    putc
        pop     ax
        call    hex8
        mov     dx, offset msg_want
        mov     ah, 9
        int     21h
        mov     al, [si+3]
        call    hex8
        mov     dx, offset msg_end
        mov     ah, 9
        int     21h
        mov     ax, 4C01h
        int     21h

hex16:  push    ax
        mov     al, ah
        call    hex8
        pop     ax
hex8:   push    ax
        shr     al, 4
        call    hex4
        pop     ax
hex4:   and     al, 0Fh
        add     al, '0'
        cmp     al, '9'
        jbe     h4
        add     al, 7
h4:     mov     dl, al
putc:   push    ax
        mov     ah, 2
        int     21h
        pop     ax
        ret

; MSR, byte, value: as CPUSET.BAT writes them (CTCHIP's 1000h:n is byte n of edx:eax).
table   dw      1000h
        db      0, 92h                  ; cache on, ADS# snooping
        dw      1000h
        db      1, 8Ch                  ; XTOUT clear
        dw      1001h
        db      0, 0FFh                 ; LMCR: low 640 KB cacheable
        dw      1001h
        db      1, 03h
        dw      1001h
        db      4, 0F0h                 ; CMLR: 1-16 MB cacheable
        dw      1002h
        db      3, 03h                  ; clock 2x
        dw      0
msg_ok  db      'CPU OK: 2x, L1 on, 1-16 MB cached', 13, 10, '$'
msg_bad db      'CPU NOT SET: $'
msg_want db     ' want $'
msg_end db      ' - run C:\CTCHIP\CPUSET.BAT', 13, 10, '$'
_TEXT   ends
        end     start
