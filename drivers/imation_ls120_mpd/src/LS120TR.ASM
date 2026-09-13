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
;  Bounded in real time, not in arbitrary iterations. One poll is a nibble
;  read: ~8 port accesses at ~3.9 us of Inboard-to-XT-bus sync each, so an
;  iteration costs roughly 31 us. 400000 of them was 12 SECONDS, long enough
;  for SCSIPORT to time the SRB out and abandon the device - which is exactly
;  how the first working transport still failed to produce a drive letter.
;  MEASURED 2026-09-10 (technique 109): one 8-bit I/O access on this machine
;  costs 5.55 us, not the ~3.9 us assumed above, so a nibble read is 44.4 us
;  and these were 5.8 s and 2.9 s, not 4 s and 2 s. Several of those during
;  SCSIPORT's bus scan is the boot hang.
;  Re-derived: 1 s / 44.4 us = 22522, and half that. Anything legitimately
;  slower (a spin-up) belongs to the class driver and its own timeout. The
;  DDK's PC2X sample bounds its waits in TIME, calling ScsiPortStallExecution(1)
;  per turn so the count IS microseconds - the right shape, worth adopting.
;  RESIZED 2026-09-11 for the restructure. These no longer have to cover a
;  spin-up, because LS_ST_READY in the miniport's timer waits for the drive
;  to stop being busy BEFORE a command is issued, one poll per 1 ms tick.
;  What is left here is the latency of a command the drive has already
;  accepted, which is milliseconds. Sizing them for the worst case was what
;  made a single tick block for a second at a time.
;
;  ~44 us per nibble register read, so 2000 = ~90 ms and 1000 = ~45 ms.
LS_SPIN_BSY		equ	2000		; ~90 ms
LS_SPIN_DRQ		equ	1000		; ~45 ms

	.DATA

	PUBLIC	LS_BasePort
LS_BasePort		dw	0378h		; set by the miniport from PORT=

