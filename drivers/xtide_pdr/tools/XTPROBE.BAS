' XTPROBE.BAS - XT-IDE IDENTIFY probe, real-mode DOS.
'
'   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #21).
'
'   WHY THIS EXISTS.  Phase 1 of the Windows 95 port driver returned
'   "Init Failure port.pdr" in 2 bootlog ticks - too fast for any of its
'   400000-iteration status spins to have run.  That leaves two causes that
'   look identical from outside: either IOS handed the driver a zero I/O base,
'   or the card answered wrong.  This program removes IOS, CONFIGMG and the
'   device node from the question by doing the same IDENTIFY from a bare DOS
'   prompt, where the only variable left is the card.
'
'   WHAT IT DOES.  Issues IDENTIFY DEVICE (0ECh) to the master at 300h and
'   reads the answer back twice - once through each candidate transport -
'   then applies the same model-string test the driver uses.
'
'   SAFETY.  Reads only.  IDENTIFY DEVICE does not touch media and takes no
'   parameters.  Nothing here writes to the drive.  Run it from a DOS prompt
'   with nothing else accessing the disk.
'
'   RUN:  C:\DOS\QBASIC.EXE /RUN C:\XTPROBE.BAS
'         (add  > C:\XTPROBE.LOG  is NOT possible from QBASIC - see README;
'          photograph the screen, or note the three marked lines)

DEFINT A-Z
DIM buf(511)

BP = &H300

CLS
PRINT "XT-IDE IDENTIFY probe   base = "; HEX$(BP); "h"
PRINT "issue #21 - phase 1 returned Init Failure; this says whether the card is at fault"
PRINT

' Sweep the taskfile.  +00 is deliberately skipped: reading the data register
' has side effects, and 0306h/0307h reading alike is already on record as odd.
PRINT "taskfile sweep";
FOR i = 1 TO 15
    PRINT " +"; HEX$(i); "="; HEX$(INP(BP + i));
NEXT i
PRINT : PRINT

FOR mode = 1 TO 2

    IF mode = 1 THEN PRINT "--- transport 1: 16-bit latch  (low from +00, high from +08) ---"
    IF mode = 2 THEN PRINT "--- transport 2: 8-bit PIO     (every byte from +00) ---"

    ' 1. drive idle?
    st = 0
    FOR t = 1 TO 30000
        st = INP(BP + &HE)
        IF (st AND &H80) = 0 THEN EXIT FOR
    NEXT t
    IF (st AND &H80) <> 0 THEN
        PRINT "  altstat = "; HEX$(st); "  *** BSY never cleared - card not answering ***"
        GOTO NextMode
    END IF
    PRINT "  altstat before select = "; HEX$(st); "h"

    ' 2. select the master, and read the register back.  The driver fails here
    '    if bit 4 comes back set, so this line is a direct check on that path.
    OUT BP + 6, &HA0
    FOR t = 1 TO 30000
        st = INP(BP + &HE)
        IF (st AND &H80) = 0 THEN EXIT FOR
    NEXT t
    rb = INP(BP + 6)
    PRINT "  +06 readback after writing A0 = "; HEX$(rb); "h";
    IF (rb AND &H10) <> 0 THEN
        PRINT "   <== DEV BIT SET: the driver's readback check fails here"
    ELSE
        PRINT "   (DEV clear - driver's check passes)"
    END IF

    ' 3. IDENTIFY DEVICE
    OUT BP + 2, 0: OUT BP + 3, 0: OUT BP + 4, 0: OUT BP + 5, 0
    OUT BP + 7, &HEC

    drq = 0
    FOR t = 1 TO 60000
        st = INP(BP + &HE)
        IF (st AND &H80) = 0 THEN
            IF (st AND &H1) <> 0 THEN EXIT FOR
            IF (st AND &H8) <> 0 THEN drq = 1: EXIT FOR
        END IF
    NEXT t
    PRINT "  status after IDENTIFY = "; HEX$(st); "h";
    IF drq = 0 THEN
        IF (st AND &H1) <> 0 THEN
            PRINT "   ERR set, error reg = "; HEX$(INP(BP + 1)); "h"
        ELSE
            PRINT "   no DRQ, no ERR - command ignored"
        END IF
        GOTO NextMode
    END IF
    PRINT "   DRQ set, reading 512 bytes"

    ' 4. read the result through the transport under test
    IF mode = 1 THEN
        FOR i = 0 TO 510 STEP 2
            buf(i) = INP(BP)
            buf(i + 1) = INP(BP + 8)
        NEXT i
    ELSE
        FOR i = 0 TO 511
            buf(i) = INP(BP)
        NEXT i
    END IF

    PRINT "  first 16 bytes:";
    FOR i = 0 TO 15
        PRINT " "; RIGHT$("0" + HEX$(buf(i)), 2);
    NEXT i
    PRINT
    PRINT "  word0="; HEX$(buf(1) * 256 + buf(0)); "h";
    PRINT "  cyls="; buf(3) * 256 + buf(2);
    PRINT "  heads="; buf(7) * 256 + buf(6);
    PRINT "  spt="; buf(13) * 256 + buf(12)

    ' 5. the model string - bytes 54..93, the driver's validation field.
    '    ATA stores it byte-swapped within each word, so both orders are shown:
    '    the readable one tells us the transport AND the word order in one go.
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
    PRINT "  printable non-blank chars ="; n;
    IF n >= 4 THEN
        PRINT "  <== THIS TRANSPORT VALIDATES"
    ELSE
        PRINT "  <== fails the driver's test"
    END IF

NextMode:
    PRINT
NEXT mode

PRINT "Done.  The three lines that matter: the +06 readback, the status after"
PRINT "IDENTIFY, and which transport (if either) says VALIDATES."
