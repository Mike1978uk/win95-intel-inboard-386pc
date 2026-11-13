;============================================================================
; XTIDETR.ASM - XT-IDE transport for a Windows 95 SCSI miniport
;
;   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #21).
;   Written for the IBM 5160 + Intel Inboard 386/PC, but deliberately not
;   specific to it: the register stride and the data transport are both
;   selectable, because XTIDE Universal BIOS drives a family of electrically
;   different adapters and a driver that hardcodes one of them is a driver for
;   one machine.
;
;   The transport only: register map, waits, data phase, probe. The SRB
;   side is XTIDEMP.ASM. See docs/scsi_miniport_costing.md.
;
;   Register map cross-checked against 86Box's src/disk/hdc_xtide.c and against
;   the XT-IDE port of sava/lpproj's patacd.asm by Miran Grca (@OBattler).
;   NOTE those two are not independent - same author - and OBattler's XT
;   transfer loops are broken (they never issue the second IN), so use them for
;   the register map and the write ordering only. See drivers/xtide_cdrom/.
;============================================================================

	page	,132
	title	xtidetr - XT-IDE transport

	.386
	.MODEL	FLAT, STDCALL
	OPTION	CASEMAP:NONE

;  VMM.INC supplied OFFSET32; flat model does not need the distinction.
OFFSET32	EQU	<OFFSET>


;============================================================================
;  Register INDEX, not offset. The byte offset is index * XTIDE_Stride.
;
;  MEASURED on the real card 2026-08-31, over COMrade, by write/readback:
;  wrote 11h,22h,33h,44h to base+2,+3,+4,+5 and read the window back.
;  base+2 and base+3 both returned 00h (one register, write-only side);
;  base+4 and base+5 both returned 44h (one register, last write wins).
;  A0 is not decoded on this card. Register N lives at base + 2N:
;
;      300 data      302 err/feat   304 sec count  306 sec number
;      308 cyl low   30A cyl high   30C drive/head 30E status/command
;
;  The earlier one-byte-stride assumption was disproved by a single read pair:
;  base+7 (Status) and base+0Eh (Alternate Status) are the same register in
;  every ATA device and must agree. They read 92h and 50h. See technique 78.
;============================================================================
XTR_DATA	equ	0
XTR_ERROR	equ	1		; read
XTR_FEATURES	equ	1		; write
XTR_SECCNT	equ	2
XTR_SECNUM	equ	3
XTR_CYLLO	equ	4
XTR_CYLHI	equ	5
XTR_DRVHD	equ	6
XTR_STATUS	equ	7		; read
XTR_CMD		equ	7		; write

;  The control block is a SECOND strided bank, and the ROM stores its address
;  unstrided: wControlBlockPort = 0308h means base+8 in register indices, which
;  at stride 2 is base+10h. Alternate status is control-block register 6, so it
;  lands at base + (8+6)*stride = base + 14*stride.
;
;  MEASURED 2026-08-31: 0x310-0x31A all read FFh (unimplemented), 0x31C reads
;  50h, matching the status register exactly. base + 14*2 = 31Ch.
;  The same formula gives base+0Eh at stride 1, which is where a classic XTIDE
;  puts it. One expression, both cards.
XTR_ALTSTATUS	equ	14		; read
XTR_DEVCTRL	equ	14		; write

;  The high-byte latch, for cards that have one, is a fixed card-level address
;  and is NOT part of the strided taskfile. This card does not have one:
;  a marker written to base+8 (5Ah) read straight back mid-DRQ, so base+8 is
;  cylinder low, not a latch.
XT_HILATCH	equ	8

ATA_IDENTIFY	equ	0ECh
ATA_READ	equ	020h		; READ SECTOR(S), the 28-bit PIO command
ATA_WRITE	equ	030h		; WRITE SECTOR(S)

;  IDENTIFY DEVICE word offsets, in bytes. Word N is at 2N.
ID_CYLS		equ	2		; word 1  - logical cylinders
ID_HEADS	equ	6		; word 3  - logical heads
ID_SPT		equ	12		; word 6  - logical sectors per track
ID_CAPS		equ	98		; word 49 - bit 9 set = LBA supported
ID_LBA28	equ	120		; words 60-61 - total addressable sectors

DRVHD_LBA	equ	0E0h		; LBA mode 	 master. CHS mode is 0A0h.

ST_BSY		equ	80h
ST_DRDY		equ	40h
ST_DRQ		equ	08h
;  Write fault (DF from ATA-4): a write the drive could not store, which it
;  may report without ERR.
ST_DF		equ	20h
ST_ERR		equ	01h

DRVHD_MASTER	equ	0A0h
DRVHD_SLAVE	equ	0B0h		; DEV bit set

;----------------------------------------------------------------------------
;  Data transports. AUTO tries each and keeps the one whose IDENTIFY data
;  validates. Safe to autodetect: a wrong transport only misreads a buffer.
;----------------------------------------------------------------------------
XT_TR_AUTO	equ	0
XT_TR_PIO8	equ	1		; 8-bit: every byte through the data port
XT_TR_LATCH	equ	2		; 16-bit: low at data, high at base+8

XT_SPIN		equ	400000		; status poll limit; no timer at init

;  Poll pacing (XT_POLL_BACKOFF). MEASURED on the real 5160, 2026-09-10, by
;  timing 1000 and 8000 reads of this very port against PIT channel 0 (both
;  runs agree to 0.4%):
;
;      one 8-bit I/O cycle        5.55 us   <- NOT the ~1 us assumed before
;      a register-only loop pass  0.22 us
;
;  So a status poll costs 5.55 us of bus that transfers nothing, and one 512
;  byte sector costs 2.84 ms - a hard ceiling of about 180 KB/s for 8-bit PIO
;  on this machine, which no software change can beat. What CAN be reduced is
;  the NUMBER of cycles, and 96% of a transfer is bus, 4% is our loop.
;
;  With the Inboard's cache enabled - also measured that day, the value
;  INBRDPC restores to port 0670h is 1Fh, bit 0 set - a register-only delay
;  runs from L1 and costs NO bus cycle at all, so pacing the polls is free.
;
;  Check BEFORE pacing, so a drive that is already ready pays nothing. The
;  fast window covers the streaming case, where DRQ is up by the time we look;
;  only a genuine wait reaches the paced loop.
XT_POLL_FAST	equ	32	; 32 x 5.55us = 178 us flat out, then back off
XT_POLL_DELAY	equ	256	; 256 x 0.22us = ~51 us, so ~10x less poll traffic
XT_SPIN_SLOW	equ	20000	; x 56.5us = ~1.1 s, near the old real timeout

;  Wait first, then look (XT_WAIT_FIRST). The card's latencies are known
;  (docs/captures/2026-10-10_xtlat/), so the driver stays off the bus for most
;  of each one and polls only near its end. Counts are register-only loop
;  passes at 0.22 us, the same unit as XT_POLL_DELAY; each is set just under
;  the shortest latency measured, so a ready card is never kept waiting.
XT_RD_FIRST_WAIT	equ	1000	; ~220 us; a read's first DRQ, 232-313 us
XT_WR_FIRST_WAIT	equ	3200	; ~700 us; a write's first DRQ, 0.8-3.3 ms
XT_COMMIT_WAIT		equ	3200	; ~700 us; the fastest commit seen, 0.8 ms
;  Paced polls before a commit is handed to the timer: 170 x ~61 us = ~10 ms.
;  A handoff can cost up to one tick (13.7 ms) of latency, so only a commit
;  that outlasts this - a flash erase, ~110 ms - is worth handing off.
XT_COMMIT_POLLS		equ	170


;  IDENTIFY DEVICE result layout (ATA-1 onwards)
ID_MODEL_OFF	equ	54		; 40-byte model string, word 27..46
ID_MODEL_LEN	equ	40
ID_MIN_TEXT	equ	4		; minimum non-blank printable chars
ID_MIN_RUN	equ	4		; and they must include one unbroken run

