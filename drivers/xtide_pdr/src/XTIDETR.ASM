;============================================================================
; XTIDETR.ASM - XT-IDE transport for a Windows 95 IOS port driver
;
;   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #21).
;   Written for the IBM 5160 + Intel Inboard 386/PC, but deliberately not
;   specific to it: the transport is selectable, because XTIDE Universal BIOS
;   drives a family of electrically different adapters and a driver that
;   hardcodes one of them is a driver for one machine.
;
;   Register map cross-checked against 86Box's src/disk/hdc_xtide.c and against
;   the XT-IDE port of sava/lpproj's patacd.asm by Miran Grca (@OBattler).
;   NOTE those two are not independent - same author - so neither confirms the
;   other. See drivers/xtide_cdrom/README.md.
;
;   PHASE 1: IDENTIFY only. No writes, no sector reads yet. The point is to
;   prove we can address the card and get a coherent answer back.
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
;  Register offsets from the card's base I/O address.
;
;  The control block sits at base+8 (XTIDE_CONTROL_BLOCK_OFFSET), not at the
;  standard ATA +0x206, so alternate status is base+8+6 = base+0Eh. Measured
;  on the real card 2026-08-30: 0x30E returned 50h = DRDY|DSC.
;============================================================================
XT_DATA		equ	0		; data (low byte in latch mode)
XT_FEATURES	equ	1
XT_ERROR	equ	1
XT_SECCNT	equ	2
XT_SECNUM	equ	3
XT_CYLLO	equ	4
XT_CYLHI	equ	5
XT_DRVHD	equ	6
XT_CMD		equ	7		; write
XT_STATUS	equ	7		; read
XT_HILATCH	equ	8		; high byte latch (latch transports only)
XT_ALTSTAT	equ	0Eh		; read
XT_DEVCTRL	equ	0Eh		; write

ATA_IDENTIFY	equ	0ECh

ST_BSY		equ	80h
ST_DRDY		equ	40h
ST_DRQ		equ	08h
ST_ERR		equ	01h

DRVHD_MASTER	equ	0A0h
DRVHD_SLAVE	equ	0B0h		; DEV bit set

;----------------------------------------------------------------------------
;  Transports. AUTO tries each in turn and keeps the one whose IDENTIFY data
;  validates. An explicit value skips detection - see XTIDE_Transport.
;----------------------------------------------------------------------------
XT_TR_AUTO	equ	0
XT_TR_LATCH	equ	1		; 16-bit: low at +0 (latches high), high at +8
XT_TR_PIO8	equ	2		; 8-bit: every byte through +0

XT_SPIN		equ	400000		; status poll limit; no timer at init

;  IDENTIFY DEVICE result layout (ATA-1 onwards)
ID_MODEL_OFF	equ	54		; 40-byte model string, word 27..46
ID_MODEL_LEN	equ	40
ID_MIN_TEXT	equ	4		; minimum non-blank printable chars

;============================================================================
VxD_LOCKED_DATA_SEG

;  Override. Zero = autodetect. Set from the device node by the INF so an
;  unusual card can be forced without a rebuild.
public XTIDE_Transport
XTIDE_Transport		dd	XT_TR_AUTO

;  What detection settled on. Read by phase 2's transfer path.
public XTIDE_ActiveTransport
XTIDE_ActiveTransport	dd	XT_TR_AUTO

;  Transport currently under test.
XTIDE_TryTransport	dd	XT_TR_AUTO

;  Drive/head value for the unit under test. Phase 1 probes the master only,
;  but the whole path is parameterised so v2's slave support is a second call
;  rather than a rewrite. Costs nothing to carry.
XTIDE_TryUnit		dd	DRVHD_MASTER

;  Set once a unit's IDENTIFY validates. Phase 2 turns this into an array.
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
; XTIDE_WaitNotBusy - poll alternate status until BSY clears.
;
;   In:   BX  = base I/O address
;   Out:  CF=0 and AL = status, or CF=1 on timeout
;   Uses: EAX, ECX, EDX
;
;   Alternate status is used rather than the status register because reading
;   status proper acknowledges a pending interrupt. The card is jumpered
;   without one here, but a driver that only works on a polled card is the
;   thing this file is trying not to be.
;----------------------------------------------------------------------------
BeginProc XTIDE_WaitNotBusy

	movzx	edx, bx
	add	edx, XT_ALTSTAT
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
;   In:   BX  = base I/O address
;   Out:  CF=0 data is ready, CF=1 on error or timeout
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_WaitDrq

	movzx	edx, bx
	add	edx, XT_ALTSTAT
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
;   In:   BX  = base I/O address
;         EDI -> destination buffer
;         XTIDE_TryTransport selects how
;   Uses: EAX, ECX, EDX, EDI
;
;   LATCH: reading base+0 returns the low byte AND latches the high byte,
;   which is then read from base+8. Low-then-high on reads; writes are the
;   other way round (high to +8 first, then low to +0 commits the word) - that
;   asymmetry is phase 2's problem and is easy to get backwards.
;
;   PIO8: the CF is in 8-bit mode and every byte comes through base+0.
;----------------------------------------------------------------------------
BeginProc XTIDE_ReadData

	movzx	edx, bx
	cmp	XTIDE_TryTransport, XT_TR_PIO8
	je	xrd_pio8

	mov	ecx, 256		; words
