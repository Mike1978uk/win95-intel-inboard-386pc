;============================================================================
; XTIDETR.ASM - XT-IDE transport for a Windows 95 IOS port driver
;
;   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #21).
;   Written for the IBM 5160 + Intel Inboard 386/PC, but deliberately not
;   specific to it: the register stride and the data transport are both
;   selectable, because XTIDE Universal BIOS drives a family of electrically
;   different adapters and a driver that hardcodes one of them is a driver for
;   one machine.
;
;   PHASE 1: IDENTIFY only. No writes, no sector reads yet. The point is to
;   prove we can address the card and get a coherent answer back.
;
;   Register map cross-checked against 86Box's src/disk/hdc_xtide.c and against
;   the XT-IDE port of sava/lpproj's patacd.asm by Miran Grca (@OBattler).
;   NOTE those two are not independent - same author - and OBattler's XT
;   transfer loops are broken (they never issue the second IN), so use them for
;   the register map and the write ordering only. See drivers/xtide_cdrom/.
;============================================================================

	page	,132
	title	xtidetr - XT-IDE transport
	name	port.pdr

.386p

	.xlist
	include vmm.inc
	include	ddb.inc
	include	portddb.inc
	.list

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

;  IDENTIFY DEVICE result layout (ATA-1 onwards)
ID_MODEL_OFF	equ	54		; 40-byte model string, word 27..46
ID_MODEL_LEN	equ	40
ID_MIN_TEXT	equ	4		; minimum non-blank printable chars
ID_MIN_RUN	equ	4		; and they must include one unbroken run

;============================================================================
VxD_LOCKED_DATA_SEG

