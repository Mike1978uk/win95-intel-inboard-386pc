' XTPROBE.BAS - XT-IDE register-map walk and IDENTIFY probe, real-mode DOS.
'
'   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #21).
'
'   v2, 2026-08-31.  v1 established two things before it died:
'
'     taskfile sweep +1=0 +2=0 +3=0 +4=0 +5=0 +6=92 +7=92 +8=8F +9=8F
'                    +A=1 +B=1 +C=E0 +D=E0 +E=50 +F=50
'
'   Every pair reads alike, and +07 (Status under the assumed map) disagrees
'   with +0E (Alternate Status).  Those are the same register in every ATA
'   device, so the assumed one-byte stride is wrong.  On a two-byte stride the
'   values are all sane: +0E=50h is DRDY|DSC and +0C=E0h is LBA|master.
'
'   v1 then hung waiting for DRQ - because it had written the IDENTIFY opcode
'   to the sector number register - and QBASIC raised Overflow on the wait
'   loop, DEFINT A-Z having made the counter a 32767-max integer.  Both are
'   fixed here: LONG counters, and the stride is MEASURED rather than assumed.
'
'   SAFETY.  Writes taskfile registers and issues IDENTIFY DEVICE (0ECh) only.
'   IDENTIFY takes no parameters and does not touch media.  Nothing here writes
'   to the disk.  Run from a bare DOS prompt.
'
'   RUN:  C:\DOS\QBASIC.EXE /RUN C:\XTPROBE.BAS

DEFINT A-Z
DIM buf(511)
BP = &H300

CLS
PRINT "XT-IDE probe v2   base = "; HEX$(BP); "h    (map walk, then IDENTIFY)"
PRINT

'---------------------------------------------------------------- part 1
PRINT "1. read sweep  (+00 skipped, reading data has side effects)"
PRINT "  ";
FOR i = 1 TO 15
    PRINT "+"; HEX$(i); "="; HEX$(INP(BP + i)); " ";
NEXT i
PRINT : PRINT

'---------------------------------------------------------------- part 2
' Write distinct values into the low taskfile and read the whole window back.
' Nothing here can issue a command: +07 is only the command register if the
' stride is one, and we do not write +07.
PRINT "2. write/readback walk - this decides the stride"
OUT BP + 2, &H11
OUT BP + 3, &H22
OUT BP + 4, &H33
OUT BP + 5, &H44
PRINT "  wrote 11h,22h,33h,44h to +02,+03,+04,+05"
PRINT "  ";
FOR i = 1 TO 15
    PRINT "+"; HEX$(i); "="; HEX$(INP(BP + i)); " ";
NEXT i
PRINT

r2 = INP(BP + 2): r3 = INP(BP + 3): r4 = INP(BP + 4): r5 = INP(BP + 5)
stride = 0
IF r2 = &H11 AND r3 = &H22 AND r4 = &H33 AND r5 = &H44 THEN stride = 1
IF r4 = &H44 AND r5 = &H44 AND r2 = r3 THEN stride = 2

IF stride = 1 THEN PRINT "  => STRIDE 1. Standard taskfile: reg N at base+N."
IF stride = 2 THEN PRINT "  => STRIDE 2. A0 not decoded: reg N at base+2N."
IF stride = 0 THEN
    PRINT "  => INCONCLUSIVE. Neither pattern matched - note the two lines above"
    PRINT "     and stop here; do not trust anything below."
    PRINT "     Trying stride 2 anyway, since the read sweep implies it."
    stride = 2
END IF
PRINT

'  register offsets, derived not assumed
S = stride
RDATA = 0
RSECC = 2 * S: RSECN = 3 * S: RCYLL = 4 * S: RCYLH = 5 * S
RDRVH = 6 * S: RCMD = 7 * S
PRINT "  data=+"; HEX$(RDATA); "  drive/head=+"; HEX$(RDRVH);
PRINT "  status/command=+"; HEX$(RCMD)
PRINT