;============================================================================
	.DATA

;  Register stride. 1 = standard taskfile, 2 = A0 undecoded (this card).
;  0 = autodetect, and see XTIDE_DetectStride for why that is now safe.
;
;  The earlier note here said autodetection was out of the question, and the
;  reason it gave was correct: probing stride 2 by WRITING on stride-1
;  hardware puts the IDENTIFY opcode into base+0Eh, which is DEVICE CONTROL
;  there. 0ECh has bit 2 set, so that asserts SRST and leaves it asserted - a
;  hung drive, on somebody else's machine, caused by a probe.
;
;  XTIDE_DetectStride writes nothing. It reads two ports that must hold the
;  same register on a correct map and compares them, so the objection does not
;  apply to it. An explicit -DXT_STRIDE=1 or 2 still skips detection entirely.
;  Default is AUTODETECT. A shipped driver has to work on both card families,
;  not just the one it was developed against, and the probe is read-only so it
;  costs nothing to try. -DXT_STRIDE=1 or 2 pins it when a test needs one map
;  held still.
ifndef XT_STRIDE
XT_STRIDE	equ	0		; 0 = autodetect, 1 = classic XTIDE, 2 = XT-CF
endif
public XTIDE_Stride
XTIDE_Stride		dd	XT_STRIDE

;  Transport override. Zero = autodetect.
public XTIDE_Transport
XTIDE_Transport		dd	XT_TR_AUTO

;  What detection settled on. Read by XTIDE_ProgramTaskfile.
public XTIDE_ActiveTransport
XTIDE_ActiveTransport	dd	XT_TR_AUTO

XTIDE_TryTransport	dd	XT_TR_AUTO

;  Drive/head value for the unit under test. Phase 1 probes the master only,
;  but the whole path is parameterised so v2 slave support is a second call
;  rather than a rewrite.
XTIDE_TryUnit		dd	DRVHD_MASTER

;  Port addresses, computed once from base and stride by XTIDE_SetPorts.
;
;  These exist to kill a bug class, not to save arithmetic. Phase 1 failed on
;  real hardware because a helper documented as "Uses: EDX" left EDX pointing
;  at a status register and the caller then read the drive/head register
;  through it. With every port held in its own variable, a callee cannot leave
;  a stale address behind for a caller to use. Technique 78.
XTIDE_pData		dd	0
XTIDE_pError		dd	0
XTIDE_pSecCnt		dd	0
XTIDE_pSecNum		dd	0
XTIDE_pCylLo		dd	0
XTIDE_pCylHi		dd	0
XTIDE_pDrvHd		dd	0
XTIDE_pStatus		dd	0
XTIDE_pAltStatus	dd	0
XTIDE_pHiLatch		dd	0

;  Why the probe ended: the codes are set in XTIDE_Probe and the routines it
;  calls. Nothing reports it; it is there to be read with a debugger.
public XTIDE_FailCode
XTIDE_FailCode		dd	0

;  Printable non-blank characters found in the model field by the last
;  successful validation. Read with a debugger, like XTIDE_FailCode.
public XTIDE_ModelChars
XTIDE_ModelChars	dd	0

;  Status byte at the moment WaitDrq gave up, so ERR and a plain timeout can
;  be told apart afterwards.
public XTIDE_LastStatus
XTIDE_LastStatus	dd	0

;  Commit handoff (XT_YIELD_COMMIT). DeferOk lets XTIDE_WriteSectors return
;  with the drive still committing; it is set only for the last chunk of a
;  request, because a later chunk could not start until the commit ends.
;  CommitPending says it did, and XTIDE_CommitPoll finishes the job.
public XTIDE_CommitPending
XTIDE_CommitPending	dd	0
XTIDE_DeferOk		dd	0

;  Set once a unit IDENTIFY validates.
public XTIDE_MasterPresent
XTIDE_MasterPresent	dd	0
public XTIDE_SlavePresent
XTIDE_SlavePresent	dd	0

;  IDENTIFY landing area, used by the probe only.

public XTIDE_IdBuf
XTIDE_IdBuf		db	512 dup (0)

;----------------------------------------------------------------------------
;  Geometry taken from IDENTIFY, and the transfer request block.
;
;  These are module variables rather than registers on purpose. Phase 1 failed
;  on real hardware because a helper documented as "Uses: EDX" left EDX on the
;  alternate-status port and the caller then read Drive/Head through it
;  (technique 78). A read path has four more addressing values to lose that
;  way, so none of them live in a register across a call.
;----------------------------------------------------------------------------
public XTIDE_Cyls
XTIDE_Cyls			dd	0
public XTIDE_Heads
XTIDE_Heads			dd	0
public XTIDE_Spt
XTIDE_Spt			dd	0
public XTIDE_TotalSec
XTIDE_TotalSec		dd	0		; LBA28 capacity, 0 if the drive is CHS-only
public XTIDE_LbaOk
XTIDE_LbaOk			dd	0		; 1 = address in LBA, 0 = fall back to CHS

;  Request block for XTIDE_XferRun.
public XTIDE_ReqLba
XTIDE_ReqLba			dd	0
public XTIDE_ReqCount
XTIDE_ReqCount		dd	0
public XTIDE_ReqBuf
XTIDE_ReqBuf			dd	0


public XTIDE_ReqIsWrite
XTIDE_ReqIsWrite		dd	0	; direction of the request

;  The four taskfile values, filled by either addressing mode and written out
;  by one common block, so LBA and CHS cannot diverge in how they program the
;  drive - only in how they compute these.
XTIDE_oSecNum			dd	0
XTIDE_oCylLo			dd	0
XTIDE_oCylHi			dd	0
XTIDE_oDrvHd			dd	0
ifdef XT_SKIP_DRVHD
;  The DRV/HEAD value the drive last honoured. 0FFFFFFFFh = unknown: no byte
;  value matches it, so the next command selects again.
XTIDE_LastDrvHd		dd	0FFFFFFFFh
endif

;  I/O base, filled by the miniport before it calls XTIDE_Probe.
public XTIDE_BasePort
XTIDE_BasePort		dw	0

;  One sector, for the probe's read of LBA 0.
public XTIDE_SecBuf
XTIDE_SecBuf			db	512 dup (0)
public XTIDE_PartType
XTIDE_PartType		dd	0	; MBR partition 1 type byte

;  Command byte for XTIDE_ProgramTaskfile, so read and write share one
;  addressing path and differ only in what they do after the command lands.
XTIDE_Cmd			dd	0

ifndef XT_NO_WRITETEST
;  Write/read-back self-test buffers. 2 x 512 so a mismatch can be seen as a
;  mismatch rather than inferred from a checksum.
XTIDE_WrBuf			db	512 dup (0)
XTIDE_RbBuf			db	512 dup (0)
endif


;----------------------------------------------------------------------------
;  Per-unit geometry. One global set would let a slave's IDENTIFY overwrite
;  the master's numbers, and every later request would address the wrong
;  shape of disk. XTIDE_SelectUnit copies a unit's set into the working
;  globals.
;
;  Index 0 = master, 1 = slave, matching the SRB's TargetId.
;----------------------------------------------------------------------------
XTIDE_UnitPresent	dd	2 dup (0)
XTIDE_UnitCyls		dd	2 dup (0)
XTIDE_UnitHeads		dd	2 dup (0)
XTIDE_UnitSpt		dd	2 dup (0)
XTIDE_UnitTotal		dd	2 dup (0)
XTIDE_UnitLbaOk		dd	2 dup (0)

;  Sector-count register is 8 bits, and 0 there means 256. SCSIPORT can ask
;  for more than one command can carry, so requests are chunked.
XT_MAX_CHUNK	equ	128		; 64 KB per command

;  Per-unit model string, de-swapped. ATA stores its text two characters per
;  16-bit word with the pair reversed, so it has to be unswapped before it is
;  fit to hand to anything that will display it.
public XTIDE_UnitModel
XTIDE_UnitModel		db	80 dup (32)	; 2 units x 40 bytes, space filled


