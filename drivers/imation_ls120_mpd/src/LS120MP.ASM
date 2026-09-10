;============================================================================
; LS120MP.ASM - Windows 95 SCSI miniport for a parallel-port LS-120
;               (Imation SuperDisk behind a Shuttle EPAT bridge).
;
;   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #22).
;
;   PHASE 0 - SKELETON. This does NOT drive the drive. It registers with
;   SCSIPORT, takes its parallel-port base from the Settings tab, reads the
;   status register once, and reports no devices. Nothing else.
;
;   It exists to answer one question, for one boot, before anyone writes a
;   transport:
;
;       Does a polling SCSI miniport that owns the parallel port kill the
;       keyboard on this machine, or was that specific to Imation's binary?
;
;   That matters because the mechanism behind issue #22 is NOT known. Six
;   binary patches against SD120PPD.MPD produced six nulls - the chipset
;   writes at 0x22/0x24/0x94 were all NOPed and the keyboard still died - and
;   on 2026-09-07 its whole AdapterSettings surface was shown to contain no
;   equivalent of the DOS driver's /ni either. What IS proven is only that
;   renaming SD120PPD.MPD out of IOSUBSYS brings the keyboard back.
;
;   So "our driver will be fine because it is cleaner" is a hypothesis. This
;   file is the cheapest possible test of it: if the keyboard survives a boot
;   with this loaded, the premise holds and the transport is worth building.
;   If it dies, the fault is structural to a parallel-port miniport on an XT
;   and the whole plan changes - for one build and one boot, not 2,000 lines.
;
;   Technique 88: bisect by REMOVING, and prove the cheap thing first.
;
;   Structure, calling convention and the SCSIPORT contract are carried from
;   XTIDEMP.ASM, which is shipped and hardware-confirmed (issue #21). Read the
;   commentary there for why each field is set the way it is.
;============================================================================

	.386
	.MODEL	FLAT, STDCALL
	OPTION	CASEMAP:NONE

	INCLUDE	miniport.inc
	INCLUDE	srb.inc

ifdef LS_PHASE2
;----------------------------------------------------------------------------
;  The transport (LS120TR.ASM). Only linked in a -Phase 2 build; the phase-0
;  skeleton neither references nor contains any of it.
;----------------------------------------------------------------------------
EXTERNDEF	LS_PacketCommand	: NEAR
EXTERNDEF	LS_Connect		: NEAR
EXTERNDEF	LS_Disconnect		: NEAR
EXTERNDEF	LS_BasePort		: WORD
EXTERNDEF	LS_LastStatus		: DWORD
LS_CDB_MAX	equ	12
endif

;----------------------------------------------------------------------------
;  SCSIPORT services, decorated exactly as BLOCK/LIB/SCSIPORT.LIB exports them.
;  ScsiPortNotification is varargs and therefore cdecl.
;----------------------------------------------------------------------------
ScsiPortInitialize	PROTO	STDCALL :DWORD, :DWORD, :DWORD, :DWORD
ScsiPortNotification	PROTO	C	:DWORD, :VARARG

;----------------------------------------------------------------------------
;  The parallel port, as three 8-bit registers. Phase 0 READS ONLY.
;
;  A standard LPT decodes three ports from its base:
;     +0  data       (write, or read in bidirectional mode)
;     +1  status     (read only - always safe)
;     +2  control    (read/write)
;  EPP adds +3..+7. We claim eight so the range matches what an EPP-capable
;  transport will eventually need, and so the device node's range is right
;  from the first install rather than changing under the owner later.
;----------------------------------------------------------------------------
LS_REG_DATA		equ	0
LS_REG_STATUS		equ	1
LS_REG_CONTROL		equ	2
LS_PORT_COUNT		equ	8

;  LPT1 on this machine, and the base the DOS driver is given in CONFIG.SYS
;  (`/port:378`). The INF sets it too; this is a floor, not a policy.
ifndef LS_DEFAULT_BASE
LS_DEFAULT_BASE		equ	0378h
endif

;  One adapter, one target. Phase 0 answers nothing, so this only bounds how
;  many times SCSIPORT asks.
LS_MAX_TARGETS		equ	1
LS_MAX_XFER		equ	65536

;----------------------------------------------------------------------------
;  Per-adapter storage. SCSIPORT needs a non-zero size and hands the pointer
;  back to every callback.
;----------------------------------------------------------------------------
DEVEXT	STRUCT
dxBase		DWORD	?		; parallel-port base actually in use
dxStatus	DWORD	?		; status byte read at FindAdapter
dxSrbCount	DWORD	?		; SRBs seen - phase 0 evidence
dxReserved	DWORD	5 DUP (?)
DEVEXT	ENDS

	.DATA

;  The tag looked for in the Settings-tab string. Lower case: the compare
;  lowercases the input, and '=' already has bit 5 set so the OR leaves it be.
LsPortTag	db	'port='

ifdef LS_PHASE2
;  The twelve-byte ATAPI packet handed to the device. SCSIPORT serialises
;  requests for a single adapter, so one static buffer is safe.
LsPacket	db	12 dup (0)
endif

	.CODE

;============================================================================
; DriverEntry - the PE entry point named in the link line.
;============================================================================
	PUBLIC	DriverEntry
DriverEntry PROC STDCALL PUBLIC DriverObject:DWORD, Argument2:DWORD

	LOCAL	hwInit	: HW_INITIALIZATION_DATA
	LOCAL	context	: DWORD

	push	ebx
	push	esi
	push	edi

	lea	edi, hwInit
	mov	ecx, SIZEOF HW_INITIALIZATION_DATA
	xor	eax, eax
	cld
	rep	stosb

	mov	hwInit.HwInitializationDataSize, SIZEOF HW_INITIALIZATION_DATA
	mov	hwInit.AdapterInterfaceType, Isa

	mov	hwInit.HwInitialize, OFFSET LsInitialize
	mov	hwInit.HwStartIo, OFFSET LsStartIo
	mov	hwInit.HwFindAdapter, OFFSET LsFindAdapter
	mov	hwInit.HwResetBus, OFFSET LsResetBus

;  HwInterrupt and HwDmaStarted stay NULL. This is the single most important
;  line in the file for issue #22.
;
;  A parallel-port drive can be driven by IRQ 7 or by polling; the DOS build
;  offers both (`/IRQ:7` and `/di - Operate in polled mode`). We poll, and we
;  declare no handler at all, so SCSIPORT is never asked to connect an
;  interrupt. A NULL handler with a zero BusInterruptLevel says "we poll"
;  without asking for anything - whereas a non-NULL handler with a zero level
;  asks for IRQ 0, the system timer.
;
;  XTIDEMP.MPD makes the same choice and runs on this machine without
;  disturbing the keyboard, which is the closest thing to evidence we have
;  that the shape is safe.

	mov	hwInit.DeviceExtensionSize, SIZEOF DEVEXT
	mov	hwInit.SpecificLuExtensionSize, 0
	mov	hwInit.SrbExtensionSize, 0
	mov	hwInit.NumberOfAccessRanges, 1

;  Let SCSIPORT do scatter/gather decomposition and hand us one flat mapped
;  buffer. Misreading an S/G list by hand is what corrupted a volume on
;  2026-09-01 (technique 85); that bug class stays somebody else's.
	mov	hwInit.MapBuffers, TRUE
	mov	hwInit.NeedPhysicalAddresses, FALSE
	mov	hwInit.TaggedQueuing, FALSE
	mov	hwInit.AutoRequestSense, FALSE
	mov	hwInit.MultipleRequestPerLu, FALSE
	mov	hwInit.ReceiveEvent, FALSE

	mov	context, 0

	lea	eax, context
	push	eax
	lea	eax, hwInit
	push	eax
	push	Argument2
	push	DriverObject
	call	ScsiPortInitialize

	pop	edi
	pop	esi
	pop	ebx
	ret

DriverEntry ENDP

;============================================================================
; LsFindAdapter - work out which parallel port we own, and claim nothing else.
;
;   PHASE 0 IS READ-ONLY. The only hardware access in this whole file is one
;   IN from the status register, which has no side effect on any parallel port
;   and cannot mask an interrupt.
;
;   Deliberately absent, and this is the point of the exercise:
;     - no chipset probe. SD120PPD writes 0x22/0x23, 0x24/0x25 and 0x94 to
;       configure 386SL and VLSI super-I/O parts (its DOS build calls these
;       /fe and /fev). On an XT the 8259 answers across 0x20-0x3F, so those
;       land on the interrupt controller (technique 75).
;     - no EPP/ECP negotiation, no mode detection, no bidirectional test.
;     - no write of any kind.
;============================================================================
LsFindAdapter PROC STDCALL DeviceExtension:DWORD, Context:DWORD,
			BusInformation:DWORD, ArgumentString:DWORD,
			ConfigInfo:DWORD, Again:DWORD

	push	ebx
	push	esi
	push	edi

	mov	esi, ConfigInfo

;  Only ever one adapter.
	mov	eax, Again
	mov	byte ptr [eax], FALSE

ifdef LS_FORCE_BASE
	mov	eax, LS_FORCE_BASE
	jmp	lfa_havebase
endif

;  1. What the owner typed on the Settings tab wins. This is the convention
;     Imation's own README documents for exactly this situation, and the
;     person installing it already knows which LPT the drive is on.
	mov	esi, ArgumentString
	call	LsParseBase			; eats ESI
	mov	esi, ConfigInfo
	or	eax, eax
	jnz	lfa_havebase

;  2. Otherwise whatever CONFIGMG assigned the node.
	mov	edi, [esi].PORT_CONFIGURATION_INFORMATION.AccessRanges
	or	edi, edi
	jz	lfa_default
	cmp	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfAccessRanges, 0
	je	lfa_default
	mov	eax, [edi].ACCESS_RANGE.RangeStart.LowPart
	or	eax, eax
	jnz	lfa_havebase

lfa_default:
	mov	eax, LS_DEFAULT_BASE

lfa_havebase:
	mov	ebx, DeviceExtension
	mov	[ebx].DEVEXT.dxBase, eax
	mov	[ebx].DEVEXT.dxSrbCount, 0

ifdef LS_PHASE2
;  The transport addresses the port directly, so it needs the same base.
	mov	LS_BasePort, ax
endif

;  The one and only hardware access in phase 0: read the status register.
;  Recorded for later diagnosis; nothing is decided on it, because a phase-0
;  build that declines to load would test nothing.
	mov	edx, eax
	add	edx, LS_REG_STATUS
	xor	eax, eax
	in	al, dx
	mov	[ebx].DEVEXT.dxStatus, eax

;  Describe the adapter. Values follow XTIDEMP.MPD, which is confirmed on this
;  machine.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfBuses, 1
	mov	[esi].PORT_CONFIGURATION_INFORMATION.MaximumTransferLength, LS_MAX_XFER
	mov	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfPhysicalBreaks, 0
	mov	[esi].PORT_CONFIGURATION_INFORMATION.MaximumNumberOfTargets, LS_MAX_TARGETS
	mov	[esi].PORT_CONFIGURATION_INFORMATION.AlignmentMask, 0
	mov	[esi].PORT_CONFIGURATION_INFORMATION.InitiatorBusId, 7

;  No interrupt, no DMA, no bus mastering. See the note in DriverEntry.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.BusInterruptLevel, 0
	mov	[esi].PORT_CONFIGURATION_INFORMATION.BusInterruptVector, 0
	mov	[esi].PORT_CONFIGURATION_INFORMATION.DmaChannel, 0FFFFFFFFh
	mov	[esi].PORT_CONFIGURATION_INFORMATION.Master, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.ScatterGather, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.Dma32BitAddresses, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.DemandMode, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.CachesData, FALSE

	mov	[esi].PORT_CONFIGURATION_INFORMATION.MapBuffers, TRUE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.NeedPhysicalAddresses, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.TaggedQueuing, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.AutoRequestSense, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.MultipleRequestPerLu, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.RealModeInitialized, FALSE
	mov	[esi].PORT_CONFIGURATION_INFORMATION.BufferAccessScsiPortControlled, FALSE

	pop	edi
	pop	esi
	pop	ebx
	mov	eax, SP_RETURN_FOUND
	ret

LsFindAdapter ENDP

;============================================================================
; LsParseBase - read `PORT=0x378` out of the Settings-tab string.
;
;   In:   ESI -> ASCIIZ ArgumentString, or NULL
;   Out:  EAX = base address, or 0 if the string carries none
;   Uses: EAX, ECX, EDX, ESI  (EBX, EDI preserved)
;
;   Carried unchanged from XTIDEMP.ASM. Case-insensitive, `0x` optional, up to
;   four hex digits; anything unparseable returns 0 so a typo falls through to
;   the device node rather than pointing the driver at nothing.
;============================================================================
LsParseBase PROC

	push	ebx
	push	edi
	xor	eax, eax
	or	esi, esi
	jz	lpb_out

lpb_next:
	cmp	byte ptr [esi], 0
	je	lpb_out
	mov	edi, esi
	mov	edx, OFFSET LsPortTag
	mov	ecx, 5
lpb_cmp:
	mov	bl, [edi]
	or	bl, 20h
	cmp	bl, [edx]
	jne	lpb_advance
	inc	edi
	inc	edx
	dec	ecx
	jnz	lpb_cmp
	jmp	lpb_num
lpb_advance:
	inc	esi
	jmp	lpb_next

lpb_num:
	mov	esi, edi
	cmp	byte ptr [esi], '0'
	jne	lpb_hex
	mov	bl, [esi+1]
	or	bl, 20h
	cmp	bl, 'x'
	jne	lpb_hex
	add	esi, 2

lpb_hex:
	xor	eax, eax
	xor	ecx, ecx
lpb_digit:
	mov	bl, [esi]
	cmp	bl, '0'
	jb	lpb_end
	cmp	bl, '9'
	ja	lpb_alpha
	sub	bl, '0'
	jmp	lpb_acc
lpb_alpha:
	or	bl, 20h
	cmp	bl, 'a'
	jb	lpb_end
	cmp	bl, 'f'
	ja	lpb_end
	sub	bl, 'a' - 10
lpb_acc:
	shl	eax, 4
	movzx	edx, bl
	or	eax, edx
	inc	esi
	inc	ecx
	cmp	ecx, 4
	jb	lpb_digit

lpb_end:
	or	ecx, ecx
	jnz	lpb_out
	xor	eax, eax

lpb_out:
	pop	edi
	pop	ebx
	ret

LsParseBase ENDP

;============================================================================
; LsInitialize - nothing to initialise in phase 0, and that is the point.
;============================================================================
LsInitialize PROC STDCALL DeviceExtension:DWORD

ifdef LS_PHASE2
;  Prove the bridge is really there, once, at init - rather than letting the
;  first SRB discover it. LS_Connect reads ATA status and rejects 00h and
;  FFh, which are exactly what an unconnected or absent bridge returns and
;  what every probe saw all day on 2026-09-08 while the connect itself was
;  correct. Release it again immediately; the port is shared.
	call	LS_Connect
	jc	lsi_nobridge
	call	LS_Disconnect
	mov	eax, TRUE
	ret
lsi_nobridge:
;  Say so honestly. Returning TRUE here would leave SCSIPORT scanning a bus
;  that cannot answer, which looks like a drive fault rather than a missing
;  bridge.
	mov	eax, FALSE
	ret
else
	mov	eax, TRUE
	ret
endif

LsInitialize ENDP

;============================================================================
; LsResetBus - nothing to reset. Returning FALSE would tell SCSIPORT the
;              adapter is broken; nothing is wrong, so say so.
;============================================================================
LsResetBus PROC STDCALL DeviceExtension:DWORD, PathId:DWORD

	mov	eax, TRUE
	ret

LsResetBus ENDP

;============================================================================
; LsStartIo - phase 0 answers every request with a selection timeout.
;
;   SCSIPORT will scan the bus, be told there is nothing on it, and stop. The
;   adapter stays loaded and resident, which is what the keyboard test needs -
;   a driver that declined to load would prove nothing about a driver that
;   runs.
;
;   No hardware is touched here at all.
;============================================================================
LsStartIo PROC STDCALL DeviceExtension:DWORD, Srb:DWORD

	push	ebx
	push	esi
	push	edi

	mov	ebx, Srb

;  Count what we were asked, so a later phase can read back evidence that the
;  dispatch path really ran rather than inferring it.
	mov	esi, DeviceExtension
	inc	[esi].DEVEXT.dxSrbCount

	movzx	eax, [ebx].SCSI_REQUEST_BLOCK.Function
	cmp	eax, SRB_FUNCTION_EXECUTE_SCSI
	jne	lsi_badfunc

ifndef LS_PHASE2
;  PHASE 0: nothing is attached, as far as this build is concerned.
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lsi_complete
else
;----------------------------------------------------------------------------
;  PHASE 2: SRB -> ATAPI, and it is very nearly a pass-through.
;
;  An ATAPI command packet IS a SCSI CDB. SCSIPORT hands us a CDB in the SRB
;  and, for a command of twelve bytes or fewer, it goes on the wire unchanged
;  - zero-padded if the class driver sent a 6- or 10-byte form. So this layer
;  does no command translation at all, which is the whole reason an ATAPI
;  device is worth presenting as SCSI.
;
;  *** UNTESTED. The transport beneath this is stubbed. ***
;----------------------------------------------------------------------------
	cmp	[ebx].SCSI_REQUEST_BLOCK.Lun, 0
	jne	lsi_noselect
	cmp	[ebx].SCSI_REQUEST_BLOCK.TargetId, 0
	jne	lsi_noselect

;  Copy the CDB into a zero-filled twelve-byte packet.
	lea	edi, LsPacket
	mov	ecx, LS_CDB_MAX
	xor	eax, eax
	cld
	rep	stosb

	movzx	ecx, [ebx].SCSI_REQUEST_BLOCK.CdbLength
	cmp	ecx, LS_CDB_MAX
	ja	lsi_badreq
	or	ecx, ecx
	jz	lsi_badreq
	lea	esi, [ebx].SCSI_REQUEST_BLOCK.Cdb
	lea	edi, LsPacket
	rep	movsb

;  Direction, from the SRB flags. UNSPECIFIED is both bits set and is treated
;  as data-in, which is the safe reading for an inquiry-shaped command.
	xor	edx, edx
	mov	eax, [ebx].SCSI_REQUEST_BLOCK.SrbFlags
	test	eax, SRB_FLAGS_DATA_OUT
	jz	lsi_dirin
	test	eax, SRB_FLAGS_DATA_IN
	jnz	lsi_dirin
	mov	edx, 1
lsi_dirin:

	lea	esi, LsPacket
	mov	edi, [ebx].SCSI_REQUEST_BLOCK.DataBuffer
	mov	ecx, [ebx].SCSI_REQUEST_BLOCK.DataTransferLength
	call	LS_PacketCommand
	jc	lsi_ioerror

	mov	al, SRB_STATUS_SUCCESS
	jmp	lsi_complete
endif

ifdef LS_PHASE2
lsi_ioerror:
	mov	al, SRB_STATUS_ERROR
	jmp	lsi_complete
lsi_badreq:
	mov	al, SRB_STATUS_INVALID_REQUEST
	jmp	lsi_complete
lsi_noselect:
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lsi_complete
endif

lsi_badfunc:
	mov	al, SRB_STATUS_BAD_FUNCTION

lsi_complete:
	mov	[ebx].SCSI_REQUEST_BLOCK.SrbStatus, al

;  ScsiPortNotification is cdecl and varargs, so we clean the stack.
	push	ebx
	push	DeviceExtension
	push	RequestComplete
	call	ScsiPortNotification
	add	esp, 12

	push	DeviceExtension
	push	NextRequest
	call	ScsiPortNotification
	add	esp, 8

	pop	edi
	pop	esi
	pop	ebx
	mov	eax, TRUE
	ret

LsStartIo ENDP

	END
