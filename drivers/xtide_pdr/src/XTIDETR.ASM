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
public XTIDE_Stride
XTIDE_Stride		dd	2

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

;  Set once a unit IDENTIFY validates. Phase 2 turns this into an array.
public XTIDE_MasterPresent
XTIDE_MasterPresent	dd	0
public XTIDE_SlavePresent
XTIDE_SlavePresent	dd	0

;  IDENTIFY landing area. 512 bytes, one device - phase 2 grows this.
public XTIDE_IdBuf
XTIDE_IdBuf		db	512 dup (0)

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

	add	esi, ID_MODEL_OFF
	mov	ecx, ID_MODEL_LEN
	xor	edx, edx		; printable, non-blank count
xvi_loop:
	mov	al, [esi]
	cmp	al, 20h
	jb	xvi_bad
	cmp	al, 7Eh
	ja	xvi_bad
	je	xvi_next
	cmp	al, 20h
	je	xvi_next
	inc	edx
xvi_next:
	inc	esi
	dec	ecx
	jnz	xvi_loop

	cmp	edx, ID_MIN_TEXT	; all blanks is not a model string
	jb	xvi_bad

	pop	edx
	pop	ecx
	pop	esi
	clc
	ret

xvi_bad:
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
	jc	xti_fail

	mov	edx, XTIDE_pDrvHd	; select the unit under test
	mov	eax, XTIDE_TryUnit
	out	dx, al

	call	XTIDE_WaitNotBusy	; honour the select
	jc	xti_fail

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
	jnz	xti_fail
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
	jc	xti_fail

	mov	edi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_ReadData

	mov	esi, OFFSET32 XTIDE_IdBuf
	call	XTIDE_ValidateId
	jc	xti_fail

	clc
	ret

xti_fail:
	stc
	ret

EndProc XTIDE_TryIdentify

;----------------------------------------------------------------------------
; XTIDE_Probe - PUBLIC. Called from Port_initialize once the device node
;               resources have been parsed into the DDB.
;
;   In:   EDI -> PortDDB (DDB_base_ioa already filled by Port_Scan_Inp_Params)
;   Out:  CF=0 if a drive answered and its IDENTIFY data validated
;   Uses: EAX, ECX, EDX  (EBX, ESI, EDI preserved - Port_initialize still
;         needs EBX as the AEP pointer and EDI as the DDB)
;----------------------------------------------------------------------------
BeginProc XTIDE_Probe

	push	ebx
	push	esi
	push	edi

	movzx	ebx, word ptr [edi].DDB_base_ioa
	or	bx, bx
	jz	xp_fail			; no I/O resource - nothing to talk to

	call	XTIDE_SetPorts

	mov	XTIDE_TryUnit, DRVHD_MASTER

	mov	eax, XTIDE_Transport
	or	eax, eax
	jz	xp_auto

	mov	XTIDE_TryTransport, eax	; explicit override, no detection
	call	XTIDE_TryIdentify
	jc	xp_fail
	jmp	xp_ok

xp_auto:
	mov	XTIDE_TryTransport, XT_TR_PIO8	; measured first on this card
	call	XTIDE_TryIdentify
	jnc	xp_ok

	mov	XTIDE_TryTransport, XT_TR_LATCH
	call	XTIDE_TryIdentify
	jc	xp_fail

xp_ok:
	mov	eax, XTIDE_TryTransport
	mov	XTIDE_ActiveTransport, eax
	mov	XTIDE_MasterPresent, 1
	pop	edi
	pop	esi
	pop	ebx
	clc
	ret

xp_fail:
	pop	edi
	pop	esi
	pop	ebx
	stc
	ret

EndProc XTIDE_Probe

VxD_LOCKED_CODE_ENDS

	end
