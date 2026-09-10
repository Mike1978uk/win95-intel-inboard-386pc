;============================================================================
; LS120TR.ASM - transport for a parallel-port LS-120 (Shuttle EPAT bridge).
;
;   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #22).
;
;   Split out of LS120MP.ASM for the same reason XTIDETR.ASM is split out of
;   XTIDEMP.ASM: the SRB translation and the wire protocol fail in different
;   ways and are worth reading separately.
;
;   THREE LAYERS, and only the middle one is unknown:
;
;     1. Parallel port registers          - IEEE 1284, standard, WRITTEN BELOW.
;     2. EPAT bridge register access      - PROPRIETARY. Stubs, see below.
;     3. ATAPI taskfile over layer 2      - ATA/ATAPI standard, WRITTEN BELOW.
;
;   Layer 2 is the only part blocked on the licence decision recorded in
;   README.md. Everything either side of it is a documented standard and is
;   implemented here, so when layer 2 lands it drops into an interface that is
;   already exercised by the layers above and below it.
;
;   NOTHING IN THIS FILE IS COMPILED INTO THE PHASE-0 BINARY. It is assembled
;   only when the build is given -Phase 2, so the skeleton that is staged for
;   the keyboard test is unaffected. Verified by code hash, not by assertion.
;
;   *** UNTESTED. No part of this file has executed on any machine. ***
;============================================================================

	.386
	.MODEL	FLAT, STDCALL
	OPTION	CASEMAP:NONE

;============================================================================
;  LAYER 1 - the parallel port itself.
;
;  Standard IEEE 1284 register file. Three registers at the base, plus the EPP
;  extensions at +3..+4 which the bridge uses in its faster modes.
;
;    +0  DATA     write (or read, in bidirectional mode)
;    +1  STATUS   read only:  bit7 /BUSY  bit6 ACK  bit5 PE  bit4 SEL  bit3 /ERR
;    +2  CONTROL  read/write: bit0 STROBE bit1 AUTOFD bit2 /INIT bit3 SELIN
;                             bit5 direction (1 = input, bidirectional ports)
;    +3  EPP address
;    +4  EPP data
;
;  Note the inversions, because getting them wrong is the classic parallel-port
;  bug: STATUS bit 7 is inverted in hardware (BUSY low reads as 1), and CONTROL
;  bits 0, 1 and 3 are inverted on the wire. The accessors below hide none of
;  that - callers deal in raw register values, as the DOS driver does.
;============================================================================
LPT_DATA		equ	0
LPT_STATUS		equ	1
LPT_CONTROL		equ	2
LPT_EPP_ADDR		equ	3
LPT_EPP_DATA		equ	4

;  STATUS bits, as read.
LPT_ST_NBUSY		equ	80h		; inverted: 1 = not busy
LPT_ST_ACK		equ	40h
LPT_ST_PE		equ	20h
LPT_ST_SEL		equ	10h
LPT_ST_NERR		equ	08h

;  CONTROL bits, as written.
LPT_CT_STROBE		equ	01h		; inverted on the wire
LPT_CT_AUTOFD		equ	02h		; inverted on the wire
LPT_CT_NINIT		equ	04h		; NOT inverted
LPT_CT_SELIN		equ	08h		; inverted on the wire
LPT_CT_IRQEN		equ	10h
LPT_CT_DIR		equ	20h		; 1 = input, bidirectional only

;  ECP register file, offset from the SPP base. Not used by the nibble path.
LPT_ECP_FIFO		equ	400h		; ECP data FIFO
LPT_ECP_ECR		equ	402h		; bit 0 = FIFO empty

;  Shuttle EPAT connect-protocol packet. VERIFIED on the real 5160 2026-09-08.
;  These are CONNECT and DISCONNECT commands, not transfer modes - a mistake
;  that cost a day, because [0C02] reads 30h at rest and E0h after real drive
;  traffic, which looks exactly like a mode byte.
CPP_CONNECT		equ	0E0h
CPP_DISCONNECT		equ	030h

;  cont_map - the bridge addresses three spaces, and a register number alone
;  is meaningless without one. EVERY probe on 2026-09-08 used bare numbers 0-7,
;  which is empty internal-bridge space, and read uniform 00 or FF all day.
CONT_BRIDGE		equ	000h		; internal bridge registers
CONT_IDECTL		equ	010h		; IDE control
CONT_TASKFILE		equ	018h		; IDE task file - ATA status is 018h+7