;  REQUEST SENSE. MEASURED on the real drive 2026-09-11: a READ(10) after a
;  media change comes back status 51h, error register 64h - sense key 6,
;  UNIT ATTENTION - and transfers nothing. That is the device REFUSING the
;  command, not a broken data path. The class driver clears it by reading the
;  sense data and retrying; a miniport that never reports CHECK CONDITION
;  leaves it set for ever, so the volume never mounts ("media is not
;  formatted") and the dismount that would release PREVENT MEDIUM REMOVAL
;  never runs either - which is a drive that will not eject until you cut its
;  power. Both symptoms, one cause.
LS_SENSE_LEN		equ	18
LS_SenseCdb		db	003h, 0, 0, 0, LS_SENSE_LEN, 0, 0, 0, 0, 0, 0, 0
LS_SenseBuf		db	LS_SENSE_LEN dup (0)

;  A real INQUIRY, issued once by LS_BringUp so that BOOTLOG.TXT reports on the
;  whole packet path and not merely on the transport. BOOTLOG's Init
;  Success/Init Failure line is the only diagnostic channel readable from a
;  miniport on this machine (technique 74), so it is worth spending one command
;  to make it mean something. 12 bytes, zero-padded: ATAPI takes a SCSI CDB
;  unchanged.
LS_InqCdb		db	012h, 0, 0, 0, 24h, 0, 0, 0, 0, 0, 0, 0
LS_InqBuf		db	36 dup (0)

;  START STOP UNIT with the start bit. The vendor's Windows driver audibly
;  spins the drive during boot and we did not, which is what put us onto this:
;  an INQUIRY is answered by a spun-down drive, so it proves far less than it
;  looks like it does.
LS_StartCdb		db	01Bh, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0

;  TEST UNIT READY - "is there media and are you ready to talk about it".
LS_TurCdb		db	000h, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0

	PUBLIC	LS_Mode
LS_Mode			dd	0		; negotiated transfer mode, 0 = none

	PUBLIC	LS_Connected
LS_Connected		dd	0		; layer 2 currently owns the port

	PUBLIC	LS_LastStatus
LS_LastStatus		dd	0		; last ATA status byte read
LS_XferN		dd	0		; byte count the DEVICE offered this phase
LS_Phase		dd	0		; ATAPI interrupt reason, masked to 2 bits
LS_HasEcp		dd	0		; 1 = an ECR answered at base+402h, use the FIFO
	PUBLIC	LS_ModeReq
ifdef LS_FORCE_MODE
LS_ModeReq		dd	LS_FORCE_MODE	; PINNED AT BUILD TIME - a test build
else
LS_ModeReq		dd	0		; 0 = auto-detect, 1 = force SPP, 2 = force ECP
endif

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
;  Connect and nothing else. This deliberately does NOT probe.
;
;  It used to validate by reading the ATAPI signature out of BCLO/BCHI, which
;  is wrong in a way that only shows up on the SECOND command: those two ARE
;  the byte-count registers, so they hold 14/EB only until something issues a
;  packet - after which every connect read a byte count, failed, and returned
;  SRB_STATUS_ERROR. Enumeration died one INQUIRY in and no drive letter
;  appeared, with the transport working perfectly underneath.
;
;  The signature test belongs where it is now: once, in LS_BringUp, straight
;  after the reset that produces it. Probing per connect also costs two nibble
;  reads - about 16 port accesses at ~3.9 us of bus sync each - on every
;  single SRB, to learn nothing that init did not already establish.
	mov	al, CPP_CONNECT
	call	LS_CppFrame
	mov	LS_Connected, 1
	clc
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
;  base+402h is 077Ah, and on a 5160 that is worth checking rather than
;  assuming: 077Ah and 037Ah differ only in A10, which is exactly how 022h
;  lands on the 8259 (technique 75). MEASURED 2026-09-10 on the real machine -
;  writing 0Bh to 077Ah left 037Ah reading 04h, so this port does NOT alias
;  onto the control register. 077Ah reads 09h, a live ECR: SPP mode, FIFO
;  empty. The access is safe and the register is real.
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

;  5. One INQUIRY, so a failure anywhere in the ATAPI path shows up as Init
;     Failure in BOOTLOG rather than as a silently missing drive letter. That
;     is what the first hardware boot cost: transport perfect, Init Success,
;     no drive, and nothing to read. LS_PacketCommand releases the port on
;     both of its exits, so this does not disconnect afterwards.
;  POISON THE BUFFER FIRST. The previous version checked only that the device
;  type in byte 0 was not 1Fh - but an INQUIRY that silently did nothing leaves
;  the buffer at 00h, and 00h is a legitimate device type, so the test passed
;  on a completely dead path and reported Init Success. A self-test that cannot
;  fail is worse than none: it spends a boot and tells you nothing.
	lea	edi, LS_InqBuf
	mov	ecx, 36
	mov	al, 0AAh
	cld
	rep	stosb

	lea	esi, LS_InqCdb
	lea	edi, LS_InqBuf
	mov	ecx, 36
	xor	edx, edx		; data in
	call	LS_PacketCommand
	jc	lbu_fail

;  Drain the unit attentions the soft reset just raised, or the class driver's
;  very first READ is refused and the volume never mounts. INQUIRY above is
;  exempt from them, so this is invisible until something else is tried.
	call	LS_DrainSense

;  Byte 4 is Additional Length. 0AAh means nothing was written at all; a real
;  device reports at least 4 and usually 1Fh.
	mov	al, LS_InqBuf+4
	cmp	al, 0AAh
	je	lbu_fail
	cmp	al, 4
	jb	lbu_fail
;  Byte 8 starts the vendor ID, which is space-padded ASCII on any real drive.
;  Nothing else in the reply is as cheap to check or as hard to fake.
	mov	al, LS_InqBuf+8
	cmp	al, 020h
	jb	lbu_fail
	cmp	al, 07Eh
	ja	lbu_fail

;  The motor work that used to be here has MOVED to the first SRB (LS_SpinUp).
;  Spinning a drive up inside HwInitialize can block for many seconds - the
;  owner could hear it land just before the desktop appeared - and HwInitialize
;  is not the place for that. XTIDEMP.MPD, which enumerates correctly on this
;  machine, returns from init almost immediately.
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
	push	ecx
	mov	ch, CONT_TASKFILE
	jmp	short lrw_common

;  Same write, into a caller-chosen container. Device control is the only
;  register outside the task file the driver touches, but it is the one that
;  matters - see LS_AtaSoftReset.
;   In:   AL = register index, AH = value, CH = CONT_* container offset
public LS_RegWriteCont
LS_RegWriteCont:
	push	ecx
lrw_common:
;  ECX is saved rather than merely documented as clobbered. The packet builder
;  passes the byte count in CX and reads CL then CH one call apart; it happens
;  to reload ECX in between, but a register write that silently eats half the
;  caller's count is the kind of thing that is found later and expensively.
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
	pop	ecx
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
public LS_BlockReadSpp
;----------------------------------------------------------------------------
;  NIBBLE block read, mode 0, transcribed from epat.c's epat_read_block.
;
;  This is NOT a loop of register reads, which is what it used to be and why
;  every task-file register read correctly while the data register returned
;  zeros for good. The bridge is put into block mode ONCE and then streamed,
;  with an alternating PHASE bit driven on the control port. Nothing in a
;  per-register read expresses that handshake.
;
;  Proven on hardware 2026-09-11: the drive set byte count to 36 for an
;  INQUIRY - it had the data ready - and the old routine still read 36 zeros.
;
;   In:   EDI = buffer, ECX = byte count
;----------------------------------------------------------------------------
; ECP support.
;
;  The owner's card is ECP-capable and the vendor's own driver selects
;  "ECP Read" / "ECP Write" on this exact machine - captured live on
;  2026-09-08, TRANSPORT_SPEC.md section 0, [0BCB]/[0BCF]/[0BD7]=0C.
;  Nibble costs ~4 port accesses per byte; the ECP FIFO costs ~1, because the
;  hardware does the handshake. Keep BOTH: SPP/nibble is universal and is the
;  fallback for any port without an ECR.
;
;  Register layout, confirmed from SD120PPD.SYS's own arithmetic
;  (rva 0x3cd2: base -> +2 -> +0x402 -> -0x402 -> ...):
;
;      base+000h  data                base+002h  control (bit 5 = direction)
;      base+400h  ECP data FIFO       base+402h  ECR
;
;  ECR bits 7:5 are the mode - 0x74 selects ECP, 0x34 byte mode - and bit 0
;  reads 1 when the FIFO is empty.
;
;  ** The vendor's block routines sit next to chipset writes to 22h/23h, which
;  alias onto the 8259 here (technique 75). Those are GATED on a chipset type
;  ([0xbc0] == 3, SD120PPD.SYS rva 0x443c / 0x4ba7) and are jumped over when it
;  does not match. Nothing below writes any fixed port - every access is
;  base-relative, which is the same guarantee /ni gives on the real machine. **
;----------------------------------------------------------------------------
LPT_ECPFIFO	equ	400h

ECR_MODE_ECP	equ	074h
ECR_FIFO_EMPTY	equ	001h
CTRL_DIR	equ	020h

;  LS_DetectEcp - is there an ECR at base+402h?
;
;  The standard probe: the ECR's low bits are read-only status, bits 7:5 are
;  the mode. Write byte mode, read it back, and require the mode bits to stick
;  while the status bits stay sane. A port with no ECR floats or mirrors, and
;  neither survives this.
;
;   Out: LS_HasEcp = 0/1, AL = the ECR value seen
public LS_DetectEcp
LS_DetectEcp:
	push	edx
	push	ebx
	mov	LS_HasEcp, 0
	cmp	LS_ModeReq, 1
	je	lde_done			; MODE=SPP - pinned to nibble, do not probe
	cmp	LS_ModeReq, 2
	jne	lde_probe
	mov	LS_HasEcp, 1			; MODE=ECP - trust the operator over the probe
	jmp	lde_done
lde_probe:
	movzx	edx, LS_BasePort
	add	edx, LPT_ECP_ECR
	in	al, dx
	mov	bl, al				; whatever the port was in
	mov	al, ECR_KEEP_MASK
	out	dx, al
	in	al, dx
	mov	ah, al

;  ** RESTORE BEFORE DECIDING. ** A probe must not change state. Leaving the
;  ECR in byte mode (bits 7:5 = 001) makes the port bidirectional, and the
;  nibble path reads status through it - so a driver that probed and then fell
;  back to SPP found SPP broken by its own probe. Measured 2026-09-13: boot
;  stalled with 12,588 nibble reads and not one command issued.
	mov	al, bl
	and	al, 01Fh			; mode 000 - plain SPP
	out	dx, al

	cmp	ah, 0FFh
	je	lde_done			; floating bus - no ECR here
	mov	al, ah
	and	al, 0E0h
	cmp	al, ECR_KEEP_MASK AND 0E0h
	jne	lde_done
	mov	LS_HasEcp, 1
lde_done:
	pop	ebx
	pop	edx
	ret

;  LS_EcpEnter - put the port in ECP mode, DX left on the ECR.
;   In: BL = 0 to receive (direction reverse), 1 to send
LS_EcpEnter:
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	in	al, dx
	and	al, 01Fh
	or	bl, bl
	jnz	lee_fwd
	or	al, CTRL_DIR			; reverse: the drive sends
lee_fwd:
	out	dx, al
	movzx	edx, LS_BasePort
	add	edx, LPT_ECP_ECR
	mov	al, ECR_MODE_ECP
	out	dx, al
	ret

;  LS_EcpLeave - direction forward, byte mode, port quiescent again.
LS_EcpLeave:
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	in	al, dx
	and	al, 01Fh			; clears the direction bit
	out	dx, al
	movzx	edx, LS_BasePort
	add	edx, LPT_ECP_ECR
;  Back to mode 000 - plain SPP - and NOT byte mode. The task file is read
;  with nibble cycles through the status register, and any ECR mode with bits
;  7:5 non-zero makes the port bidirectional, which changes what those reads
;  return. Leaving byte mode set here would break every register access after
;  the first block transfer.
	in	al, dx
	and	al, 01Fh
	out	dx, al
	ret

;  LS_BlockReadEcp - ECX bytes into ES:EDI, through the ECP FIFO.
;   Out: CF=1 on timeout
LS_BlockReadEcp:
	push	ebx
	push	ecx
	xor	bl, bl
	call	LS_EcpEnter
	pop	ecx
	push	ecx
	movzx	edx, LS_BasePort
	add	edx, LPT_ECPFIFO
	cld
	rep	insb
	call	LS_EcpLeave
	pop	ecx
	pop	ebx
	clc
	ret

;  LS_BlockWriteEcp - ECX bytes from DS:ESI, through the ECP FIFO.
LS_BlockWriteEcp:
	push	ebx
	push	ecx
	mov	bl, 1
	call	LS_EcpEnter
	pop	ecx
	push	ecx
	movzx	edx, LS_BasePort
	add	edx, LPT_ECPFIFO
	cld
	rep	outsb

;  ** Wait for the FIFO to drain before leaving ECP mode. ** Writing the ECR
;  back to a non-FIFO mode RESETS the FIFO, so anything still queued is thrown
;  away - the drive then sits holding DRQ for bytes that were discarded, and
;  the OS reports the failed write as "not enough disk space". The vendor's own
;  ECP handler polls ECR bit 0 for exactly this reason (SD120PPD.SYS 0x494c:
;  mov cx,0xffff / in al,dx / test al,1 / loope).
	push	ecx
	movzx	edx, LS_BasePort
	add	edx, LPT_ECP_ECR
	mov	ecx, 0FFFFh
lbwe_drain:
	in	al, dx
	test	al, ECR_FIFO_EMPTY
	jnz	lbwe_drained
	dec	ecx
	jnz	lbwe_drain
lbwe_drained:
	pop	ecx

	call	LS_EcpLeave
	pop	ecx
	pop	ebx
	clc
	ret

;----------------------------------------------------------------------------
; Transport selector. SPP/nibble is the fallback and works on any parallel
; port; ECP is taken when an ECR answered at bring-up. One decision, made
; once, so the hot path costs a single test.
;----------------------------------------------------------------------------
public LS_BlockRead
LS_BlockRead:
	cmp	LS_HasEcp, 0
	je	LS_BlockReadSpp
	jmp	LS_BlockReadEcp

public LS_BlockWrite
LS_BlockWrite:
	cmp	LS_HasEcp, 0
	je	LS_BlockWriteSpp
	jmp	LS_BlockWriteEcp

;----------------------------------------------------------------------------
LS_BlockReadSpp:
	or	ecx, ecx
	jz	lbr_done
	push	ebx
	pushfd
	cli
	movzx	edx, LS_BasePort
	mov	al, 7
	out	dx, al			; w0(7) - enter block read
	add	edx, LPT_CONTROL
	mov	al, 1
	out	dx, al			; w2(1)
	mov	al, 3
	out	dx, al			; w2(3)
	movzx	edx, LS_BasePort
	mov	al, 0FFh
	out	dx, al			; w0(0FFh)
	xor	bh, bh			; BH = phase
lbr_loop:
	cmp	ecx, 1
	jne	lbr_notlast
	movzx	edx, LS_BasePort
	mov	al, 0FDh
	out	dx, al			; w0(0FDh) announces the LAST byte
lbr_notlast:
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 6
	add	al, bh
	out	dx, al			; w2(6+ph)
	movzx	edx, LS_BasePort
	add	edx, LPT_STATUS
	in	al, dx			; a = r1()
	mov	ah, al
;  Bit 3 set means the bridge gave us both nibbles in one go - take the
;  shortcut rather than clocking a second cycle that will not come.
	test	al, 8
	jnz	lbr_join
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 4
	add	al, bh
	out	dx, al			; w2(4+ph)
	movzx	edx, LS_BasePort
	add	edx, LPT_STATUS
	in	al, dx			; b = r1()
lbr_join:
;  j44(a,b): low nibble from the top of a, high nibble from the top of b.
	and	al, 0F0h
	shr	ah, 4
	or	al, ah
	mov	[edi], al
	inc	edi
	xor	bh, 1
	dec	ecx
	jnz	lbr_loop
	movzx	edx, LS_BasePort
	xor	al, al
	out	dx, al			; w0(0)
	add	edx, LPT_CONTROL
	mov	al, 4
	out	dx, al			; w2(4) - leave block mode
	popfd
	pop	ebx
lbr_done:
	clc
	ret

public LS_BlockWriteSpp
;----------------------------------------------------------------------------
;  NIBBLE block write, from epat_write_block. Same shape: enter block mode
;  once, then a byte and a phase-alternating strobe each iteration.
;
;  Two port accesses per byte instead of the old four, which halves bus
;  occupancy on the write path as a side effect of being correct.
;
;  Interrupts OFF for the duration, and that is not optional: with them live
;  the drive returns sense key 0Bh / ASC 4Ah, COMMAND PHASE ERROR. A 512-byte
;  sector is ~11 ms of port accesses, which is a long time to hold IF clear -
;  chunking the transfer is the way to shorten it, not running unmasked.
;
;   In:   ESI = buffer, ECX = byte count
;----------------------------------------------------------------------------
LS_BlockWriteSpp:
	or	ecx, ecx
	jz	lbw_done
	push	ebx
	pushfd
	cli
	movzx	edx, LS_BasePort
	mov	al, 067h
	out	dx, al			; w0(067h) - enter block write
	add	edx, LPT_CONTROL
	mov	al, 1
	out	dx, al			; w2(1)
	mov	al, 5
	out	dx, al			; w2(5)
	xor	bh, bh
lbw_loop:
	mov	al, [esi]
	inc	esi
	movzx	edx, LS_BasePort
	out	dx, al			; w0(buf[k])
	add	edx, LPT_CONTROL
	mov	al, 4
	add	al, bh
	out	dx, al			; w2(4+ph)
	xor	bh, 1
	dec	ecx
	jnz	lbw_loop
	movzx	edx, LS_BasePort
	add	edx, LPT_CONTROL
	mov	al, 7
	out	dx, al			; w2(7)
	mov	al, 4
	out	dx, al			; w2(4)
	popfd
	pop	ebx
lbw_done:
	clc
	ret

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
;----------------------------------------------------------------------------
; LS_SpinUp - START STOP UNIT with the start bit, then TEST UNIT READY.
;
;   Out:  always CF=0 - an empty drive is entitled to refuse both
;   Uses: EAX, ECX, EDX, ESI, EDI
;
;   Called once, on the first SRB. That is correct behaviour for removable
;   media in its own right, and it doubles as the only evidence channel this
;   machine offers for "is HwStartIo being called at all": the motor is
;   audible, binary, and reported by the hardware rather than by the code
;   under test. Nothing else in init touches the motor now, so any spin-up
;   noise means SRBs are arriving.
;----------------------------------------------------------------------------
public LS_SpinUp
LS_SpinUp:
	lea	esi, LS_StartCdb
	xor	edi, edi
	xor	ecx, ecx
	xor	edx, edx
	call	LS_PacketCommand

	lea	esi, LS_TurCdb
	xor	edi, edi
	xor	ecx, ecx
	xor	edx, edx
	call	LS_PacketCommand
	clc
	ret

;----------------------------------------------------------------------------
; LS_RequestSense - fetch sense data after a device reports CHECK CONDITION.
;
;   In:   EDI = buffer, ECX = its length in bytes
;   Out:  CF=0 if the REQUEST SENSE itself succeeded
;   Uses: everything
;
;   REQUEST SENSE is the one command a device will answer while a contingent
;   allegiance condition is pending, and answering it is what clears the
;   condition. See the LS_SenseCdb comment for the measurement.
;----------------------------------------------------------------------------
;----------------------------------------------------------------------------
; LS_DrainSense - consume the unit-attention conditions a reset leaves behind.
;
;   Out:  always CF=0 - this is best effort
;   Uses: everything
;
;   MEASURED 2026-09-11. After SRST the drive queues MORE THAN ONE condition -
;   ASC 28h "medium may have changed" and ASC 29h "power on / reset occurred"
;   were both observed, reported one per REQUEST SENSE - and a contingent
;   allegiance condition is cleared by READING THE SENSE, not by the next
;   command. So every command until they are drained returns CHECK CONDITION.
;
;   INQUIRY is exempt, which is exactly why bring-up looked healthy: the
;   INQUIRY below succeeds, Init Success is logged, the drive enumerates - and
;   then the first READ the class driver issues is refused, which Windows
;   reports as "media is not formatted".
;
;   Five attempts, matching pf.c's PF_MAX_RETRIES.
;----------------------------------------------------------------------------
public LS_DrainSense
LS_DrainSense:
	push	ebx
	mov	ebx, 5
lds_loop:
	lea	edi, LS_SenseBuf
	mov	ecx, LS_SENSE_LEN
	call	LS_RequestSense
	jc	lds_done
	mov	al, LS_SenseBuf+2
	and	al, 0Fh			; sense key
	jz	lds_done		; nothing pending - done
	dec	ebx
	jnz	lds_loop
lds_done:
	pop	ebx
	clc
	ret

public LS_RequestSense
LS_RequestSense:
	cmp	ecx, LS_SENSE_LEN
	jbe	lrq_len
	mov	ecx, LS_SENSE_LEN
lrq_len:
	or	ecx, ecx
	jz	lrq_none
	mov	byte ptr LS_SenseCdb + 4, cl	; allocation length
	lea	esi, LS_SenseCdb
	xor	edx, edx			; data in
	call	LS_PacketCommand
	ret
lrq_none:
	stc
	ret

;----------------------------------------------------------------------------
; LS_PfWait - Linux pf.c's pf_wait, transliterated.
;
;   In:   BL = "go" mask   - spin while ANY of these bits are SET
;         BH = "stop" mask - spin while ALL of these are CLEAR (0 = ignore)
;   Out:  AL = last status, CF=1 on timeout OR on ERR-within-stop
;   Uses: EAX, ECX, EDX
;
;   pf.c treats ERR appearing inside the stop mask as a FAILURE, not as a
;   reason to stop happily. Our own waits never did that, so an error left
;   the caller believing the phase had completed.
;----------------------------------------------------------------------------
public LS_PfWait
LS_PfWait:
	push	ecx
	push	edx
	mov	ecx, LS_SPIN_BSY
lpw_loop:
	mov	al, ATA_REG_STATUS
	call	LS_RegRead
	mov	LS_LastStatus, eax
	mov	dl, al
	and	dl, bl			; any "go" bit still set?
	jnz	lpw_next
	or	bh, bh
	jz	lpw_ok			; no stop mask - clear is enough
	mov	dl, al
	and	dl, bh
	jnz	lpw_ok			; a "stop" bit has appeared
lpw_next:
	dec	ecx
	jnz	lpw_loop
	pop	edx
	pop	ecx
	stc
	ret
lpw_ok:
	mov	dl, al
	and	dl, bh
	and	dl, ATA_ST_ERR
	jnz	lpw_err
	pop	edx
	pop	ecx
	clc
	ret
lpw_err:
	pop	edx
	pop	ecx
	stc
	ret

;----------------------------------------------------------------------------
; LS_Delay1ms - pf_atapi's mdelay(1), which sits between handing over the
; command packet and reading the reply. We had no delay there at all.
;
;   1 ms at the measured 5.55 us per 8-bit I/O access is ~180 accesses.
;   Port 80h is inside the XT's write-only DMA page block, so a READ returns
;   floating bus and disturbs nothing (technique 75).
;----------------------------------------------------------------------------
LS_Delay1ms:
	push	ecx
	push	edx
	mov	ecx, 180
	mov	edx, 80h
lda_loop:
	in	al, dx
	dec	ecx
	jnz	lda_loop
	pop	edx
	pop	ecx
	ret

public LS_PacketCommand
public LS_ColdStart
public LS_ReadyPoll
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

;  pf_wait(STAT_BUSY | STAT_DRQ, 0) - "before command". Both must be CLEAR.
;  We used to check BSY only, so a stale DRQ from a previous command that was
;  not drained went straight into the next one.
	mov	bl, ATA_ST_BSY OR ATA_ST_DRQ
	xor	bh, bh
	call	LS_PfWait
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

;  4. pf_wait(STAT_BUSY, STAT_DRQ) - the device asks for the packet.
	mov	bl, ATA_ST_BSY
	mov	bh, ATA_ST_DRQ
	call	LS_PfWait
	jc	lpc_fail

;  pf_command: if (read_reg(pf, 2) != 1) -> "command phase error".
;  Interrupt reason must read exactly 1 - C/D set, I/O clear - meaning the
;  device wants a command packet. Handing 12 bytes to a device that is in
;  some other phase is what earns sense key 0Bh / ASC 4Ah, which is exactly
;  what this drive reported on 2026-09-11.
	mov	al, ATA_REG_ICR
	call	LS_RegRead
	jc	lpc_fail
	and	al, 03h
	cmp	al, 1
	jne	lpc_fail

	mov	esi, [ebp-16]
	mov	ecx, ATAPI_PACKET_LEN
;  ** The COMMAND PACKET always goes out in SPP. ** Only the data payload uses
;  ECP. The bridge's command phase is framed by the SPP block-write sequence
;  (w0(67) w2(1) w2(5) ... w2(7)); pushing the CDB through the ECP FIFO instead
;  means the bridge never sees a command at all - measured 2026-09-13, the
;  drive sat at status 48h (DRDY|DRQ) through 5,733 connect/disconnect cycles
;  with not one CDB logged. The vendor does the same: its ECP Read/ECP Write
;  handlers carry data, while the task file and command phase stay nibble.
	call	LS_BlockWriteSpp
	jc	lpc_fail

;  pf_atapi does mdelay(1) here, between the packet and the reply.
	call	LS_Delay1ms

;  5. pf_completion.
;     pf_wait(STAT_BUSY, STAT_DRQ | STAT_READY | STAT_ERR) - "completion".
	mov	bl, ATA_ST_BSY
	mov	bh, ATA_ST_DRQ OR ATA_ST_DRDY OR ATA_ST_ERR
	call	LS_PfWait
	jc	lpc_fail

;     if ((read_reg(2) & 2) && (read_reg(7) & STAT_DRQ))
;     Move data ONLY when the device says it is in a data-in phase and is
;     actually asking. We used to transfer unconditionally, which is how a
;     command that returned nothing still had bytes clocked out of the bridge.
	mov	al, ATA_REG_ICR
	call	LS_RegRead
	jc	lpc_fail
	and	eax, 03h
	mov	LS_Phase, eax

;     The two low bits are C/D and I/O, and they name the phase outright:
;
;         0  data OUT - the device wants the payload FROM us
;         1  command  - it wants another CDB, which here means we are lost
;         2  data IN  - it has the payload FOR us
;         3  status   - the command is finished and moved no data
;
;     This used to be `test al,02h / jz lpc_done`, which required I/O set and
;     so treated phase 0 exactly like phase 3 - "nothing to do". Every write
;     therefore sent its CDB, declined to send the data, and waited for a
;     completion the drive could not give while it held DRQ for 512 bytes
;     that never arrived. Measured in the bed 2026-09-13: 15 CDB blocks, 9
;     read blocks, ZERO blocks carrying write data.
;
;     Imation's own miniport never had this bug because its phase wait takes
;     the expected phase as an argument - rva 0x1e27 waits for `0 or 3` on a
;     write where rva 0x1df6 waits for `2 or 3` on a read.
	cmp	eax, 3
	je	lpc_done		; finished, no data phase at all
	cmp	eax, 1
	je	lpc_fail		; it wants a command packet - out of step

	mov	eax, LS_LastStatus
	test	al, ATA_ST_DRQ
	jz	lpc_done

;     n = ((read_reg(4) + 256 * read_reg(5)) + 3) & 0xfffc
;     The count the DEVICE is offering this phase, rounded up to a multiple
;     of 4 - not the length we asked for. Reading more than it offers leaves
;     the bridge mid-stream and the NEXT command dies in its command phase,
;     which is the sense key 0Bh / ASC 4Ah this drive reported.
	mov	al, ATA_REG_BCLO
	call	LS_RegRead
	jc	lpc_fail
	mov	LS_XferN, eax
	mov	al, ATA_REG_BCHI
	call	LS_RegRead
	jc	lpc_fail
	shl	eax, 8
	add	LS_XferN, eax
	mov	ecx, LS_XferN
	add	ecx, 3
	and	ecx, 0FFFCh
	jz	lpc_done

;     Never overrun the caller's buffer, whatever the device claims.
	mov	eax, [ebp-24]
	cmp	ecx, eax
	jbe	lpc_n_ok
	mov	ecx, eax
lpc_n_ok:
	or	ecx, ecx
	jz	lpc_done
;     Cross-check the caller's direction against the DEVICE's phase before
;     moving a byte. They must agree; if they do not, transferring either way
;     corrupts something, so fail loudly instead.
	cmp	dword ptr [ebp-28], 0
	jne	lpc_want_out
	cmp	LS_Phase, 2
	jne	lpc_fail		; we expect to read, the drive is not sending
	mov	edi, [ebp-20]
	call	LS_BlockRead
	jc	lpc_fail
	jmp	lpc_done
lpc_want_out:
	cmp	LS_Phase, 0
	jne	lpc_fail		; we expect to write, the drive is not asking
	jmp	lpc_out

lpc_out:
	mov	esi, [ebp-20]
	call	LS_BlockWrite
	jc	lpc_fail

;  6. pf_wait(STAT_BUSY, STAT_READY | STAT_ERR) - "data done".
lpc_done:
	mov	bl, ATA_ST_BSY
	mov	bh, ATA_ST_DRDY OR ATA_ST_ERR
	call	LS_PfWait
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


;----------------------------------------------------------------------------
; LS_ColdStart - open the bridge and take the drive out of reset. Nothing more.
;
;   Out:  CF=0 if the drive answered with the ATAPI signature
;   Uses: EAX, ECX, EDX
;
;   This is LS_BringUp with the INQUIRY and the sense drain removed, and it
;   exists because those two do not belong in a bring-up at all:
;
;     - The class driver issues its own INQUIRY. Ours only ever existed to
;       put something in BOOTLOG.TXT, which was the only diagnostic channel
;       available before the 86Box bridge existed. It is not any more.
;     - The sense drain is the unit attention the soft reset itself raises.
;       Draining it here HIDES it from the class driver, which is entitled
;       to see it and will retry. Reporting CHECK CONDITION properly is the
;       correct handling, and that now lives in the state machine.
;
;   What is left is only what the hardware genuinely requires: the drive
;   powers up held in ATA reset and answers nothing until SRST is pulsed.
;   TRANSPORT_SPEC.md section 5.
;----------------------------------------------------------------------------
LS_ColdStart:
	movzx	edx, LS_BasePort
	add	edx, LPT_ECP_ECR
	in	al, dx
	and	al, ECR_KEEP_MASK
	out	dx, al
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
	call	LS_DetectEcp		; sets LS_HasEcp; MODE=SPP pins the nibble path
	mov	al, CPP_PRE1
	call	LS_CppFrame
	mov	al, CPP_PRE2
	call	LS_CppFrame
	mov	al, CPP_PRE3
	call	LS_CppFrame
	mov	al, CPP_PRE4
	call	LS_CppFrame
	mov	al, CPP_CONNECT
	call	LS_CppFrame
	mov	LS_Connected, 1
	call	LS_AtaSoftReset
	jc	lcs_fail
	mov	al, ATA_REG_BCLO
	call	LS_RegRead
	jc	lcs_fail
	cmp	al, ATAPI_SIG_LO
	jne	lcs_fail
	mov	al, ATA_REG_BCHI
	call	LS_RegRead
	jc	lcs_fail
	cmp	al, ATAPI_SIG_HI
	jne	lcs_fail
	call	LS_Disconnect
	clc
	ret
lcs_fail:
	call	LS_Disconnect
	stc
	ret

;----------------------------------------------------------------------------
; LS_ReadyPoll - is the drive willing to take a command RIGHT NOW?
;
;   Out:  CF=0 and ZF=1  drive is idle (BSY and DRQ both clear)
;         CF=0 and ZF=0  still busy - the caller should try again next tick
;         CF=1           the bridge did not answer at all
;   Uses: EAX, ECX, EDX
;
;   ONE status read, then the port is released. This is the whole point of
;   the restructure: a drive that is spinning up is waited for across many
;   1 ms ticks, costing the system nothing, instead of inside a spin loop
;   that holds the CPU for a second at a time. pf.c allows 8 seconds here
;   (PF_SPIN) and so does the caller, as a tick budget.
;----------------------------------------------------------------------------
LS_ReadyPoll:
	cmp	LS_Connected, 0
	jne	lrp_have
	call	LS_Connect
	jc	lrp_fail
lrp_have:
	mov	al, ATA_REG_STATUS
	call	LS_RegRead
	jc	lrp_fail
	mov	LS_LastStatus, eax
	call	LS_Disconnect
	mov	eax, LS_LastStatus
	test	al, ATA_ST_BSY OR ATA_ST_DRQ
	clc
	ret
lrp_fail:
	call	LS_Disconnect
	stc
	ret


	END
