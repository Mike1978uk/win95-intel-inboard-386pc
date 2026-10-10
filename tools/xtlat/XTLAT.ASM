; XTLAT.COM - how long does the XT-CF's CF card take to get ready, wait by wait?
;
; Design by measurement: if the waits are known, the driver can wait silently (a register-only
; loop runs from the Inboard's L1 cache and makes no bus cycle) and only then poll, instead of
; polling throughout. This measures the distribution the silent wait would be sized from.
;
; Read-only. 64 READ SECTORS commands of 8 sectors at spread LBAs on the boot CF. For each command:
; polls of alternate status until BSY clear and DRQ set before each of the 8 sectors (wait 0 is the
; command latency, waits 1-7 the gaps between sectors), then polls until BSY clears after the last.
; One poll is one I/O read plus a short loop, ~5.8 us (M8BUS: 5.78 us per I/O byte read).
; Interrupts are off within a command only. Registers as XTIDEMP: base 300h, stride 2.
; Writes XTLAT.BIN: "XTL1", then per command: LBA (dword), 9 poll counts (words); a count of
; FFFFh means ERR was set and the command was abandoned.

        org     100h
        cpu     386

BASE            equ     300h
R_DATA          equ     BASE + 0
R_COUNT         equ     BASE + 2*2
R_LBA0          equ     BASE + 3*2
R_LBA1          equ     BASE + 4*2
R_LBA2          equ     BASE + 5*2
R_DRVHD         equ     BASE + 6*2
R_CMD           equ     BASE + 7*2
R_ALT           equ     BASE + 14*2
NCMD            equ     64
NSEC            equ     8
RES             equ     1000h
BUF             equ     3000h

        jmp     start

oname   db      'XTLAT.BIN', 0
m_ok    db      'XTLAT: done, results in XTLAT.BIN', 13, 10, '$'
m_out   db      'XTLAT: cannot write XTLAT.BIN', 13, 10, '$'
hdr     db      'XTL1'
lba     dd      0
cmdn    dw      0

start:  cld
        mov     di, RES
        mov     dword [lba], 1000
        mov     word [cmdn], 0
.cmd:   mov     eax, [lba]
        stosd
        cli
        call    idlewait
        mov     dx, R_COUNT
        mov     al, NSEC
        out     dx, al
        mov     eax, [lba]
        mov     dx, R_LBA0
        out     dx, al
        shr     eax, 8
        mov     dx, R_LBA1
        out     dx, al
        shr     eax, 8
        mov     dx, R_LBA2
        out     dx, al
        shr     eax, 8
        and     al, 0Fh
        or      al, 0E0h
        mov     dx, R_DRVHD
        out     dx, al
        mov     dx, R_CMD
        mov     al, 20h                 ; READ SECTORS
        out     dx, al

        mov     bx, NSEC
.sec:   call    drqwait
        stosw
        cmp     ax, 0FFFFh
        je      .abort
        push    di
        mov     di, BUF
        mov     dx, R_DATA
        mov     cx, 256
        rep     insw                    ; stride 2: base+1 is also data, two bytes a word
        pop     di
        dec     bx
        jnz     .sec
        call    bsywait
        stosw
        jmp     .next
.abort: dec     bx                      ; pad the record so every record is the same length
        jz      .padlast
        mov     ax, 0FFFFh
.pad:   stosw
        dec     bx
        jnz     .pad
.padlast:
        mov     ax, 0FFFFh
        stosw
.next:  sti
        add     dword [lba], 8191
        inc     word [cmdn]
        cmp     word [cmdn], NCMD
        jb      .cmd

        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, oname
        int     21h
        jc      .err
        mov     bx, ax
        mov     ah, 40h
        mov     cx, 4
        mov     dx, hdr
        int     21h
        mov     ah, 40h
        mov     cx, di
        sub     cx, RES
        mov     dx, RES
        int     21h
        mov     ah, 3Eh
        int     21h
        mov     dx, m_ok
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Poll until BSY clear and DRQ set. AX = polls, or FFFFh on ERR or timeout.
drqwait:
        mov     dx, R_ALT
        xor     cx, cx
.p:     in      al, dx
        inc     cx
        jz      .fail
        test    al, 80h
        jnz     .p
        test    al, 01h
        jnz     .fail
        test    al, 08h
        jz      .p
        mov     ax, cx
        ret
.fail:  mov     ax, 0FFFFh
        ret

; Poll until BSY clear. AX = polls.
bsywait:
        mov     dx, R_ALT
        xor     cx, cx
.p:     in      al, dx
        inc     cx
        jz      .t
        test    al, 80h
        jnz     .p
.t:     mov     ax, cx
        ret

; Make sure the drive is idle before a command (it should be, at a DOS prompt).
idlewait:
        mov     dx, R_ALT
        xor     cx, cx
.p:     in      al, dx
        test    al, 80h
        jz      .ok
        loop    .p
.ok:    ret