;  WRITE Normal tags the register number; NIBBLE read sends it bare.
NIB_WRITE_TAG		equ	060h

;  Device control lives in the IDE CONTROL space, not the task file: cont 1,
;  register 6 -> offset 016h. Pulsing SRST here is what brings a cold drive up
;  (TRANSPORT_SPEC.md section 5). Before the pulse EVERY task-file register
;  reads 00h, which looks exactly like an absent bridge and is not.
ATA_REG_DEVCTL		equ	6
ATA_DEVCTL_SRST		equ	004h
ATA_ST_BSY		equ	080h

;  ATAPI signature, left in the cylinder registers after a reset. This, not the
;  status register, is the proof a drive is there: an ATAPI device holds DRDY
;  clear until its first packet command, so status legitimately reads 00h.
ATAPI_SIG_LO		equ	014h
ATAPI_SIG_HI		equ	0EBh

;  Vendor bring-up constants, transcribed from SD120PPD.SYS 0x2C42 (section 4i).
CPP_PRE1		equ	030h
CPP_PRE2		equ	040h
CPP_PRE3		equ	050h
CPP_PRE4		equ	000h
ECR_KEEP_MASK		equ	034h

;============================================================================
;  LAYER 3 - ATAPI, once layer 2 can reach its registers.
;
;  An LS-120 behind this bridge is an ordinary ATAPI device. ATAPI register
;  indices are the ATA taskfile's, reused:
;============================================================================
ATA_REG_DATA		equ	0
ATA_REG_ERR		equ	1		; read
ATA_REG_FEATURES	equ	1		; write
ATA_REG_ICR		equ	2		; interrupt reason (ATAPI)
ATA_REG_SECNUM		equ	3
ATA_REG_BCLO		equ	4		; byte count low  (ATAPI)
ATA_REG_BCHI		equ	5		; byte count high (ATAPI)
ATA_REG_DRVHD		equ	6
ATA_REG_STATUS		equ	7		; read
ATA_REG_CMD		equ	7		; write

ATA_ST_BSY		equ	80h
ATA_ST_DRDY		equ	40h
ATA_ST_DRQ		equ	08h
ATA_ST_ERR		equ	01h

;  Interrupt reason bits, which is how ATAPI says what phase it is in.
ATAPI_IR_CD		equ	01h		; 1 = command, 0 = data
ATAPI_IR_IO		equ	02h		; 1 = to host, 0 = to device

ATA_CMD_PACKET		equ	0A0h
ATA_CMD_IDENTIFY_PKT	equ	0A1h

;  The ATAPI command packet is twelve bytes, and it IS a SCSI CDB. That is the
;  single most useful fact in this file: SCSIPORT hands us a CDB in the SRB and
;  for a 12-byte command it goes on the wire unchanged. See LS120MP.ASM.
ATAPI_PACKET_LEN	equ	12

;  Spin bounds. Deliberately finite: every wait in this driver returns a
;  failure rather than hanging, because a driver that can spin forever wedges
;  a shutdown, and that cost this project four sessions on the port driver
;  (technique 88).
LS_SPIN_BSY		equ	400000
LS_SPIN_DRQ		equ	400000

	.DATA

	PUBLIC	LS_BasePort
LS_BasePort		dw	0378h		; set by the miniport from PORT=

	PUBLIC	LS_Mode
LS_Mode			dd	0		; negotiated transfer mode, 0 = none

	PUBLIC	LS_Connected
LS_Connected		dd	0		; layer 2 currently owns the port

	PUBLIC	LS_LastStatus
LS_LastStatus		dd	0		; last ATA status byte read

	.CODE

;============================================================================
;  LAYER 1 - parallel port accessors.
;
;  These are complete and correct, and they are the only code in this file
;  that touches hardware directly. Everything above goes through layer 2.
;============================================================================

;----------------------------------------------------------------------------
; LS_WriteData / LS_ReadData / LS_WriteControl / LS_ReadStatus / LS_ReadControl
;
;   In:   AL = value (write forms)
;   Out:  AL = value (read forms)
;   Uses: EAX, EDX
;----------------------------------------------------------------------------
public LS_WriteData
LS_WriteData:
	movzx	edx, LS_BasePort
	add	edx, LPT_DATA
	out	dx, al
	ret