;============================================================================
	.CODE


;----------------------------------------------------------------------------
; XTIDE_DetectStride - settle the register stride by reading, never writing.
;
;   In:   BX = base I/O address
;   Out:  CF=0 and XTIDE_Stride set to 1 or 2; CF=1 if neither map holds up
;   Uses: EAX, ECX, EDX
;
;   Status (base+07*stride) and Alternate Status (base+14*stride) are the SAME
;   register on any correct ATA map. So read both under a candidate stride and
;   compare: they agree on the right one and, measured on the real card, do not
;   on the wrong one - 92h against 50h (technique 78 addendum). Two IN
;   instructions per candidate, no writes, so a wrong guess cannot leave a
;   drive reset or a latch loaded.
;
;   Stride 1 is tried first deliberately. On stride-1 hardware it succeeds
;   before anything reads base+1Ch, which is outside the card's decode; on
;   stride-2 hardware both of its addresses are inside the decode anyway. So
;   no candidate ever reads off the end of a card that a later candidate would
;   have matched.
;
;   All-ones and all-zeroes are rejected: an undecoded port floats to 0FFh on
;   this bus and two floating reads agree with each other, which would let a
;   card that is not there pass. IDENTIFY is still the real proof; this only
;   has to pick which map to attempt it on.
;----------------------------------------------------------------------------
XTIDE_DetectStride:

	mov	XTIDE_Stride, 1
	call	xds_try
	jnc	xds_ok
	mov	XTIDE_Stride, 2
	call	xds_try
	jnc	xds_ok
	stc
	ret
xds_ok:
	clc
	ret

;  Candidate test. XTIDE_SetPorts uses BX, which the caller still holds.
xds_try:
	call	XTIDE_SetPorts
	mov	edx, XTIDE_pStatus
	in	al, dx
	mov	ah, al
	mov	edx, XTIDE_pAltStatus
	in	al, dx
	cmp	al, ah
	jne	xds_no
	cmp	al, 0FFh
	je	xds_no
	or	al, al
	jz	xds_no
	clc
	ret
xds_no:
	stc
	ret

;  end XTIDE_DetectStride

;----------------------------------------------------------------------------
; XTIDE_SetPorts - derive every port address from the base and the stride.
;
;   In:   BX = base I/O address
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
XTIDE_SetPorts:

	movzx	eax, bx
	mov	ecx, XTIDE_Stride

	mov	edx, eax		; the latch is a fixed card address,
	add	edx, XT_HILATCH		; never strided
	mov	XTIDE_pHiLatch, edx

	mov	XTIDE_pData, eax
	add	eax, ecx
	mov	XTIDE_pError, eax
	add	eax, ecx
	mov	XTIDE_pSecCnt, eax
	add	eax, ecx
	mov	XTIDE_pSecNum, eax
	add	eax, ecx
	mov	XTIDE_pCylLo, eax
	add	eax, ecx
	mov	XTIDE_pCylHi, eax
	add	eax, ecx
	mov	XTIDE_pDrvHd, eax
	add	eax, ecx
	mov	XTIDE_pStatus, eax

	add	eax, ecx		; +8  control block base
	add	eax, ecx		; +9
	add	eax, ecx		; +10
	add	eax, ecx		; +11
	add	eax, ecx		; +12
	add	eax, ecx		; +13
	add	eax, ecx		; +14 alternate status
	mov	XTIDE_pAltStatus, eax
	ret

;  end XTIDE_SetPorts

;----------------------------------------------------------------------------
; XTIDE_WaitNotBusy - poll status until BSY clears.
;
;   Out:  CF=0 and AL = status, or CF=1 on timeout
;   Uses: EAX, ECX, EDX
;
;   ALTERNATE status, at base + 14*stride. Reading the status register proper
;   acknowledges a pending interrupt; alternate status does not, which is the
;   correct thing to poll even on a card jumpered without an interrupt.
;
;   An earlier version of this file said there was no reachable alternate
;   status, because base+8 answers as cylinder low. That was right about
;   base+8 and wrong about the conclusion: the control block is a second
;   strided bank at base+10h. Measured, 2026-08-31.
;----------------------------------------------------------------------------
XTIDE_WaitNotBusy:

	mov	edx, XTIDE_pAltStatus
ifdef XT_POLL_BACKOFF
	mov	ecx, XT_POLL_FAST
xwnb_fast:
	in	al, dx
	test	al, ST_BSY
	jz	xwnb_ok
	dec	ecx
	jnz	xwnb_fast

	mov	ecx, XT_SPIN_SLOW
xwnb_loop:
	in	al, dx
	test	al, ST_BSY
	jz	xwnb_ok
	push	ecx
	mov	ecx, XT_POLL_DELAY
xwnb_pace:
	dec	ecx			; register-only: L1, no bus cycle
	jnz	xwnb_pace
	pop	ecx
	dec	ecx
	jnz	xwnb_loop
else
	mov	ecx, XT_SPIN
xwnb_loop:
	in	al, dx
	test	al, ST_BSY
	jz	xwnb_ok
	dec	ecx
	jnz	xwnb_loop
endif
	stc
	ret
xwnb_ok:
	clc
	ret

;  end XTIDE_WaitNotBusy

;----------------------------------------------------------------------------
; XTIDE_WaitDrq - poll until BSY clears and DRQ sets, failing on ERR.
;
;   Out:  CF=0 data is ready, CF=1 on error or timeout
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
XTIDE_WaitDrq:

	mov	edx, XTIDE_pAltStatus
ifdef XT_POLL_BACKOFF
	mov	ecx, XT_POLL_FAST
xwd_fast:
	in	al, dx
	test	al, ST_BSY
	jnz	xwd_fnext		; still busy, status bits not valid yet
	test	al, ST_ERR
	jnz	xwd_fail
	test	al, ST_DRQ
	jnz	xwd_ok
xwd_fnext:
	dec	ecx
	jnz	xwd_fast

	mov	ecx, XT_SPIN_SLOW
xwd_loop:
	in	al, dx
	test	al, ST_BSY
	jnz	xwd_again
	test	al, ST_ERR
	jnz	xwd_fail
	test	al, ST_DRQ
	jnz	xwd_ok
xwd_again:
	push	ecx
	mov	ecx, XT_POLL_DELAY
xwd_pace:
	dec	ecx			; register-only: L1, no bus cycle
	jnz	xwd_pace
	pop	ecx
	dec	ecx
	jnz	xwd_loop
else
	mov	ecx, XT_SPIN
xwd_loop:
	in	al, dx
	test	al, ST_BSY
	jnz	xwd_again		; still busy, status bits not valid yet
	test	al, ST_ERR
	jnz	xwd_fail
	test	al, ST_DRQ
	jnz	xwd_ok
xwd_again:
	dec	ecx
	jnz	xwd_loop
endif
xwd_fail:
	movzx	eax, al
	mov	XTIDE_LastStatus, eax	; so ERR and timeout are distinguishable
	stc
	ret
xwd_ok:
	clc
	ret

;  end XTIDE_WaitDrq

;----------------------------------------------------------------------------
; XTIDE_Spin - spin ECX register-only loop passes (0.22 us each).
;
;   Uses: ECX
;
;   Runs from L1 with the Inboard cache on, so it holds the CPU but not the
;   bus: DMA and refresh go on while it spins.
;----------------------------------------------------------------------------
XTIDE_Spin:
	dec	ecx
	jnz	XTIDE_Spin
	ret

;  end XTIDE_Spin

