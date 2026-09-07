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
;  LAYER 2 - the EPAT bridge. NOT IMPLEMENTED.
;
;  ====================== READ THIS BEFORE FILLING THESE IN =================
;
;  Shuttle's EPAT is a proprietary parallel-to-ATAPI bridge. Its register
;  access protocol is not published. Two routes were costed on 2026-09-07 and
;  the decision is the owner's - see README.md, "The fastest correct route is
;  a LICENSING decision":
;
;    A. Licence this directory GPL-2.0 and port from Linux's
;       drivers/block/paride/epat.c, which implements this exact chip.
;    B. Recover the protocol from SD120PPD.SYS, which we hold, and keep MIT.
;
;  DO NOT read epat.c and then write these functions "from memory" under MIT.
;  That is the one outcome that is worse than either route, and it is easy to
;  drift into. If route A is taken, say so at the top of this file and carry
;  the GPL notice.
;
;  What the DOS driver tells us for free, and it bounds the job:
;
;    - It negotiates TEN modes: NIBBLE Fast / Normal / Slow / Slow(-),
;      EPP Fast / Normal / BIOS(F) / BIOS(N), ECP Read / ECP Write.
;    - Its switches expose the escape hatches: /di operates in polled mode,
;      /de disables the EPP check, /db disables the EPP BIOS check, /sf skips
;      fast-mode detection, and /rx /wy set read and write timing (x 0-11,
;      y 0-4). A first implementation can therefore target ONE slow, safe
;      mode and ignore the other nine.
;    - The vendor README adds that EPP initialises only for bases 378h and
;      278h, never 3BCh.
;
;  So the minimum viable layer 2 is: connect, read register, write register,
;  read block, write block, disconnect - in nibble mode only. Speed is a
;  later problem and this machine is an 8-bit XT anyway.
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
	mov	LS_Connected, 0
	stc
	ret

;----------------------------------------------------------------------------
; LS_Disconnect - give the port back, leaving it as a plain printer port.
;   Uses: EAX, EDX
;   STUB.
;----------------------------------------------------------------------------
public LS_Disconnect
LS_Disconnect:
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
	xor	eax, eax
	stc
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
	stc
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
	stc
	ret

public LS_BlockWrite
LS_BlockWrite:
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

	add	esp, 16
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	clc
	ret

lpc_fail:
	add	esp, 16
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	stc
	ret


	END