;  Register stride. 1 = standard taskfile, 2 = A0 undecoded (this card).
;
;  DELIBERATELY NOT AUTODETECTED, and this is a safety decision rather than a
;  shortcut. Probing stride 2 on stride-1 hardware writes the IDENTIFY opcode
;  to base+0Eh, which on a stride-1 XTIDE is the DEVICE CONTROL register.
;  0ECh has bit 2 set, so that asserts SRST and leaves it asserted - a hung
;  drive, on somebody else machine, from a probe. Probing the other way round
;  is harmless. Set it, do not guess it.
ifndef XT_STRIDE
XT_STRIDE	equ	2		; -DXT_STRIDE=1 for a classic XTIDE (86Box's model)
endif
public XTIDE_Stride
XTIDE_Stride		dd	XT_STRIDE

;  Transport override. Zero = autodetect.
public XTIDE_Transport
XTIDE_Transport		dd	XT_TR_AUTO

;  What detection settled on. Read by phase 2 transfer path.
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

;  Why the probe ended, reported through the boot log as a deliberate delay.
;  0 = success, and see XTIDE_Probe for the rest. There is no readback channel
;  at IOS init time, but BOOTLOG.TXT timestamps every driver's init, so the
;  time spent in here IS a channel - roughly 33 ms per tick on this machine.
public XTIDE_FailCode
XTIDE_FailCode		dd	0

;  Printable non-blank characters found in the model field by the last
;  successful validation. Added to the success delay so the boot log reports
;  not merely "a model string" but HOW LONG it was - and the length is specific
;  to the drive. TRANSCEND is 9.
public XTIDE_ModelChars
XTIDE_ModelChars	dd	0

;  Status byte at the moment WaitDrq gave up, so ERR and a plain timeout can
;  be told apart afterwards.
public XTIDE_LastStatus
XTIDE_LastStatus	dd	0

;  Set once a unit IDENTIFY validates. Phase 2 turns this into an array.
public XTIDE_MasterPresent
XTIDE_MasterPresent	dd	0
public XTIDE_SlavePresent
XTIDE_SlavePresent	dd	0

;  IDENTIFY landing area. 512 bytes, one device - phase 2 grows this.
public XTIDE_IdBuf
XTIDE_IdBuf		db	512 dup (0)

;----------------------------------------------------------------------------
;  Phase 2 - geometry taken from IDENTIFY, and the sector-read request block.
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

;  Request block for XTIDE_ReadSectors.
public XTIDE_ReqLba
XTIDE_ReqLba			dd	0
public XTIDE_ReqCount
XTIDE_ReqCount		dd	0
public XTIDE_ReqBuf
XTIDE_ReqBuf			dd	0

;  The four taskfile values, filled by either addressing mode and written out
;  by one common block, so LBA and CHS cannot diverge in how they program the
;  drive - only in how they compute these.
XTIDE_oSecNum			dd	0
XTIDE_oCylLo			dd	0
XTIDE_oCylHi			dd	0
XTIDE_oDrvHd			dd	0

;  One sector, for the phase 2a self-test read of LBA 0.
public XTIDE_SecBuf
XTIDE_SecBuf			db	512 dup (0)
public XTIDE_PartType
XTIDE_PartType		dd	0		; MBR partition 1 type byte, read back through the log

;  Command byte for XTIDE_ProgramTaskfile, so read and write share one
;  addressing path and differ only in what they do after the command lands.
XTIDE_Cmd			dd	0

;  Write/read-back self-test buffers. 2 x 512 so a mismatch can be seen as a
;  mismatch rather than inferred from a checksum.
public XTIDE_WrBuf
XTIDE_WrBuf			db	512 dup (0)
public XTIDE_RbBuf
XTIDE_RbBuf			db	512 dup (0)

VxD_LOCKED_DATA_ENDS

;============================================================================
VxD_LOCKED_CODE_SEG

;----------------------------------------------------------------------------
; XTIDE_SetPorts - derive every port address from the base and the stride.
;
;   In:   BX = base I/O address
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_SetPorts

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

EndProc XTIDE_SetPorts

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
BeginProc XTIDE_WaitNotBusy

	mov	edx, XTIDE_pAltStatus
	mov	ecx, XT_SPIN
xwnb_loop:
	in	al, dx
	test	al, ST_BSY
	jz	xwnb_ok
	dec	ecx
	jnz	xwnb_loop
	stc
	ret
xwnb_ok:
	clc
	ret

EndProc XTIDE_WaitNotBusy

;----------------------------------------------------------------------------
; XTIDE_WaitDrq - poll until BSY clears and DRQ sets, failing on ERR.
;
;   Out:  CF=0 data is ready, CF=1 on error or timeout
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_WaitDrq

	mov	edx, XTIDE_pAltStatus
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
xwd_fail:
	movzx	eax, al
	mov	XTIDE_LastStatus, eax	; so ERR and timeout are distinguishable
	stc
	ret
xwd_ok:
	clc
	ret

EndProc XTIDE_WaitDrq

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
BeginProc XTIDE_ReadData

	cmp	XTIDE_TryTransport, XT_TR_LATCH
	je	xrd_latch

	mov	edx, XTIDE_pData
	mov	ecx, 512		; bytes
xrd_pio8_loop:
	in	al, dx
	mov	[edi], al
	inc	edi
	dec	ecx
	jnz	xrd_pio8_loop
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

EndProc XTIDE_ReadData

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
BeginProc XTIDE_ValidateId

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

	mov	XTIDE_ModelChars, edx	; report the length through the delay

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

EndProc XTIDE_ValidateId

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
BeginProc XTIDE_TryIdentify

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

EndProc XTIDE_TryIdentify


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
BeginProc XTIDE_CaptureGeometry

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

EndProc XTIDE_CaptureGeometry

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
BeginProc XTIDE_ProgramTaskfile

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
	mov	edx, XTIDE_pDrvHd
	mov	eax, XTIDE_oDrvHd
	out	dx, al
	call	XTIDE_WaitNotBusy	; let the drive honour the select
	jc	xrs_notready

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

EndProc XTIDE_ProgramTaskfile

;----------------------------------------------------------------------------
; XTIDE_ReadSectors - read XTIDE_ReqCount sectors from XTIDE_ReqLba into
;                     XTIDE_ReqBuf.
;
;   Out:  CF=0 on success; CF=1 with XTIDE_FailCode 7, 8 or 9
;   Uses: EAX, ECX, EDX, EDI
;----------------------------------------------------------------------------
BeginProc XTIDE_ReadSectors

	mov	XTIDE_Cmd, ATA_READ
	call	XTIDE_ProgramTaskfile
	jc	xrd_fail

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

EndProc XTIDE_ReadSectors

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
BeginProc XTIDE_WriteSectors

	mov	XTIDE_Cmd, ATA_WRITE
	call	XTIDE_ProgramTaskfile
	jc	xwr_fail

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

	call	XTIDE_WaitNotBusy	; the drive is still committing the last sector
	jc	xwr_notready
	test	al, ST_ERR
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
	mov	XTIDE_FailCode, 14	; drive reported ERR on completion
	stc
	ret

xwr_fail:
	stc
	ret

EndProc XTIDE_WriteSectors

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
BeginProc XTIDE_WriteData

	cmp	XTIDE_TryTransport, XT_TR_LATCH
	je	xwd8_latch

	mov	edx, XTIDE_pData
	mov	ecx, 512			; bytes
xwd8_pio8_loop:
	mov	al, [esi]
	out	dx, al
	inc	esi
	dec	ecx
	jnz	xwd8_pio8_loop
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

EndProc XTIDE_WriteData


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

BeginProc XTIDE_WriteReadBack

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

EndProc XTIDE_WriteReadBack

;----------------------------------------------------------------------------
; XTIDE_Probe - PUBLIC. Called from Port_initialize once the device node
;               resources have been parsed into the DDB.
;
;   In:   EDI -> PortDDB (DDB_base_ioa already filled by Port_Scan_Inp_Params)
;   Out:  CF=0 if a drive answered and its IDENTIFY data validated
;   Uses: EAX, ECX, EDX  (EBX, ESI, EDI preserved - Port_initialize still
;         needs EBX as the AEP pointer and EDI as the DDB)
;----------------------------------------------------------------------------
;----------------------------------------------------------------------------
; XTIDE_Delay - burn roughly EAX seconds.
;
;   The only channel out of IOS initialisation is how long we take, because
;   BOOTLOG.TXT timestamps "Initing" and "Init Success" for every driver.
;   Precision does not matter; separation does. Calibrate from the observed
;   tick delta rather than trusting this constant.
;
;   Uses: EAX, ECX
;----------------------------------------------------------------------------
BeginProc XTIDE_Delay

	or	eax, eax
	jz	xdl_done
xdl_outer:
	mov	ecx, 725000
xdl_inner:
	dec	ecx
	jnz	xdl_inner
	dec	eax
	jnz	xdl_outer
xdl_done:
	ret

EndProc XTIDE_Delay

BeginProc XTIDE_Probe

	push	ebx
	push	esi
	push	edi

	movzx	ebx, word ptr [edi].DDB_base_ioa
	or	bx, bx
	jz	xp_nobase

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

;  PHASE 2a - prove the read path on the safest target there is: LBA 0, one
;  sector, read-only, on a drive this routine has just established is idle.
;  Wiring reads into the IOS request path means claiming the device, and the
;  only disk in the test bed is the boot disk - that comes after this works.
	mov	XTIDE_ReqLba, 0
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_SecBuf
	call	XTIDE_ReadSectors
	jc	xp_exit			; 7, 8 or 9 already in XTIDE_FailCode

;  A sector that came back with an MBR signature is a sector that came back.
	cmp	word ptr XTIDE_SecBuf+510, 0AA55h
	jne	xp_nosig

;  Remember which sector was read, so the delay can report WHICH one rather
;  than merely that 512 bytes arrived.
	movzx	eax, byte ptr XTIDE_SecBuf+01BEh+4
	mov	XTIDE_PartType, eax

;  PHASE 2b - the write path, on a SLAVE if one is fitted, never on the master.
;  The master is the boot disk both here and on the 5160.
;
;  NOTE: XTIDE_CaptureGeometry overwrites one global set of geometry, so the
;  slave's values replace the master's from here on. That is fine while this is
;  a self-test and nothing addresses the master afterwards; per-unit geometry
;  arrives with the DCB work, which needs it anyway.
	mov	XTIDE_TryUnit, DRVHD_SLAVE
	call	XTIDE_TryIdentify
	jc	xp_noslave

	mov	XTIDE_SlavePresent, 1
	mov	esi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_CaptureGeometry
	call	XTIDE_WriteReadBack
	jc	xp_exit			; 7, 9, 12, 13, 14 or 15 already set

	mov	eax, XTIDE_PartType	; read AND write verified
	and	eax, 0Fh
	add	eax, 40
	mov	XTIDE_FailCode, eax
	jmp	xp_exit

;  No slave is not a failure - the real card has one drive. Report the phase 2a
;  result: the read verified, there was nothing safe to write to.
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

xp_nobase:
	mov	XTIDE_FailCode, 1	; no I/O resource reached the DDB

;  NEVER fail AEP_INITIALIZE.
;
;  Failing it makes IOS drop the driver permanently - and on this machine that
;  is a one-shot: resources may not be assigned until CONFIGMG's CONFIG_START,
;  which arrives after initialisation. Fail here and that callback never comes.
;  Device Manager's code 10 is IOS reporting that we hung up on it.
;
;  zikolas/cfu1-win9x (Nick, MIT) does the same thing and says why in one line
;  of comment: "AEP_SUCCESS (stay resident either way)". Claiming a device is
;  AEP_DEVICE_INQUIRY's job, not this one, and "no device" there is
;  AEP_NO_MORE_DEVICES - still not a failure.
xp_exit:
	mov	eax, XTIDE_FailCode
	call	XTIDE_Delay		; the boot log is the readback channel
	pop	edi
	pop	esi
	pop	ebx
	clc				; always. See above.
	ret

EndProc XTIDE_Probe

VxD_LOCKED_CODE_ENDS

	end