;----------------------------------------------------------------------------
; XTIDE_CommitPoll - PUBLIC. One look at a write left committing by
;                    XTIDE_WriteSectors.
;
;   Out:  EAX = 0 still busy, 1 committed, 2 ended with ERR or a write fault
;   Uses: EAX, EDX
;
;   One status read and no loop: the caller is a timer callback running in
;   the timer interrupt, and comes back on the next tick if the drive is busy.
;----------------------------------------------------------------------------
public XTIDE_CommitPoll
XTIDE_CommitPoll:

	mov	edx, XTIDE_pAltStatus
	in	al, dx
	test	al, ST_BSY
	jnz	xcp_busy
	mov	XTIDE_CommitPending, 0
	test	al, ST_ERR OR ST_DF
	jnz	xcp_err
	mov	eax, 1
	ret
xcp_err:
	movzx	eax, al
	mov	XTIDE_LastStatus, eax
	mov	XTIDE_FailCode, 14	; drive reported ERR or a write fault
	mov	eax, 2
	ret
xcp_busy:
	xor	eax, eax
	ret

;  end XTIDE_CommitPoll

;----------------------------------------------------------------------------
; XTIDE_ReadData - read 512 bytes from the data register.
;
;   In:   EDI -> destination buffer, XTIDE_TryTransport selects how
;   Uses: EAX, ECX, EDX, EDI
;
;   THE LOOP MUST NOT BE INTERRUPTED BY ANYTHING THAT TOUCHES THE DRIVE.
;   Driving this data phase from the host over COMrade, one I/O per serial
;   round trip, lost DRQ after about nineteen accesses every time. The first
;   byte always came back correct (4Ah) and the rest came back zero. That is
;   not a property of the card - it is a property of leaving a PIO transfer
;   half-finished for milliseconds at a time. A tight loop here is the only
;   place this transfer can happen.
;
;   PIO8: expected correct on this card. The CF is in 8-bit mode (the XTIDE
;   BIOS puts it there) and every byte comes through the data port.
;
;   LATCH: for cards that have a high-byte latch. Reading the data port
;   returns the low byte AND latches the high byte, read from base+8.
;   Low-then-high on reads; writes are the other way round (high first, then
;   low commits the word) - phase 2 problem, and easy to get backwards.
;----------------------------------------------------------------------------
XTIDE_ReadData:

	cmp	XTIDE_TryTransport, XT_TR_LATCH
	je	xrd_latch

	mov	edx, XTIDE_pData
	mov	ecx, 512		; bytes
ifdef XT_FAST_XFER
;  One instruction per byte instead of five. The bus cycle still dominates, so
;  this is not a large win on its own - it removes the ~13 core cycles of loop
;  overhead per byte, not the transfer. insb writes through ES:EDI and this
;  driver addresses its buffers off DS, so point ES at DS for the duration
;  rather than assuming SCSIPORT left them equal.
	push	es
	mov	ax, ds
	mov	es, ax
	cld
	cmp	XTIDE_Stride, 2
	jne	xrd_bytewise
;  WORD-WIDE, stride 2 only. A0 is not decoded on this map, so base+1 IS
;  the data register - a word access lands two bus cycles on it and yields
;  two sequential bytes out of the sector FIFO, low byte first, which is
;  the order the buffer wants.
;
;  MEASURED on the real 5160, 2026-09-10, 512 bytes off this very port:
;      rep insb   3588 PIT ticks   3007 us   5.87 us/byte
;      rep insw   2334 PIT ticks   1956 us   3.82 us/byte   <- 35% faster
;  Roughly 3.9 us of the 5.77 us per-access cost is the Inboard
;  synchronising to the 4.77 MHz bus, and a word access pays that ONCE for
;  two bytes. Verified byte-correct by draining IDENTIFY both ways and
;  diffing - identical, model string and word 47 intact.
;
;  NEVER on stride 1: base+1 is the Error register there, and this would
;  corrupt every second byte. 32-bit is out for the same reason - base+2
;  and base+3 are Error, because stride 2 still decodes A1.
	shr	ecx, 1			; 256 words
	rep	insw
	pop	es
	ret
xrd_bytewise:
	rep	insb			; advances EDI by 512
	pop	es
else
xrd_pio8_loop:
	in	al, dx
	mov	[edi], al
	inc	edi
	dec	ecx
	jnz	xrd_pio8_loop
endif
	ret

xrd_latch:
	mov	ecx, 256		; words
xrd_latch_loop:
	mov	edx, XTIDE_pData
	in	al, dx			; low byte, latches the high byte
	mov	[edi], al
	mov	edx, XTIDE_pHiLatch
	in	al, dx			; high byte out of the latch
	mov	[edi+1], al
	add	edi, 2
	dec	ecx
	jnz	xrd_latch_loop
	ret

;  end XTIDE_ReadData

;----------------------------------------------------------------------------
; XTIDE_ValidateId - does this look like real IDENTIFY data?
;
;   In:   ESI -> 512-byte IDENTIFY buffer
;   Out:  CF=0 if the model string is sane, CF=1 otherwise
;
;   This is what makes the transport self-verifying. The model string is 40
;   bytes of printable ASCII at a fixed offset. Read through the wrong
;   transport it comes back mangled - byte-duplicated, in the case of a latch
;   read that never fetches the high byte. That exact bug ships in one of the
;   binaries in drivers/xtide_cdrom/, which is where the idea came from.
;----------------------------------------------------------------------------
XTIDE_ValidateId:

	push	esi
	push	ecx
	push	edx
	push	ebx
	push	edi

	add	esi, ID_MODEL_OFF
	mov	ecx, ID_MODEL_LEN
	xor	edx, edx		; printable, non-blank count
	xor	ebx, ebx		; longest unbroken run
	xor	edi, edi		; current run
xvi_loop:
	mov	al, [esi]
	cmp	al, 20h
	jb	xvi_bad
	cmp	al, 7Eh
	ja	xvi_bad
	je	xvi_next
	cmp	al, 20h
	je	xvi_blank
	inc	edx
	inc	edi			; extend the current run
	cmp	edi, ebx
	jbe	xvi_next
	mov	ebx, edi		; new longest
	jmp	xvi_next
xvi_blank:
	xor	edi, edi		; a blank breaks the run
xvi_next:
	inc	esi
	dec	ecx
	jnz	xvi_loop

;  A run, not a count. The old test was "at least 4 printable non-blank bytes
;  anywhere", and garbage clears that easily - a latch read of cylinder-low
;  scored 10 and was accepted as valid on 2026-08-31. A real model string
;  starts with an unbroken run of printable characters; noise rarely does.
	cmp	ebx, ID_MIN_RUN		; longest unbroken run
	jb	xvi_bad
	cmp	edx, ID_MIN_TEXT	; all blanks is not a model string
	jb	xvi_bad

	mov	XTIDE_ModelChars, edx

	pop	edi
	pop	ebx
	pop	edx
	pop	ecx
	pop	esi
	clc
	ret

xvi_bad:
	pop	edi
	pop	ebx
	pop	edx
	pop	ecx
	pop	esi
	stc
	ret

;  end XTIDE_ValidateId

;----------------------------------------------------------------------------
; XTIDE_TryIdentify - issue IDENTIFY DEVICE and validate the answer.
;
;   Out:  CF=0 on success, XTIDE_IdBuf filled
;   Uses: EAX, ECX, EDX, EDI, ESI
;
;   IDENTIFY does not touch media, so this is safe to issue against a drive
;   the real-mode BIOS is still driving - provided it is idle, which the
;   leading WaitNotBusy establishes. This is also what ESDI_506.PDR does on
;   every 486: probing the live boot disk during IOS init is the designed
;   sequence, not an edge case.
;
;   Measured on the real card 2026-08-31: writing 0ECh to the status/command
;   port returns 58h - DRDY|DSC|DRQ - and the first data byte is 4Ah, a real
;   IDENTIFY word-0 low byte.
;----------------------------------------------------------------------------
XTIDE_TryIdentify:

ifdef XT_SKIP_DRVHD
	mov	XTIDE_LastDrvHd, 0FFFFFFFFh	; the probe selects units of its own