public LS_ReadData
LS_ReadData:
	movzx	edx, LS_BasePort
	add	edx, LPT_DATA
	in	al, dx
	ret

public LS_ReadStatus
LS_ReadStatus:
	movzx	edx, LS_BasePort
	add	edx, LPT_STATUS
	in	al, dx
	ret

public LS_WriteControl
LS_WriteControl:
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	out	dx, al
	ret

public LS_ReadControl
LS_ReadControl:
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	in	al, dx
	ret

;============================================================================
;  LAYER 2 - the Shuttle EPAT bridge. IMPLEMENTED 2026-09-10.
;
;  Route B was taken and it worked: the protocol was recovered from
;  SD120PPD.SYS, which we hold, and confirmed by replay on the real 5160.
;  **This directory stays MIT.** No vendor code is transcribed - register
;  numbers, port sequences and protocol facts are not copyrightable.
;
;  Linux's drivers/block/paride/epat.c (GPL-2.0) describes the identical CPP
;  sequence and the same cont_map offsets. It was read ONLY as an independent
;  cross-check on facts already derived here, and no code was taken. Do not
;  paste from it; two unrelated sources agreeing is the value, not the source.
;
;  Full derivation, evidence tags and the measured values:
;  drivers/imation_ls120/TRANSPORT_SPEC.md, section 4e.
;
;  The shape, and why:
;
;    NIBBLE for the task file - a few bytes per command, so the slow path
;    costs nothing measurable, and it uses ONLY the three standard SPP ports,
;    so it works on a plain XT parallel port with no ECP hardware at all.
;
;    ECP rep insb/outsb for SECTOR DATA - that is where every byte moves.
;    DERIVED, not yet verified, so it is not wired up here yet.
;
;  Per-register ECP reads time out on this machine under every connect mode
;  tried. That is not a defect: a register read has no data phase, so the
;  reverse FIFO never fills. It does not block a driver and it costs no
;  throughput, because the throughput is all in the block path.
;
;  WARNING - the vendor's DMA-assisted block path writes ports 22h/23h, which
;  alias onto the 8259 on this XT. That is the issue #22 keyboard-killer, and
;  it lives in the TRANSFER path, not merely in chipset init. Never copy it.
;  dmaEn stays 0 regardless (technique 62: 20-bit DMA reach).
;============================================================================

;----------------------------------------------------------------------------
; LS_Connect - take the port and put the bridge into a known mode.
;
;   Out:  CF=0 on success, CF=1 if the bridge did not answer
;   Uses: EAX, ECX, EDX
;
;   STUB. Returns failure, so every layer above degrades honestly rather than
;   pretending to work. A stub that returned success would let the ATAPI layer
;   above spin against nothing and look like a hardware fault.
;----------------------------------------------------------------------------
public LS_Connect
LS_Connect:
	mov	al, CPP_CONNECT
	call	LS_CppFrame
	mov	LS_Connected, 1
;  Prove the drive answered rather than assuming it did - but test the ATAPI
;  SIGNATURE, not the status register. Status 00h is the correct post-reset
;  state for an ATAPI device (DRDY stays clear until the first packet command),
;  so the old "00h means absent" check rejected a working drive. FFh in the
;  cylinder registers still means nobody is driving the bus.
	mov	al, ATA_REG_BCLO
	call	LS_RegRead
	jc	lcon_fail
	cmp	al, ATAPI_SIG_LO
	jne	lcon_fail
	mov	al, ATA_REG_BCHI
	call	LS_RegRead
	jc	lcon_fail
	cmp	al, ATAPI_SIG_HI
	jne	lcon_fail
	mov	al, ATA_REG_STATUS
	call	LS_RegRead
	jc	lcon_fail
	movzx	eax, al
	mov	LS_LastStatus, eax
	clc
	ret
lcon_fail:
	mov	LS_Connected, 0
	stc
	ret