'---------------------------------------------------------------- part 3
' Three candidate data transports.  IDENTIFY is repeatable, so each gets a
' fresh command rather than trying to re-read one buffer three ways.
FOR tr = 1 TO 3

    IF tr = 1 THEN PRINT "3. transport LATCH+8   low from +00, high from +08"
    IF tr = 2 THEN PRINT "3. transport PIO8      every byte from +00"
    IF tr = 3 THEN PRINT "3. transport LATCH+1   low from +00, high from +01"

    st = 0
    FOR t& = 1 TO 20000
        st = INP(BP + RCMD)
        IF (st AND &H80) = 0 THEN EXIT FOR
    NEXT t&
    IF (st AND &H80) <> 0 THEN
        PRINT "  status = "; HEX$(st); "h  *** BSY never cleared ***"
        GOTO NextTr
    END IF

    OUT BP + RDRVH, &HA0
    FOR t& = 1 TO 20000
        st = INP(BP + RCMD)
        IF (st AND &H80) = 0 THEN EXIT FOR
    NEXT t&
    PRINT "  drive/head readback = "; HEX$(INP(BP + RDRVH)); "h";
    PRINT "   status = "; HEX$(st); "h"

    OUT BP + RSECC, 0: OUT BP + RSECN, 0
    OUT BP + RCYLL, 0: OUT BP + RCYLH, 0
    OUT BP + RCMD, &HEC

    drq = 0
    FOR t& = 1 TO 40000
        st = INP(BP + RCMD)
        IF (st AND &H80) = 0 THEN
            IF (st AND &H1) <> 0 THEN EXIT FOR
            IF (st AND &H8) <> 0 THEN drq = 1: EXIT FOR
        END IF
    NEXT t&
    PRINT "  status after IDENTIFY = "; HEX$(st); "h";
    IF drq = 0 THEN
        IF (st AND &H1) <> 0 THEN
            PRINT "   ERR, error reg = "; HEX$(INP(BP + S)); "h"
        ELSE
            PRINT "   no DRQ, no ERR - command ignored at this offset"
        END IF
        GOTO NextTr
    END IF
    PRINT "   DRQ set"

    IF tr = 1 THEN
        FOR i = 0 TO 510 STEP 2
            buf(i) = INP(BP): buf(i + 1) = INP(BP + 8)
        NEXT i
    ELSEIF tr = 2 THEN
        FOR i = 0 TO 511
            buf(i) = INP(BP)
        NEXT i
    ELSE
        FOR i = 0 TO 510 STEP 2
            buf(i) = INP(BP): buf(i + 1) = INP(BP + 1)
        NEXT i
    END IF

    PRINT "  bytes 0-15:";
    FOR i = 0 TO 15
        PRINT " "; RIGHT$("0" + HEX$(buf(i)), 2);
    NEXT i
    PRINT
    PRINT "  word0="; HEX$(buf(1) * 256& + buf(0)); "h";
    PRINT "  cyl="; buf(3) * 256& + buf(2);
    PRINT "  hd="; buf(7) * 256& + buf(6);
    PRINT "  spt="; buf(13) * 256& + buf(12)

    a$ = "": b$ = ""
    FOR i = 54 TO 93
        c = buf(i): IF c < 32 OR c > 126 THEN c = 46
        a$ = a$ + CHR$(c)
    NEXT i
    FOR i = 54 TO 92 STEP 2
        c = buf(i + 1): IF c < 32 OR c > 126 THEN c = 46
        b$ = b$ + CHR$(c)
        c = buf(i): IF c < 32 OR c > 126 THEN c = 46
        b$ = b$ + CHR$(c)
    NEXT i
    PRINT "  model as read ["; a$; "]"
    PRINT "  model swapped ["; b$; "]"

    n = 0
    FOR i = 54 TO 93
        IF buf(i) > 32 AND buf(i) < 127 THEN n = n + 1
    NEXT i
    PRINT "  printable ="; n;
    IF n >= 4 THEN PRINT "  <== THIS TRANSPORT VALIDATES" ELSE PRINT "  <== fails"

NextTr:
    PRINT
NEXT tr

PRINT "Done. Report: the stride line, the drive/head readback, and which"
PRINT "transport (if any) says VALIDATES."