endif
	call	XTIDE_WaitNotBusy	; drive idle before we touch anything?
	jc	xti_notidle

	mov	edx, XTIDE_pDrvHd	; select the unit under test
	mov	eax, XTIDE_TryUnit
	out	dx, al

	call	XTIDE_WaitNotBusy	; honour the select
	jc	xti_notidle

;  A present slave answers with its own DEV bit; an absent one leaves the bus
;  floating or mirrors the master. Reading Drive/Head back and comparing is how
;  ATASelectDevice in drivers/xtide_cdrom/ tells them apart.
;
;  Slave only. A master that does not read Drive/Head back is not thereby
;  broken, and the model string is stronger evidence than this check. Phase 1
;  has no readback channel, so a test whose only outcome is a silent failure
;  costs information rather than buying it.
	cmp	XTIDE_TryUnit, DRVHD_MASTER
	je	xti_selected

	mov	edx, XTIDE_pDrvHd
	in	al, dx
	mov	ah, byte ptr XTIDE_TryUnit
	xor	al, ah
	test	al, 10h			; DEV bit as selected?
	jnz	xti_nodev
	jmp	xti_selected
xti_nodev:
	mov	XTIDE_FailCode, 6	; slave did not answer with its DEV bit
	stc
	ret
xti_selected:

	xor	al, al			; IDENTIFY takes no parameters, but
	mov	edx, XTIDE_pSecCnt	; leaving stale ones is asking for it
	out	dx, al
	mov	edx, XTIDE_pSecNum
	out	dx, al
	mov	edx, XTIDE_pCylLo
	out	dx, al
	mov	edx, XTIDE_pCylHi
	out	dx, al

	mov	edx, XTIDE_pStatus	; status on read, command on write
	mov	al, ATA_IDENTIFY
	out	dx, al

	call	XTIDE_WaitDrq
	jc	xti_nodrq

	mov	edi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_ReadData

	mov	esi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_ValidateId
	jc	xti_badid

	mov	XTIDE_FailCode, 0
	clc
	ret

xti_notidle:
	mov	XTIDE_FailCode, 2	; BSY never cleared
	stc
	ret

xti_nodrq:
	mov	eax, XTIDE_LastStatus	; ERR set, or just no DRQ?
	test	al, ST_ERR
	jz	xti_nodrq_timeout
	mov	XTIDE_FailCode, 3	; drive rejected IDENTIFY
	stc
	ret
xti_nodrq_timeout:
	mov	XTIDE_FailCode, 4	; DRQ never asserted
	stc
	ret

xti_badid:
	mov	XTIDE_FailCode, 5	; data read, model string not sane
	stc
	ret

;  end XTIDE_TryIdentify


;----------------------------------------------------------------------------
; XTIDE_CaptureGeometry - pull addressing parameters out of the IDENTIFY data.
;
;   In:   ESI -> validated IDENTIFY buffer
;   Uses: EAX, ECX
;
;   LBA is preferred wherever the drive offers it, and CHS is kept as a real
;   fallback rather than a comment: XTIDE Universal BIOS drives plenty of
;   pre-LBA drives, and a driver that only speaks LBA is a driver for the
;   CompactFlash cards and nothing else.
;----------------------------------------------------------------------------
XTIDE_CaptureGeometry:

	movzx	eax, word ptr [esi+ID_CYLS]
	mov	XTIDE_Cyls, eax
	movzx	eax, word ptr [esi+ID_HEADS]
	mov	XTIDE_Heads, eax
	movzx	eax, word ptr [esi+ID_SPT]
	mov	XTIDE_Spt, eax
	mov	eax, [esi+ID_LBA28]
	mov	XTIDE_TotalSec, eax

	xor	ecx, ecx
	movzx	eax, word ptr [esi+ID_CAPS]
	test	ax, 0200h		; capability bit 9 - LBA supported
	jz	xcg_nolba
	cmp	XTIDE_TotalSec, 0	; a drive can claim LBA and report no
	je	xcg_nolba		; capacity; believe the capacity
	inc	ecx
xcg_nolba:
	mov	XTIDE_LbaOk, ecx
	ret

;  end XTIDE_CaptureGeometry

;----------------------------------------------------------------------------
; XTIDE_ProgramTaskfile - select the unit, program the address, issue the
;                         command in XTIDE_Cmd.
;
;   In:   XTIDE_ReqLba, XTIDE_ReqCount, XTIDE_Cmd
;   Out:  CF=0 command issued; CF=1 and XTIDE_FailCode set to 7 or 9
;   Uses: EAX, ECX, EDX
;
;   Read and write call this same routine. The addressing arithmetic is the
;   easiest thing in a disk driver to get subtly different between the two
;   paths, and the difference shows up as data written to the wrong sector -
;   silent, and only visible much later.
;
;   Polled throughout. The card here is jumpered without an interrupt, and a
;   driver that assumes otherwise is a driver for one machine.
;----------------------------------------------------------------------------
XTIDE_ProgramTaskfile:

	call	XTIDE_WaitNotBusy	; drive idle before we program anything
	jc	xrs_notready

	cmp	XTIDE_LbaOk, 0
	je	xrs_chs

;  LBA28: the taskfile carries the address a byte at a time, low first, with
;  the top four bits riding in Drive/Head alongside the LBA and master bits.
	mov	eax, XTIDE_ReqLba
	and	eax, 0FFh
	mov	XTIDE_oSecNum, eax
	mov	eax, XTIDE_ReqLba
	shr	eax, 8
	and	eax, 0FFh
	mov	XTIDE_oCylLo, eax
	mov	eax, XTIDE_ReqLba
	shr	eax, 16
	and	eax, 0FFh
	mov	XTIDE_oCylHi, eax
	mov	eax, XTIDE_ReqLba
	shr	eax, 24
	and	eax, 0Fh
	or	eax, DRVHD_LBA
	mov	ecx, XTIDE_TryUnit	; DRVHD_LBA has DEV clear, so without this
	and	ecx, 010h		; every transfer addressed the master however
	or	eax, ecx			; the unit was selected. Caught by checking the
	mov	XTIDE_oDrvHd, eax	; DISK, not the round trip - both halves of a
				; round trip go to the same wrong drive.
	jmp	xrs_program

;  CHS: cyl = lba / (heads*spt), then head = rem / spt and sector = rem MOD spt
;  + 1. Sector numbers are one-based; heads and cylinders are not.
xrs_chs:
	mov	eax, XTIDE_Heads
	imul	eax, XTIDE_Spt
	or	eax, eax
	jz	xrs_geom		; no LBA and no usable geometry either
	mov	ecx, eax
	mov	eax, XTIDE_ReqLba
	xor	edx, edx
	div	ecx			; eax = cylinder, edx = remainder
	mov	XTIDE_oCylHi, eax
	and	eax, 0FFh
	mov	XTIDE_oCylLo, eax
	mov	eax, XTIDE_oCylHi
	shr	eax, 8
	and	eax, 0FFh
	mov	XTIDE_oCylHi, eax

	mov	eax, edx
	xor	edx, edx
	div	XTIDE_Spt		; eax = head, edx = sector - 1
	and	eax, 0Fh
	or	eax, XTIDE_TryUnit	; carries the CHS base AND the DEV bit
	mov	XTIDE_oDrvHd, eax
	inc	edx
	mov	XTIDE_oSecNum, edx

xrs_program:
ifdef XT_SKIP_DRVHD
;  In LBA mode DRV/HEAD holds only the unit and LBA bits 24-27, and nothing
;  writes it between our commands, so an unchanged value needs neither the
;  write nor the wait for the drive to honour it. The other taskfile registers
;  are always written: the drive changes them as a command runs.
;
;  Never skip in CHS: the head is in DRV/HEAD, and a drive that reports the
;  last sector transferred advances it when a command crosses a track.
	cmp	XTIDE_LbaOk, 0
	je	xrs_select
	mov	eax, XTIDE_oDrvHd
	cmp	eax, XTIDE_LastDrvHd
	je	xrs_selected