xrd_latch_loop:
	in	al, dx			; low byte, latches the high byte
	mov	[edi], al
	add	dx, XT_HILATCH
	in	al, dx			; high byte out of the latch
	mov	[edi+1], al
	sub	dx, XT_HILATCH
	add	edi, 2
	dec	ecx
	jnz	xrd_latch_loop
	ret

xrd_pio8:
	mov	ecx, 512		; bytes
xrd_pio8_loop:
	in	al, dx
	mov	[edi], al
	inc	edi
	dec	ecx
	jnz	xrd_pio8_loop
	ret

EndProc XTIDE_ReadData

;----------------------------------------------------------------------------
; XTIDE_ValidateId - does this look like real IDENTIFY data?
;
;   In:   ESI -> 512-byte IDENTIFY buffer
;   Out:  CF=0 if the model string is sane, CF=1 otherwise
;   Uses: EAX
;
;   This is what makes the transport self-verifying, and it is the whole
;   reason autodetect is possible. The model string is 40 bytes of printable
;   ASCII at a fixed offset. Read through the WRONG transport it comes back
;   mangled - byte-duplicated, in the case of a latch read that never fetches
;   the high byte. That exact bug shipped in one of the two binaries in
;   drivers/xtide_cdrom/, which is where the idea came from.
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
; XTIDE_TryIdentify - issue IDENTIFY DEVICE to the master and validate it.
;
;   In:   BX = base I/O address, XTIDE_TryTransport selects the transport
;   Out:  CF=0 on success, XTIDE_IdBuf filled
;   Uses: EAX, ECX, EDX, EDI, ESI
;
;   IDENTIFY does not touch media, so this is safe to issue against a drive
;   the real-mode BIOS is still driving - provided it is idle, which the
;   leading WaitNotBusy establishes.
;----------------------------------------------------------------------------
BeginProc XTIDE_TryIdentify

	call	XTIDE_WaitNotBusy	; drive idle before we touch anything?
	jc	xti_fail

	movzx	edx, bx			; select the unit under test
	add	edx, XT_DRVHD
	mov	eax, XTIDE_TryUnit
	out	dx, al

	call	XTIDE_WaitNotBusy	; honour the select
	jc	xti_fail

;  A present slave answers with its own DEV bit; an absent one leaves the bus
;  floating or mirrors the master. Reading Drive/Head back and comparing is how
;  ATASelectDevice in drivers/xtide_cdrom/ tells them apart, and it is the only
;  reliable way to do it.
;
;  EDX MUST BE RELOADED FIRST. XTIDE_WaitNotBusy returns with EDX pointing at
;  the alternate status register, and DSC there occupies the same bit as DEV
;  here. Reading the wrong port is what made phase 1 fail on 2026-08-31: alt
;  status was 50h, bit 4 set, so every probe failed in under a millisecond
;  without ever reaching a transfer. See inboard-hw-debug technique 78.
;
;  Slave only. A master that does not read Drive/Head back is not thereby
;  broken, and the model string is stronger evidence than this check. Phase 1
;  has no readback channel, so a test whose only outcome is a silent failure
;  costs information rather than buying it.
	cmp	XTIDE_TryUnit, DRVHD_MASTER
	je	xti_selected

	movzx	edx, bx
	add	edx, XT_DRVHD
	in	al, dx
	mov	ah, byte ptr XTIDE_TryUnit
	xor	al, ah
	test	al, 10h			; DEV bit as selected?
	jnz	xti_fail
xti_selected:

	movzx	edx, bx			; IDENTIFY takes no parameters, but
	add	edx, XT_SECCNT		; leaving stale ones is asking for it
	xor	al, al
	out	dx, al
	inc	edx			; sector number
	out	dx, al
	inc	edx			; cylinder low
	out	dx, al
	inc	edx			; cylinder high
	out	dx, al

	movzx	edx, bx
	add	edx, XT_CMD
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
; XTIDE_Probe - PUBLIC. Called from Port_initialize once the device node's
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

	mov	eax, XTIDE_Transport
	or	eax, eax
	jz	xp_auto

	mov	XTIDE_TryUnit, DRVHD_MASTER
	mov	XTIDE_TryTransport, eax	; explicit override, no detection
	call	XTIDE_TryIdentify
	jc	xp_fail
	jmp	xp_ok

xp_auto:
	mov	XTIDE_TryUnit, DRVHD_MASTER
	mov	XTIDE_TryTransport, XT_TR_LATCH
	call	XTIDE_TryIdentify
	jnc	xp_ok

	mov	XTIDE_TryTransport, XT_TR_PIO8
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
