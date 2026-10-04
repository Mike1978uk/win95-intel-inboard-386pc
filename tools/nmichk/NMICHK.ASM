; NMICHK.COM - report the XT's NMI sources before Windows starts, optionally mask NMI (#10).
;
; Port 62h bit 7 is the RAM parity check latch, bit 6 the I/O channel check latch
; (the Inboard raises the latter: Intel's "Parity Check 2"). Either one, with NMI
; enabled at port A0h, interrupts the CPU. Windows 95 on an XT does not use port A0h
; (its NMI mask is the AT's port 70h, which an XT decodes as port 60h), so whatever
; the BIOS left there stays in force under Windows.
;
;   NMICHK      report only
;   NMICHK /C   report, clear both latches, report again
;   NMICHK /M   as /C, then mask NMI (port A0h = 0) for the rest of the session -
;               parity errors and, on an XT, coprocessor errors go unreported
;
; Every run appends one line to C:\NMICHK.TXT, so the result survives a boot that
; never reaches the desktop:  NMICHK 61=48 62=34 after=34 mode=M
; Run it as the last line of AUTOEXEC.BAT, after IVT68FIX.
;
; Assemble: fasm NMICHK.ASM NMICHK.COM

        org     100h
        use16

start:  mov     byte [mode], '-'
        mov     si, 81h
scan:   lodsb
        cmp     al, 0Dh
        je      scanned
        cmp     al, '/'
        jne     scan
        lodsb
        and     al, 0DFh                ; upper case
        cmp     al, 'C'
        je      setmode
        cmp     al, 'M'
        jne     scan
setmode:
        mov     [mode], al

scanned:
        in      al, 61h
        mov     di, h61
        call    hex
        in      al, 62h
        mov     [first62], al
        mov     di, h62
        call    hex

        cmp     byte [mode], '-'
        je      noclear
; Pulse port 61h bits 4 and 5 high: that clears both latches. The other bits, and
; the enables as they were, are written back.
        cli
        in      al, 61h
        mov     ah, al
        or      al, 30h
        out     61h, al
        mov     al, ah
        out     61h, al
        cmp     byte [mode], 'M'
        jne     cleared
        xor     al, al
        out     0A0h, al
cleared:
        sti
noclear:
        in      al, 62h
        mov     di, hafter
        call    hex
        mov     al, [mode]
        mov     [mchar], al

        mov     dx, line                ; screen
        mov     ah, 09h
        int     21h
        mov     ah, 09h
        mov     dx, verdict_ok
        test    byte [first62], 0C0h
        jz      say
        mov     dx, verdict_set
say:    int     21h

; Append the line to C:\NMICHK.TXT (create it if missing).
        mov     ax, 3D01h
        mov     dx, fname
        int     21h
        jnc     opened
        mov     ah, 3Ch
        xor     cx, cx
        mov     dx, fname
        int     21h
        jc      done
opened: mov     bx, ax
        mov     ax, 4202h
        xor     cx, cx
        xor     dx, dx
        int     21h
        mov     ah, 40h
        mov     cx, line_len
        mov     dx, line
        int     21h
        mov     ah, 3Eh
        int     21h
done:   mov     ax, 4C00h
        int     21h

; AL -> two hex digits at DI
hex:    push    ax
        shr     al, 4
        call    nib
        pop     ax
        and     al, 0Fh
nib:    add     al, '0'
        cmp     al, '9'
        jbe     putdig
        add     al, 7
putdig: mov     [di], al
        inc     di
        ret

fname   db      'C:\NMICHK.TXT', 0
mode    db      '-'
first62 db      0
line    db      'NMICHK 61='
h61     db      '00 62='
h62     db      '00 after='
hafter  db      '00 mode='
mchar   db      '-', 0Dh, 0Ah
line_len = $ - line
        db      '$'
verdict_ok  db  'No parity or I/O channel check latched.', 0Dh, 0Ah, '$'
verdict_set db  'A check WAS latched (62h bit 7 = parity, bit 6 = I/O channel).', 0Dh, 0Ah, '$'