xrs_select:
	mov	XTIDE_LastDrvHd, 0FFFFFFFFh	; unknown until the select is honoured
endif
	mov	edx, XTIDE_pDrvHd
	mov	eax, XTIDE_oDrvHd
	out	dx, al
	call	XTIDE_WaitNotBusy	; let the drive honour the select
	jc	xrs_notready
ifdef XT_SKIP_DRVHD
	mov	eax, XTIDE_oDrvHd
	mov	XTIDE_LastDrvHd, eax
xrs_selected:
;  A command that ends in another LBA 24-27 group can leave the drive's
;  register holding that group, so the value is unknown once it has run.
	mov	eax, XTIDE_ReqLba
	add	eax, XTIDE_ReqCount
	dec	eax
	xor	eax, XTIDE_ReqLba
	shr	eax, 24
	jz	xrs_samegroup
	mov	XTIDE_LastDrvHd, 0FFFFFFFFh
xrs_samegroup:
endif

ifdef XT_TIGHT_TASKFILE
;  Every OUT empties the L1, so a value or port read from memory after one
;  costs a line fill. Load all five values and ports first; between the OUTs
;  only register moves remain. The same taskfile writes, in the same order, as
;  the plain path below.
	push	ebx
	push	esi
	push	edi
	push	ebp
	movzx	ebx, BYTE PTR XTIDE_oCylHi
	shl	ebx, 8
	mov	bl, BYTE PTR XTIDE_oCylLo
	shl	ebx, 8
	mov	bl, BYTE PTR XTIDE_oSecNum
	shl	ebx, 8
	mov	bl, BYTE PTR XTIDE_ReqCount	; BL SecCnt, BH SecNum, then CylLo, CylHi
	mov	esi, XTIDE_pSecNum
	shl	esi, 16
	or	esi, XTIDE_pSecCnt
	mov	edi, XTIDE_pCylHi
	shl	edi, 16
	or	edi, XTIDE_pCylLo
	mov	ebp, XTIDE_pStatus	; status on read, command on write
	mov	ecx, XTIDE_Cmd

	mov	edx, esi
	mov	al, bl
	out	dx, al			; sector count
	shr	esi, 16
	mov	edx, esi
	mov	al, bh
	out	dx, al			; sector number / LBA 0-7
	shr	ebx, 16
	mov	edx, edi
	mov	al, bl
	out	dx, al			; cylinder low / LBA 8-15
	shr	edi, 16
	mov	edx, edi
	mov	al, bh
	out	dx, al			; cylinder high / LBA 16-23
	mov	edx, ebp
	mov	eax, ecx
	out	dx, al			; command

	pop	ebp
	pop	edi
	pop	esi
	pop	ebx
else
	mov	edx, XTIDE_pSecCnt
	mov	eax, XTIDE_ReqCount
	out	dx, al
	mov	edx, XTIDE_pSecNum
	mov	eax, XTIDE_oSecNum
	out	dx, al
	mov	edx, XTIDE_pCylLo
	mov	eax, XTIDE_oCylLo
	out	dx, al
	mov	edx, XTIDE_pCylHi
	mov	eax, XTIDE_oCylHi
	out	dx, al

	mov	edx, XTIDE_pStatus	; status on read, command on write
	mov	eax, XTIDE_Cmd
	out	dx, al
endif

;  Use the transport autodetect settled on, not whatever the last probe
;  attempt happened to leave in XTIDE_TryTransport.
	mov	eax, XTIDE_ActiveTransport
	mov	XTIDE_TryTransport, eax
	clc
	ret

xrs_notready:
	mov	XTIDE_FailCode, 7	; BSY never cleared around the transfer
	stc
	ret

xrs_geom:
	mov	XTIDE_FailCode, 9	; no LBA and heads*spt is zero
	stc
	ret

;  end XTIDE_ProgramTaskfile

;----------------------------------------------------------------------------
; XTIDE_ReadSectors - read XTIDE_ReqCount sectors from XTIDE_ReqLba into
;                     XTIDE_ReqBuf.
;
;   Out:  CF=0 on success; CF=1 with XTIDE_FailCode 7, 8 or 9
;   Uses: EAX, ECX, EDX, EDI
;----------------------------------------------------------------------------
XTIDE_ReadSectors:

	mov	XTIDE_Cmd, ATA_READ
	call	XTIDE_ProgramTaskfile
	jc	xrd_fail
ifdef XT_WAIT_FIRST
	mov	ecx, XT_RD_FIRST_WAIT	; later sectors are ready at once: no wait
	call	XTIDE_Spin
endif

	mov	ecx, XTIDE_ReqCount
	mov	edi, XTIDE_ReqBuf
xrd_sector:
	push	ecx
	call	XTIDE_WaitDrq		; one DRQ per sector, not one per command
	jc	xrd_nodrq
	call	XTIDE_ReadData		; advances EDI by 512
	pop	ecx
	dec	ecx
	jnz	xrd_sector
	clc
	ret

xrd_nodrq:
	pop	ecx
	mov	XTIDE_FailCode, 8	; read gave no DRQ, or set ERR
xrd_fail:
	stc
	ret

;  end XTIDE_ReadSectors

;----------------------------------------------------------------------------
; XTIDE_WriteSectors - write XTIDE_ReqCount sectors from XTIDE_ReqBuf to
;                      XTIDE_ReqLba.
;
;   Out:  CF=0 on success; CF=1 with XTIDE_FailCode 7, 9, 12 or 13
;   Uses: EAX, ECX, EDX, ESI
;
;   The drive asserts DRQ to ASK for each sector, so the wait is the same shape
;   as a read - but the completion check is not. A write is not finished when
;   the last byte has gone out; the drive is still committing it. Poll BSY down
;   and test ERR afterwards, or a failed write looks exactly like a good one.
;----------------------------------------------------------------------------
XTIDE_WriteSectors:

	mov	XTIDE_Cmd, ATA_WRITE
	call	XTIDE_ProgramTaskfile
	jc	xwr_fail
ifdef XT_WAIT_FIRST
	mov	ecx, XT_WR_FIRST_WAIT
	call	XTIDE_Spin
endif

	mov	ecx, XTIDE_ReqCount
	mov	esi, XTIDE_ReqBuf
xwr_sector:
	push	ecx
	call	XTIDE_WaitDrq		; drive asking for the next sector
	jc	xwr_nodrq
	call	XTIDE_WriteData		; advances ESI by 512
	pop	ecx
	dec	ecx
	jnz	xwr_sector

ifdef XT_YIELD_COMMIT
;  Most commits finish in milliseconds; a flash erase takes ~110 ms. Poll
;  the short ones out, paced so the bus stays nearly free, and hand only a
;  commit still running after XT_COMMIT_POLLS to the caller's timer.
	cmp	XTIDE_DeferOk, 0
	je	xwr_sync
	mov	ecx, XT_COMMIT_WAIT
	call	XTIDE_Spin
	mov	edx, XTIDE_pAltStatus
	mov	ecx, XT_COMMIT_POLLS
xwr_cpoll:
	in	al, dx
	test	al, ST_BSY
	jz	xwr_check
	push	ecx
	mov	ecx, XT_POLL_DELAY
	call	XTIDE_Spin
	pop	ecx
	dec	ecx
	jnz	xwr_cpoll
	mov	XTIDE_CommitPending, 1
	clc
	ret
xwr_sync:
endif
	call	XTIDE_WaitNotBusy	; the drive is still committing the last sector
	jc	xwr_notready
xwr_check:
	test	al, ST_ERR OR ST_DF
	jnz	xwr_err
	clc
	ret

xwr_nodrq:
	pop	ecx
	mov	XTIDE_FailCode, 13	; drive never asked for the data
	stc
	ret

xwr_notready:
	mov	XTIDE_FailCode, 12	; BSY never cleared after the last sector
	stc
	ret