;----------------------------------------------------------------------------
; LS_AtaSoftReset - take the drive out of reset.
;
;   Out:  CF=0 if BSY cleared, CF=1 on timeout
;   Uses: EAX, ECX, EDX
;
;   THIS IS THE COLD BRING-UP. The drive powers up held in ATA reset; until
;   SRST is pulsed every task-file register reads 00h and the bridge looks
;   dead. Five hypotheses drawn from epat.c failed against this, because
;   epat.c is the parallel-port BRIDGE driver and cannot contain it - in Linux
;   the reset lives one layer up, in pcd.c. TRANSPORT_SPEC.md section 5.
;
;   Must be called with the bridge connected.
;----------------------------------------------------------------------------
public LS_AtaSoftReset
LS_AtaSoftReset:
	push	ebx
	mov	ch, CONT_IDECTL
	mov	ax, (ATA_DEVCTL_SRST SHL 8) OR ATA_REG_DEVCTL
	call	LS_RegWriteCont		; SRST asserted
	mov	ch, CONT_IDECTL
	mov	ax, (0 SHL 8) OR ATA_REG_DEVCTL
	call	LS_RegWriteCont		; SRST released - status reads 80h here
;  Poll BSY down. A nibble read is ~8 port accesses at ~3.9 us of Inboard-to-
;  XT-bus sync each, so this loop is self-timing at roughly 30 us an iteration
;  - no calibrated delay to get wrong, and none to go stale on other hardware.
	mov	ecx, 0FFFFh
lsrst_wait:
	push	ecx
	mov	al, ATA_REG_STATUS
	call	LS_RegRead
	pop	ecx
	jc	lsrst_next
	test	al, ATA_ST_BSY
	jz	lsrst_done
lsrst_next:
	dec	ecx
	jnz	lsrst_wait
	pop	ebx
	stc
	ret
lsrst_done:
	pop	ebx
	clc
	ret

;----------------------------------------------------------------------------
; LS_BringUp - the vendor's cold port-open, then the reset that matters.
;
;   Out:  CF=0 if the drive answered with the ATAPI signature
;   Uses: EAX, ECX, EDX
;
;   Steps 1-3 are transcribed from SD120PPD.SYS 0x2C42 (section 4i). They are
;   retained because the vendor does them and they are harmless, NOT because
;   they were shown to be necessary - on this hardware the soft reset alone
;   was what unblocked it. A later test can remove them.
;----------------------------------------------------------------------------
public LS_BringUp
LS_BringUp:
;  1. Mask the ECP control register rather than storing a value into it. An
;     earlier probe wrote 34h outright, which is a different operation.
	movzx	edx, LS_BasePort
	add	edx, LPT_ECP_ECR
	in	al, dx
	and	al, ECR_KEEP_MASK
	out	dx, al
;  2. The control-port kick. 0Eh asserts AUTOFEED alongside SELECT IN and
;     nINIT; nothing before 2026-09-10 had ever written 0Ch or 0Eh here.
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, LPT_CT_NINIT
	out	dx, al
	mov	al, LPT_CT_NINIT OR LPT_CT_SELIN
	out	dx, al
	mov	al, LPT_CT_NINIT OR LPT_CT_SELIN OR LPT_CT_AUTOFD
	out	dx, al
	out	dx, al
	out	dx, al
	mov	al, LPT_CT_NINIT
	out	dx, al
	out	dx, al
;  3. The CPP preamble.
	mov	al, CPP_PRE1
	call	LS_CppFrame
	mov	al, CPP_PRE2
	call	LS_CppFrame
	mov	al, CPP_PRE3
	call	LS_CppFrame
	mov	al, CPP_PRE4
	call	LS_CppFrame
;  4. Connect, reset, and only then believe anything the task file says.
	mov	al, CPP_CONNECT
	call	LS_CppFrame
	mov	LS_Connected, 1
	call	LS_AtaSoftReset
	jc	lbu_fail
	mov	al, ATA_REG_BCLO
	call	LS_RegRead
	jc	lbu_fail
	cmp	al, ATAPI_SIG_LO
	jne	lbu_fail
	mov	al, ATA_REG_BCHI
	call	LS_RegRead
	jc	lbu_fail
	cmp	al, ATAPI_SIG_HI
	jne	lbu_fail
	call	LS_Disconnect
	clc
	ret
lbu_fail:
	call	LS_Disconnect
	stc
	ret

