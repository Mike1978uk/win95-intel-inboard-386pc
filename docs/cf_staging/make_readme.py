import io

txt = """BLUE LIGHTNING CPU CONTROL - test procedure     rev 2, 2026-08-24
=====================================================================
CORRECTION vs rev 1: there is NO "(F)ile Utilities" save/restore in
CTCHIP34. That text came from Cyrix's CX486.EXE, a different program,
and was quoted here in error. Registers must be READ and then SET
explicitly. Photos of the screens are the way to capture them.

Goal: get 2x clock back under Win95 without revto486.sys/lght486.sys,
which halt during the Win95 CONFIG.SYS load. (They work fine in DOS
6.22 and Win 3.11 - the halt is Win95-specific.)

CTCHIP34 is a real-mode EXE that runs and EXITS. Not a resident
CONFIG.SYS driver, so it does not hit that halt.

2x ONLY. 60 MHz part, proven stable at 83.5 MHz sustained with
heatsink and cooling. 3x is NOT used; that batch file was removed.

Registers reset on power cycle. Nothing is permanent until the CALL
line in AUTOEXEC.BAT is un-REMmed.

---------------------------------------------------------------------
STEP 1 - CAPTURE the known-good settings   (read only, changes nothing)
---------------------------------------------------------------------
    C:\\CTCHIP\\CPUSHOW.BAT

Page through with PgDn and photograph EVERY register page. Capture
each of these configurations SEPARATELY and label the photos:

    (a) DOS 6.22   WITH revto486.sys loaded      <- the reference
    (b) DOS 6.22   WITHOUT revto486.sys          <- the baseline
    (c) Win 3.11   as you normally run it
    (d) Win 95     as it is now (no revto486)    <- what we must fix

The registers that matter, all documented in IBM486.CFG:

    1000h:0  CE / DBCS / PMI / ASNP / SNP / A20M / CPC / CPE
    1000h:1  CNPX / LPH / XTOUT / CRLD / IKEN / DCLM
    1000h:2  LPPLA / BUSRD / CPGE
    1001h:0  LMCR  1MB cacheable  lo-byte
    1001h:1  LMCR  1MB cacheable  hi-byte
    1001h:2  LMROR 1MB read-only  lo-byte
    1001h:3  LMROR 1MB read-only  hi-byte
    1001h:4  CMLR  1-16MB cache limit
    1001h:5  ECMLR ..4GB limit lo-byte
    1001h:6  ECMLR ..4GB limit hi-byte
    1002h:3  CLOCK MODE  <- bits 2-0: 000=1x  011=2x  100=3x
    1004h:0  MOVS split / power saving cache
    1004h:3  OS2B / CR0 / CLP / NOP / NA16

Diffing (a) against (b) tells us exactly what revto486 changes, and
(a) against (d) tells us what Win95 is missing. That is the whole
answer, and it comes from YOUR board rather than a forum post about
somebody else's.

Note: whether (a) is OPTIMAL is a separate question from whether it is
KNOWN-GOOD. It is known-good - it has run this machine for years. Get
it replicated first; tune afterwards if at all.

---------------------------------------------------------------------
STEP 2 - the multiplier alone (safe to try now)
---------------------------------------------------------------------
    C:\\CTCHIP\\CPU2X.BAT

Re-run CPUSHOW and confirm 1002h:3 bits 2-0 now read 011.
Back out with C:\\CTCHIP\\CPU1X.BAT, or power cycle.

Expect the clock to change but NOT the cache benefit - the cache bits
are separate and come from step 1's capture.

---------------------------------------------------------------------
STEP 3 - replicate the cache settings
---------------------------------------------------------------------
Once the photos are compared, this file gets a CPUSET.BAT with the
exact CTCHIP34 lines. Syntax, for reference:

    CTCHIP34 IBM486 /1000h:0:=%%1xxxxx1x     (%% in a .BAT, % at prompt)
    x = leave bit alone, 0/1 = force

DO NOT hand-pick cache bits before the capture. Getting the snoop bits
(1000h:0 bits 3/4, SNP and ASNP) wrong with a DMA SCSI controller is
silent data corruption, not an error message.

---------------------------------------------------------------------
STEP 4 - make it stick
---------------------------------------------------------------------
Only after 1-3: edit AUTOEXEC.BAT and remove the REM from
    REM CALL C:\\CTCHIP\\CPU2X.BAT
Original AUTOEXEC.BAT is backed up as D:\\AUTOEXEC.PRE

---------------------------------------------------------------------
DO NOT add /MACHINE: switches to HIMEM.SYS on this machine. It must
stay switchless - HIMEM's default auto-detect happens to compute the
right value here because port 64h reads back 00h on this board.
/MACHINE:12 was tried before and had no effect. See project docs.
"""

io.open(r'D:\CTCHIP\README.TXT', 'w', encoding='latin-1', newline='\r\n').write(txt)
print('README.TXT rev 2 written')