xwr_err:
	movzx	eax, al
	mov	XTIDE_LastStatus, eax
	mov	XTIDE_FailCode, 14	; drive reported ERR or a write fault
	stc
	ret

xwr_fail:
	stc
	ret

;  end XTIDE_WriteSectors

;----------------------------------------------------------------------------
; XTIDE_WriteData - push 512 bytes at the data register.
;
;   In:   ESI -> source buffer, XTIDE_TryTransport selects how
;   Uses: EAX, ECX, EDX, ESI
;
;   LATCH ORDER IS THE OPPOSITE OF A READ. On a read the data port is read
;   first and that latches the high byte; on a write the high byte is staged in
;   the latch first and the data-port write commits the word. Get it backwards
;   and every byte pair is stored swapped.
;
;   A write/read-back self-test DOES catch that, because the read path is
;   separate code that is not making the same mistake - which is exactly why
;   the test pattern below gives each word two different bytes.
;   Confirmed against OBattler's XT-IDE port in drivers/xtide_cdrom/, whose
;   register map and write ordering are sound even though its transfer loops
;   are not.
;----------------------------------------------------------------------------
XTIDE_WriteData:

	cmp	XTIDE_TryTransport, XT_TR_LATCH
	je	xwd8_latch

	mov	edx, XTIDE_pData
	mov	ecx, 512			; bytes
ifdef XT_FAST_XFER
	cld
;  WORD-WIDE, stride 2 only - the write twin of the read path above.
;  VERIFIED on the real 5160, 2026-09-10, without risking a single sector:
;  ATA WRITE BUFFER (0E8h) / READ BUFFER (0E4h) move 512 bytes in and out of
;  the drive's INTERNAL buffer and never touch the media. Wrote an
;  incrementing pattern with rep outsw, read it back with the known-good rep
;  insb, and compared - 00 01 02 ... 1F exactly, no ERR in any status byte.
;  Different code on each leg, so a width or byte-order fault could not
;  cancel the way a same-path round trip would (technique 79).
;
;  Timed the same way: rep outsb 3482 ticks / 5.70 us per byte, rep outsw
;  2328 ticks / 3.81 us per byte - 33% faster, and within 0.3% of the read
;  side, so the write data phase costs exactly what the read one does.
	cmp	XTIDE_Stride, 2
	jne	xwd8_bytewise
	shr	ecx, 1			; 256 words
	rep	outsw
	ret
xwd8_bytewise:
	rep	outsb			; DS:ESI, advances ESI by 512
else
xwd8_pio8_loop:
	mov	al, [esi]
	out	dx, al
	inc	esi
	dec	ecx
	jnz	xwd8_pio8_loop
endif
	ret

xwd8_latch:
	mov	ecx, 256			; words
xwd8_latch_loop:
	mov	al, [esi+1]		; high byte STAGES in the latch
	mov	edx, XTIDE_pHiLatch
	out	dx, al
	mov	al, [esi]		; low byte COMMITS the word
	mov	edx, XTIDE_pData
	out	dx, al
	add	esi, 2
	dec	ecx
	jnz	xwd8_latch_loop
	ret

;  end XTIDE_WriteData


ifndef XT_NO_WRITETEST
;----------------------------------------------------------------------------
; XTIDE_WriteReadBack - write one sector of a known pattern and read it back.
;
;   Out:  CF=0 if the sector came back identical
;         CF=1 with XTIDE_FailCode 7, 9, 12, 13, 14 (transfer) or 15 (mismatch)
;   Uses: EAX, ECX, EDX, ESI, EDI
;
;   The pattern gives every 16-bit word two DIFFERENT bytes - n and its
;   complement - so a high/low swap in the latch path shows up as a mismatch
;   rather than passing. All-zeroes or a constant fill would prove nothing:
;   blank media returns those too.
;
;   LBA 100 is arbitrary but deliberately clear of sector 0 and of anything a
;   partition table or boot sector lives in, so a run against the wrong disk
;   damages nothing structural.
;----------------------------------------------------------------------------
XTIDE_TEST_LBA	equ	100

XTIDE_WriteReadBack:

	mov	edi, OFFSET32 XTIDE_WrBuf
	mov	ecx, 256			; words
	xor	eax, eax
xwrb_fill:
	mov	[edi], al			; low byte counts up
	not	al
	mov	[edi+1], al		; high byte is its complement
	not	al
	inc	al
	add	edi, 2
	dec	ecx
	jnz	xwrb_fill

	mov	XTIDE_ReqLba, XTIDE_TEST_LBA
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_WrBuf
	call	XTIDE_WriteSectors
	jc	xwrb_out			; code already set by the write path

	mov	XTIDE_ReqLba, XTIDE_TEST_LBA
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_RbBuf
	call	XTIDE_ReadSectors
	jc	xwrb_out

	mov	esi, OFFSET32 XTIDE_WrBuf
	mov	edi, OFFSET32 XTIDE_RbBuf
	mov	ecx, 512
xwrb_cmp:
	mov	al, [esi]
	cmp	al, [edi]
	jne	xwrb_mismatch
	inc	esi
	inc	edi
	dec	ecx
	jnz	xwrb_cmp
	clc
	ret

xwrb_mismatch:
	mov	XTIDE_FailCode, 15	; round trip returned different bytes
xwrb_out:
	stc
	ret

;  end XTIDE_WriteReadBack
endif


;----------------------------------------------------------------------------
; XTIDE_CopyId - de-swap this unit's 40-byte model string out of the IDENTIFY
;                buffer into XTIDE_UnitModel.
;
;   In:   EAX = unit index, ESI -> IDENTIFY buffer
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
XTIDE_CopyId:

	push	esi
	push	edi
	mov	edx, eax
	imul	edx, edx, 40
	add	edx, OFFSET32 XTIDE_UnitModel
	mov	edi, edx
	add	esi, ID_MODEL_OFF
	mov	ecx, 20			; words
xci_loop:
	mov	al, [esi+1]		; ATA word order is reversed
	mov	[edi], al
	mov	al, [esi]
	mov	[edi+1], al
	add	esi, 2
	add	edi, 2
	dec	ecx
	jnz	xci_loop
	pop	edi
	pop	esi
	ret

;  end XTIDE_CopyId

;----------------------------------------------------------------------------
; XTIDE_StashGeometry / XTIDE_SelectUnit - move geometry between the working
; globals and the per-unit arrays.
;
;   In:   EAX = unit index (0 master, 1 slave)
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
XTIDE_StashGeometry:

	mov	ecx, eax
	shl	ecx, 2			; dword index
	mov	edx, XTIDE_Cyls
	mov	XTIDE_UnitCyls[ecx], edx
	mov	edx, XTIDE_Heads
	mov	XTIDE_UnitHeads[ecx], edx
	mov	edx, XTIDE_Spt
	mov	XTIDE_UnitSpt[ecx], edx
	mov	edx, XTIDE_TotalSec
	mov	XTIDE_UnitTotal[ecx], edx
	mov	edx, XTIDE_LbaOk
	mov	XTIDE_UnitLbaOk[ecx], edx
	mov	XTIDE_UnitPresent[ecx], 1
	call	XTIDE_CopyId		 ; EAX still the unit, ESI still the ID buffer
	ret

;  end XTIDE_StashGeometry

public XTIDE_SelectUnit
XTIDE_SelectUnit:

	mov	ecx, eax
	shl	ecx, 2
	mov	edx, XTIDE_UnitCyls[ecx]
	mov	XTIDE_Cyls, edx
	mov	edx, XTIDE_UnitHeads[ecx]
	mov	XTIDE_Heads, edx
	mov	edx, XTIDE_UnitSpt[ecx]
	mov	XTIDE_Spt, edx
	mov	edx, XTIDE_UnitTotal[ecx]
	mov	XTIDE_TotalSec, edx
	mov	edx, XTIDE_UnitLbaOk[ecx]
	mov	XTIDE_LbaOk, edx