;----------------------------------------------------------------------------
; LS_CppFrame - the EPAT connect-protocol packet.
;
;   In:   AL = CPP_CONNECT or CPP_DISCONNECT
;   Uses: EAX, EDX
;
;   VERIFIED on the real 5160, 2026-09-08, checkpoints reproducible twice.
;   Eight bytes to the SPP data port, EACH WRITTEN TWICE - the second write is
;   the I/O delay, not a retry - then commit by pulsing nINIT 04h/05h/04h.
;
;   Runs with interrupts off and restores IF from the CALLER's flags, which is
;   what the vendor's own dispatchers do. The handshake cannot tolerate an
;   interrupt landing in the middle of it (TRANSPORT_SPEC section 1).
;----------------------------------------------------------------------------
LS_CppFrame:
	push	ebx
	pushfd
	cli
	movzx	ebx, al		; the command byte goes last
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 04h
	out	dx, al
	movzx	edx, LS_BasePort	; edx = data port for the frame
	mov	al, 022h
	call	lcpp_twice
	mov	al, 0AAh
	call	lcpp_twice
	mov	al, 055h
	call	lcpp_twice
	mov	al, 000h
	call	lcpp_twice
	mov	al, 0FFh
	call	lcpp_twice
	mov	al, 087h
	call	lcpp_twice
	mov	al, 078h
	call	lcpp_twice
	mov	eax, ebx
	call	lcpp_twice
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 04h
	out	dx, al
	mov	al, 05h
	out	dx, al
	mov	al, 04h
	out	dx, al
	popfd			; restore the CALLER's IF, not an unconditional sti
	pop	ebx
	ret

lcpp_twice:
	out	dx, al
	out	dx, al		; the second write IS the I/O delay
	ret

;----------------------------------------------------------------------------
; LS_Disconnect - give the port back, leaving it as a plain printer port.
;   Uses: EAX, EDX
;   STUB.
;----------------------------------------------------------------------------
public LS_Disconnect
LS_Disconnect:
	mov	al, CPP_DISCONNECT
	call	LS_CppFrame
	mov	LS_Connected, 0
	ret

;----------------------------------------------------------------------------
; LS_RegRead - read one ATAPI taskfile register through the bridge.
;
;   In:   AL = register index (ATA_REG_*)
;   Out:  AL = value, CF=0 on success
;   Uses: EAX, ECX, EDX
;   STUB.
;----------------------------------------------------------------------------
public LS_RegRead
LS_RegRead:
;  NIBBLE Normal. The bridge returns a byte as two nibbles, each arriving in
;  the TOP four bits of the SPP status port, so the low nibble has to be
;  shifted down and the high one masked. Getting that pair the wrong way round
;  yields plausible-looking garbage rather than an obvious failure.
	push	ebx
	pushfd
	cli
	add	al, CONT_TASKFILE	; a bare register number addresses nothing
	movzx	edx, LS_BasePort
	out	dx, al
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 01h
	out	dx, al
	mov	al, 03h
	out	dx, al
	out	dx, al
	movzx	edx, LS_BasePort
	add	edx, LPT_STATUS
	in	al, dx
	and	al, 0F0h
	shr	al, 4		; LOW nibble, arrives in bits 7-4
	mov	bl, al
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 04h
	out	dx, al
	out	dx, al
	movzx	edx, LS_BasePort
	add	edx, LPT_STATUS
	in	al, dx
	and	al, 0F0h		; HIGH nibble, already in place
	or	al, bl
	movzx	eax, al
	popfd
	pop	ebx
	clc
	ret

;----------------------------------------------------------------------------
; LS_RegWrite - write one ATAPI taskfile register through the bridge.
;
;   In:   AL = register index, AH = value
;   Out:  CF=0 on success
;   Uses: EAX, ECX, EDX
;   STUB.
;----------------------------------------------------------------------------
public LS_RegWrite
LS_RegWrite:
;  WRITE Normal. Note the asymmetry with the read above - a write TAGS the
;  register number with 60h, a read sends it bare.
	mov	ch, CONT_TASKFILE
	jmp	short LS_RegWriteCont

