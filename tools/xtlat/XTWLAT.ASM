; XTWLAT.COM - how long does the XT-CF's CF card take to accept and commit a write?
;
; The write half of XTLAT: the driver's silent wait (docs/xtcf_measured_wait.md) needs the write
; waits too, and a flash commit may be milliseconds where a read is 238 us.
;
; Writes the boot CF, but only with the data already there: each command reads 8 sectors, writes
; the same bytes back, and reads them again to compare, all with interrupts off. Refuses to run
; under Windows, which would be driving the same card. Commits SMARTDRV's write cache first, so
; no cached write can land after ours. Stops at the first error or mismatch.
;
; 32 commands of 8 sectors at LBAs 1000 + n x 8191, as XTLAT. Per command, polls of alternate status:
; before each sector's data (wait 0 is command to first DRQ), until BSY clears after the last sector
; (the commit), and for FLUSH CACHE (E7h; many CF cards reject it, recorded as FFFFh, not an error).
; One poll is one I/O read, ~5.8 us. Counts saturate at FFFEh; FFFFh means ERR, FFFDh a timeout.
; Writes XTWLAT.BIN: "XTW1", then per command: LBA (dword), 8 DRQ waits, commit, flush, verify (0 =
; read back identical) as words.

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
NCMD            equ     32
NSEC            equ     8
RES             equ     1000h
BUF             equ     3000h           ; the sectors as read
VBUF            equ     4000h           ; the same sectors read back after the write
POLL_MAX        equ     01000000h       ; ~97 s of polls: a timeout, not a slow card

        jmp     start

oname   db      'XTWLAT.BIN', 0
m_ok    db      'XTWLAT: done, results in XTWLAT.BIN', 13, 10, '$'
m_win   db      'XTWLAT: Windows is running - run it from plain DOS', 13, 10, '$'
m_stop  db      'XTWLAT: STOPPED on an error or mismatch - the last record in XTWLAT.BIN has the LBA', 13, 10, '$'
m_out   db      'XTWLAT: cannot write XTWLAT.BIN', 13, 10, '$'
hdr     db      'XTW1'
lba     dd      0
cmdn    dw      0
stopped db      0

start:  cld
        mov     ax, 1600h               ; Windows enhanced mode? AL is 0 or 80h when it is not
        int     2Fh
        test    al, 7Fh
        jz      .nowin
        mov     dx, m_win
        jmp     .say
.nowin: mov     ax, 4A10h               ; SMARTDRV: commit its write cache (harmless if absent)
        mov     bx, 1
        int     2Fh

        mov     di, RES
        mov     dword [lba], 1000
        mov     word [cmdn], 0
.cmd:   mov     eax, [lba]
        stosd
        cli
        call    idlewait

        mov     al, 20h                 ; READ SECTORS: what is there now
        call    issue
        mov     si, BUF
        call    readn
        jc      .stop

        mov     al, 30h                 ; WRITE SECTORS: the same bytes back
        call    issue
        mov     si, BUF
        mov     bx, NSEC
.ws:    call    drqwait
        stosw
        cmp     ax, 0FFFDh
        jae     .stop
        mov     dx, R_DATA
        mov     cx, 256
        rep     outsw                   ; stride 2: base+1 is also data, two bytes a word
        dec     bx
        jnz     .ws
        call    bsywait                 ; the commit
        stosw
        cmp     ax, 0FFFDh
        jae     .stop

        mov     al, 0E7h                ; FLUSH CACHE
        call    issue
        call    bsywait
        stosw
        cmp     ax, 0FFFDh              ; ERR here is the card declining, not a failure
        je      .stop

        mov     al, 20h                 ; read back and compare
        call    issue
        mov     si, VBUF
        call    readn
        jc      .stop
        push    di
        mov     si, BUF
        mov     di, VBUF
        mov     cx, NSEC * 256
        repe    cmpsw
        pop     di
        setne   al
        movzx   ax, al
        stosw
        or      ax, ax
        jnz     .stop

        sti
        add     dword [lba], 8191
        inc     word [cmdn]
        cmp     word [cmdn], NCMD
        jb      .cmd
        jmp     .save

.stop:  sti
        mov     byte [stopped], 1
.save:  mov     ah, 3Ch
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
        cmp     byte [stopped], 0
        je      .say
        mov     dx, m_stop
        jmp     .say
.err:   mov     dx, m_out
.say:   mov     ah, 9
        int     21h
        mov     ax, 4C00h
        int     21h

; Set up NSEC sectors at [lba] on the master and issue command AL.
issue:  push    ax
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
        pop     ax
        mov     dx, R_CMD
        out     dx, al
        ret

; Read NSEC sectors into [si] (counts not kept: XTLAT has them). CF set on ERR or timeout.
readn:  push    di
        mov     di, si
        mov     bx, NSEC
.s:     call    drqwait
        cmp     ax, 0FFFDh
        jae     .f
        mov     dx, R_DATA
        mov     cx, 256
        rep     insw
        dec     bx
        jnz     .s
        call    bsywait
        cmp     ax, 0FFFDh
        jae     .f
        pop     di
        clc
        ret
.f:     pop     di
        stc
        ret

; Poll until BSY clear and DRQ set. AX = polls (saturated), FFFFh on ERR, FFFDh on timeout.
drqwait:
        mov     dx, R_ALT
        xor     ecx, ecx
.p:     in      al, dx
        inc     ecx
        cmp     ecx, POLL_MAX
        ja      .t
        test    al, 80h
        jnz     .p
        test    al, 01h
        jnz     .e
        test    al, 08h
        jz      .p
        jmp     sat
.e:     mov     ax, 0FFFFh
        ret
.t:     mov     ax, 0FFFDh
        ret

; Poll until BSY clear. AX = polls (saturated), FFFFh if ERR is then set, FFFDh on timeout.
bsywait:
        mov     dx, R_ALT
        xor     ecx, ecx
.p:     in      al, dx
        inc     ecx
        cmp     ecx, POLL_MAX
        ja      .t
        test    al, 80h
        jnz     .p
        test    al, 01h
        jz      sat
        mov     ax, 0FFFFh
        ret
.t:     mov     ax, 0FFFDh
        ret

sat:    cmp     ecx, 0FFFEh
        jbe     .ok
        mov     ecx, 0FFFEh
.ok:    mov     ax, cx
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