;  Drive/Head base for this unit. Getting this wrong is how phase 2b wrote to
;  the boot disk while reporting success (technique 79).
	mov	edx, DRVHD_MASTER
	or	eax, eax
	jz	xsu_master
	mov	edx, DRVHD_SLAVE
xsu_master:
	mov	XTIDE_TryUnit, edx
	ret

;  end XTIDE_SelectUnit

;----------------------------------------------------------------------------
; XTIDE_XferRun - move XTIDE_ReqCount sectors between XTIDE_ReqLba and
;                 XTIDE_ReqBuf, in chunks one taskfile command can carry.
;
;   In:   XTIDE_ReqLba / ReqCount / ReqBuf / ReqIsWrite
;   Out:  CF=0 on success. ReqCount is zero; ReqLba and ReqBuf are advanced
;         past every chunk except the last.
;   Uses: EAX, ECX, EDX, ESI, EDI  (EBX preserved: XtStartIo holds the SRB
;         in it across this call)
;----------------------------------------------------------------------------
public XTIDE_XferRun
XTIDE_XferRun:

	push	ebx
ifdef XT_YIELD_COMMIT
	mov	XTIDE_CommitPending, 0
endif
xfr_chunk:
	mov	ecx, XTIDE_ReqCount
	or	ecx, ecx
	jz	xfr_ok
	cmp	ecx, XT_MAX_CHUNK
	jbe	xfr_have
	mov	ecx, XT_MAX_CHUNK
xfr_have:
	push	ecx			; this chunk's sector count
	mov	eax, XTIDE_ReqCount
	sub	eax, ecx
	push	eax			; sectors still to do after it
	mov	XTIDE_ReqCount, ecx

	cmp	XTIDE_ReqIsWrite, 0
	jne	xfr_write
	call	XTIDE_ReadSectors
	jmp	xfr_done_chunk
xfr_write:
ifdef XT_YIELD_COMMIT
	or	eax, eax		; last chunk? only then may the commit be left running
	setz	al
	movzx	eax, al
	mov	XTIDE_DeferOk, eax
endif
	call	XTIDE_WriteSectors
ifdef XT_YIELD_COMMIT
	mov	XTIDE_DeferOk, 0	; leaves EFLAGS alone: CF is still the result
endif
xfr_done_chunk:
	pop	eax			; remaining
	pop	ecx			; this chunk
	jc	xfr_fail

	mov	XTIDE_ReqCount, eax
	or	eax, eax
	jz	xfr_ok

;  Advance by the chunk actually transferred: LBA by sectors, buffer by bytes.
	add	XTIDE_ReqLba, ecx
	shl	ecx, 9
	add	XTIDE_ReqBuf, ecx
	jmp	xfr_chunk

xfr_ok:
	pop	ebx
	clc
	ret
xfr_fail:
ifdef XT_SKIP_DRVHD
	mov	XTIDE_LastDrvHd, 0FFFFFFFFh	; after a failure, select again
endif
	pop	ebx
	stc
	ret

;  end XTIDE_XferRun

;----------------------------------------------------------------------------
; XTIDE_Probe - PUBLIC. Called from XtFindAdapter with XTIDE_BasePort set.
;
;   Out:  CF=0 if the master answered IDENTIFY, its data validated and LBA 0
;         could be read; CF=1 otherwise, and the miniport reports not-found
;   Uses: EAX, ECX, EDX  (EBX, ESI, EDI preserved)
;----------------------------------------------------------------------------
public XTIDE_Probe
XTIDE_Probe:

	push	ebx
	push	esi
	push	edi


	movzx	ebx, word ptr XTIDE_BasePort
	or	bx, bx
	jz	xp_nobase

	cmp	XTIDE_Stride, 0		; 0 = -DXT_STRIDE=0, autodetect
	jne	xp_ports
	call	XTIDE_DetectStride
	jc	xp_nostride
xp_ports:
	call	XTIDE_SetPorts

	mov	XTIDE_TryUnit, DRVHD_MASTER

	mov	eax, XTIDE_Transport
	or	eax, eax
	jz	xp_auto

	mov	XTIDE_TryTransport, eax	; explicit override, no detection
	call	XTIDE_TryIdentify
	jc	xp_exit
	jmp	xp_ok

xp_auto:
	mov	XTIDE_TryTransport, XT_TR_PIO8	; measured first on this card
	call	XTIDE_TryIdentify
	jnc	xp_ok

	mov	XTIDE_TryTransport, XT_TR_LATCH
	call	XTIDE_TryIdentify
	jc	xp_exit

xp_ok:
	mov	eax, XTIDE_TryTransport
	mov	XTIDE_ActiveTransport, eax
	mov	XTIDE_MasterPresent, 1

	mov	esi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_CaptureGeometry
	xor	eax, eax
	call	XTIDE_StashGeometry	; unit 0 = master

;  Prove the read path on the safest target there is: LBA 0, one sector,
;  read-only, on a drive this routine has just established is idle.
	mov	XTIDE_ReqLba, 0
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_SecBuf
	call	XTIDE_ReadSectors
	jc	xp_noread		; 7, 8 or 9 already in XTIDE_FailCode

;  A sector that came back with an MBR signature is a sector that came back.
	cmp	word ptr XTIDE_SecBuf+510, 0AA55h
	jne	xp_nosig

;  Keep the first partition's type: it goes into the fail code below.
	movzx	eax, byte ptr XTIDE_SecBuf+01BEh+4
	mov	XTIDE_PartType, eax

;  The write path is tested on a slave if one is fitted, never on the master:
;  the master is the boot disk.
;
;  Only a -WriteTest build looks for a slave. The test writes 512 bytes to
;  LBA 100 of whichever unit answers as one, and on a machine with one disk
;  the DEV-bit check in XTIDE_TryIdentify is all that keeps that write off
;  the boot volume.
ifndef XT_NO_WRITETEST
	mov	XTIDE_TryUnit, DRVHD_SLAVE
	call	XTIDE_TryIdentify
	jc	xp_noslave

	mov	XTIDE_SlavePresent, 1
	mov	esi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_CaptureGeometry
	mov	eax, 1
	call	XTIDE_StashGeometry	; unit 1 = slave
	call	XTIDE_WriteReadBack
	jc	xp_exit			; 7, 9, 12, 13, 14 or 15 already set

	mov	eax, XTIDE_PartType	; read AND write verified
	and	eax, 0Fh
	add	eax, 40
	mov	XTIDE_FailCode, eax
	jmp	xp_exit
endif

;  No slave is not a failure. The code records that the read verified and
;  there was nothing safe to write to.
xp_noslave:
	mov	XTIDE_TryUnit, DRVHD_MASTER
	mov	eax, XTIDE_PartType
	and	eax, 0Fh
	add	eax, 24
	mov	XTIDE_FailCode, eax
	jmp	xp_exit

xp_nosig:
	mov	XTIDE_FailCode, 10	; 512 bytes read, no 55AA at the end of them
	jmp	xp_exit

;  A drive that identifies but cannot return sector 0 is left to the real-mode
;  BIOS: claiming it would move the volume onto a read path that just failed.
xp_noread:
	mov	XTIDE_MasterPresent, 0
	jmp	xp_exit

xp_nostride:
	mov	XTIDE_FailCode, 11	; neither stride made Status and AltStatus agree
	jmp	xp_exit

xp_nobase:
	mov	XTIDE_FailCode, 1	; the miniport passed no I/O base

xp_exit:
ifdef XT_SKIP_DRVHD
;  The real-mode BIOS drives this disk again until Windows sends the first
;  request, and it writes DRV/HEAD with values of its own.
	mov	XTIDE_LastDrvHd, 0FFFFFFFFh
endif
	pop	edi
	pop	esi
	pop	ebx
	cmp	XTIDE_MasterPresent, 0
	je	xp_none
	clc
	ret
xp_none:
	stc
	ret

;  end XTIDE_Probe


	end