;  Same write, into a caller-chosen container. Device control is the only
;  register outside the task file the driver touches, but it is the one that
;  matters - see LS_AtaSoftReset.
;   In:   AL = register index, AH = value, CH = CONT_* container offset
public LS_RegWriteCont
LS_RegWriteCont:
	push	ebx
	pushfd
	cli
	mov	bl, ah			; the value
	add	al, ch
	or	al, NIB_WRITE_TAG
	movzx	edx, LS_BasePort
	out	dx, al
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 01h
	out	dx, al
	out	dx, al
	movzx	edx, LS_BasePort
	mov	al, bl
	out	dx, al		; the value
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 04h
	out	dx, al
	popfd
	pop	ebx
	clc
	ret

;----------------------------------------------------------------------------
; LS_BlockRead / LS_BlockWrite - bulk data through the bridge's data path.
;
;   In:   EDI (read) / ESI (write) = flat buffer, ECX = byte count
;   Out:  CF=0 on success
;   Uses: EAX, ECX, EDX, ESI, EDI
;   STUBS.
;----------------------------------------------------------------------------
public LS_BlockRead
LS_BlockRead:
;  Nibble, one byte at a time out of the ATA data register. CORRECT and SLOW:
;  five port accesses per byte, and 2026-09-10 measured a port access on this
;  machine at 5.77 us, so a 512-byte sector costs roughly 15 ms.
;
;  The ECP block path is the fix and it is NOT wired up: rep insb out of the
;  FIFO at base+400h after setting control bit 5 for reverse. That is DERIVED
;  from the vendor binary and has never been executed, so it is not shipped.
;  Ship what can be tested; make it fast once it is proven.
	or	ecx, ecx
	jz	lbr_done
lbr_loop:
	push	ecx
	mov	al, ATA_REG_DATA
	call	LS_RegRead
	pop	ecx
	jc	lbr_fail
	mov	[edi], al
	inc	edi
	dec	ecx
	jnz	lbr_loop
lbr_done:
	clc
	ret
lbr_fail:
	stc
	ret

public LS_BlockWrite
LS_BlockWrite:
;  Nibble twin of LS_BlockRead. Same cost, same reason, same fix pending.
	or	ecx, ecx
	jz	lbw_done
lbw_loop:
	push	ecx
	mov	ah, [esi]
	mov	al, ATA_REG_DATA
	call	LS_RegWrite
	pop	ecx
	jc	lbw_fail
	inc	esi
	dec	ecx
	jnz	lbw_loop
lbw_done:
	clc
	ret
lbw_fail:
	stc
	ret

;============================================================================
;  LAYER 3 - ATAPI over layer 2. Complete, and calls only the layer-2 API.
;
;  This is written against the stubs above, so it assembles and is reviewable
;  now, and needs no change when layer 2 lands.
;============================================================================

;----------------------------------------------------------------------------
; LS_WaitNotBusy - spin until BSY clears, bounded.
;
;   Out:  AL = last status, CF=0 if BSY cleared, CF=1 on timeout
;   Uses: EAX, ECX
;----------------------------------------------------------------------------
public LS_WaitNotBusy
LS_WaitNotBusy:
	push	ebx
	mov	ebx, LS_SPIN_BSY
lwnb_loop:
	mov	al, ATA_REG_STATUS
	call	LS_RegRead
	jc	lwnb_fail
	mov	LS_LastStatus, eax
	test	al, ATA_ST_BSY
	jz	lwnb_ok
	dec	ebx
	jnz	lwnb_loop
lwnb_fail:
	pop	ebx
	stc
	ret
lwnb_ok:
	pop	ebx
	clc
	ret

;----------------------------------------------------------------------------
; LS_WaitDrq - spin until DRQ sets with BSY clear, bounded.
;
;   Out:  AL = last status, CF=0 if DRQ set, CF=1 on timeout or ERR
;   Uses: EAX, ECX
;----------------------------------------------------------------------------
public LS_WaitDrq
LS_WaitDrq:
	push	ebx
	mov	ebx, LS_SPIN_DRQ
lwd_loop:
	mov	al, ATA_REG_STATUS
	call	LS_RegRead
	jc	lwd_fail
	mov	LS_LastStatus, eax
	test	al, ATA_ST_BSY
	jnz	lwd_next
	test	al, ATA_ST_ERR
	jnz	lwd_fail
	test	al, ATA_ST_DRQ
	jnz	lwd_ok
lwd_next:
	dec	ebx
	jnz	lwd_loop
lwd_fail:
	pop	ebx
	stc
	ret
lwd_ok:
	pop	ebx
	clc
	ret

;----------------------------------------------------------------------------
; LS_PacketCommand - issue one ATAPI packet command and move its data.
;
;   In:   ESI = 12-byte command packet (the SRB's CDB, unchanged)
;         EDI = data buffer, or 0
;         ECX = data length in bytes, 0 for none
;         EDX = 0 for data-in (device to host), 1 for data-out
;   Out:  CF=0 on success; CF=1 with LS_LastStatus holding the ATA status
;   Uses: everything
;
;   The sequence is the ATAPI standard one:
;     1. select the device
;     2. set the byte-count registers to how much we will accept
;     3. write the PACKET opcode
;     4. wait DRQ, hand over the twelve-byte packet
;     5. wait DRQ, move the data in whichever direction
;     6. wait BSY clear and read the final status
;
;   No interrupt is used at any point: this driver polls, which is what lets
;   it declare no interrupt handler at all. See DriverEntry in LS120MP.ASM for
;   why that matters on this machine.
;----------------------------------------------------------------------------
public LS_PacketCommand
LS_PacketCommand:

	push	ebp
	mov	ebp, esp
	push	ebx
	push	esi
	push	edi

	sub	esp, 16
	mov	[ebp-16], esi		; packet
	mov	[ebp-20], edi		; buffer
	mov	[ebp-24], ecx		; length
	mov	[ebp-28], edx		; direction

	cmp	LS_Connected, 0
	jne	lpc_have
	call	LS_Connect
	jc	lpc_fail
lpc_have:

;  1. Select device 0. The bridge presents a single device.
	mov	ax, (0A0h SHL 8) OR ATA_REG_DRVHD
	call	LS_RegWrite
	jc	lpc_fail

	call	LS_WaitNotBusy
	jc	lpc_fail

;  2. Byte count = how much data we are prepared to move in one go. The device
;     may return less and says so in these same registers afterwards.
	mov	ecx, [ebp-24]
	mov	al, ATA_REG_BCLO
	mov	ah, cl
	call	LS_RegWrite
	jc	lpc_fail
	mov	ecx, [ebp-24]
	mov	al, ATA_REG_BCHI
	mov	ah, ch
	call	LS_RegWrite
	jc	lpc_fail

;  Features = 0: no DMA, no overlap. This driver is PIO only, deliberately -
;  on a 5160 the free DMA channels are not free (channel 0 is DRAM refresh).
	mov	ax, (0 SHL 8) OR ATA_REG_FEATURES
	call	LS_RegWrite
	jc	lpc_fail

;  3. PACKET.
	mov	ax, (ATA_CMD_PACKET SHL 8) OR ATA_REG_CMD
	call	LS_RegWrite
	jc	lpc_fail

;  4. The device asks for the packet by raising DRQ.
	call	LS_WaitDrq
	jc	lpc_fail

	mov	esi, [ebp-16]
	mov	ecx, ATAPI_PACKET_LEN
	call	LS_BlockWrite
	jc	lpc_fail

;  5. Data phase, if there is one.
	mov	ecx, [ebp-24]
	or	ecx, ecx
	jz	lpc_done

	call	LS_WaitDrq
	jc	lpc_fail

	mov	ecx, [ebp-24]
	cmp	dword ptr [ebp-28], 0
	jne	lpc_out

	mov	edi, [ebp-20]
	call	LS_BlockRead
	jc	lpc_fail
	jmp	lpc_done

lpc_out:
	mov	esi, [ebp-20]
	call	LS_BlockWrite
	jc	lpc_fail

;  6. Completion.
lpc_done:
	call	LS_WaitNotBusy
	jc	lpc_fail

	mov	eax, LS_LastStatus
	test	al, ATA_ST_ERR
	jnz	lpc_fail

;  Release the port. The vendor driver leaves it quiescent between
;  operations - measured 2026-09-08, control 14h and ECR 35h at rest - so
;  a connect is mandatory per operation, not once at init. Holding the
;  bridge open would also keep the parallel port from anyone else. The
;  frame is about 16 port writes, trivial against a whole command.
	call	LS_Disconnect
	add	esp, 16
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	clc
	ret

lpc_fail:
;  Release it on the error path too, or the next command inherits a
;  half-open bridge and fails for a reason that has nothing to do with it.
	call	LS_Disconnect
	add	esp, 16
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	stc
	ret


	END
