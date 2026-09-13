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
EXTERNDEF	LS_RequestSense		: NEAR
EXTERNDEF	LS_Connect		: NEAR
EXTERNDEF	LS_BringUp		: NEAR
EXTERNDEF	LS_ColdStart		: NEAR
EXTERNDEF	LS_ReadyPoll		: NEAR
EXTERNDEF	LS_SpinUp		: NEAR
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
LS_ATA_ST_ERR		equ	001h	; status bit 0 - device reports CHECK CONDITION
LS_ATA_ST_BSY		equ	080h	; status bit 7 - the command is still running
;  SCSI.INC is not included here (srb.inc + miniport.inc only), and pulling a
;  whole header in for one value is not worth the collision risk.
LS_SCSISTAT_CHECK	equ	002h	; SCSISTAT_CHECK_CONDITION
;  Timer interval in microseconds. Imation's miniport uses 1000 (1 ms) and so
;  does the DDK's PC2X sample; there is no reason to differ.
LS_TIMER_US		equ	1000
LS_MAX_TARGETS		equ	2	; XTIDEMP.MPD uses 2 and enumerates correctly
					; on this machine; 1 was the only config value
					; that differed from it.
;  MaximumTransferLength. NOT 65536: the ATAPI byte-count registers are 16
;  bits, so a 65536-byte request programmes a byte count of ZERO and the
;  drive is told to transfer nothing (IMPLEMENTATION.md section 7).
LS_MAX_XFER		equ	65024		; 0xFE00 - 127 sectors, and it fits

;----------------------------------------------------------------------------
;  The request state machine, which runs in LsTimer.
;
;  Every multi-second wait this driver needs is a COUNT OF TICKS here, not a
;  spin loop. That is the whole restructure: the OS expects a miniport to
;  return promptly and be called back, and every timeout tuned before this
;  was a constant inside a shape it does not expect.
;----------------------------------------------------------------------------
LS_ST_IDLE		equ	0
LS_ST_COLDSTART		equ	1	; open the bridge, pulse SRST
LS_ST_READY		equ	2	; poll until the drive is not busy
LS_ST_ISSUE		equ	3	; hand over the packet
LS_ST_SENSE		equ	4	; fetch sense after a CHECK CONDITION
LS_ST_DONE		equ	5	; complete on the next tick
LS_ST_FINISH		equ	6	; issued, data moved, drive still BSY - wait it out

;  Tick budgets, in 1 ms timer ticks.
;  pf.c allows PF_SPIN = 8 s for a drive to become ready, and an LS-120
;  genuinely takes seconds to spin up. Waiting that long is fine HERE - it
;  costs 8000 timer callbacks and no blocked CPU - and was fatal in a spin
;  loop.
LS_TICKS_READY		equ	8000	; 8 s, pf.c's PF_SPIN
LS_TICKS_COLD		equ	3000	; 3 s to open a bridge that is present
LS_MAX_RETRY		equ	5	; pf.c's PF_MAX_RETRIES

;----------------------------------------------------------------------------
;  Per-adapter storage. SCSIPORT needs a non-zero size and hands the pointer
;  back to every callback.
;----------------------------------------------------------------------------
DEVEXT	STRUCT
dxBase		DWORD	?		; parallel-port base actually in use
dxStatus	DWORD	?		; status byte read at FindAdapter
dxSrbCount	DWORD	?		; SRBs seen - phase 0 evidence
dxPendSrb	DWORD	?		; SRB awaiting deferred completion, 0 = idle
dxPendStat	DWORD	?		; the SrbStatus it will be completed with
dxTicks		DWORD	?		; timer ticks this request has taken
dxState		DWORD	?		; LS_ST_* - where the current request is
dxRetry		DWORD	?		; attempts made at the current command
dxBuf		DWORD	?		; caller's data buffer
dxLen		DWORD	?		; caller's transfer length
dxDir		DWORD	?		; 0 = data in, 1 = data out
dxColdDone	DWORD	?		; the bridge has been opened once
dxReserved	DWORD	2 DUP (?)
DEVEXT	ENDS

	.DATA

;  The tag looked for in the Settings-tab string. Lower case: the compare
;  lowercases the input, and '=' already has bit 5 set so the OR leaves it be.
	extrn	LS_ModeReq:DWORD	; the transport's mode selector

LsPortTag	db	'port='
LsModeTag	db	'mode='
LsSppTag	db	'spp'
LsEcpTag	db	'ecp'

ifdef LS_PHASE2
;  The twelve-byte ATAPI packet handed to the device. SCSIPORT serialises
;  requests for a single adapter, so one static buffer is safe.
LsPacket	db	12 dup (0)

ifdef LS_FAKE_DEVICE
;----------------------------------------------------------------------------
;  FAKE DEVICE - for the 86Box bed only, never for hardware.
;
;  86Box models no EPAT bridge, so a real build fails LS_BringUp there and
;  never reaches enumeration. That does not matter for the question this bed
;  exists to answer, which is entirely SCSIPORT-side: given a device that
;  definitely answers, does Windows enumerate it and give it a letter?
;
;  If it does, our PORT_CONFIGURATION_INFORMATION and SRB handling are right
;  and the remaining fault is on the hardware side. If it does not, the fault
;  is here, and it can be chased offline instead of one boot at a time.
;
;  Values are the real drive's, copied from SYSTEM.DAT on the owner's machine.
;----------------------------------------------------------------------------
LsFakeInq	db	000h		; peripheral device type: direct access
		db	080h		; RMB - removable
		db	002h, 002h	; SCSI-2, response format 2
		db	01Fh		; additional length
		db	0, 0, 0
		db	'MATSHITA'
		db	'LS-120 COSM   04'
		db	'0270'
LsFakeCap	db	0, 3, 96h, 0FFh	; last LBA, big-endian
		db	0, 0, 2, 0	; 512-byte blocks
endif

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
	push	eax
	mov	esi, ArgumentString
	call	LsParseMode			; MODE=SPP / MODE=ECP, 0 = auto
	mov	LS_ModeReq, eax
	pop	eax
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
;  Imation's own SD120PPD.MPD sets this to 1, not 0. Read out of its
;  HwFindAdapter at 0x3c9f - see TRANSPORT_SPEC.md. It is the only miniport
;  known to present THIS drive to Win95 SCSIPORT successfully, so where it
;  disagrees with us about how to describe the adapter, it wins.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfPhysicalBreaks, 1
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
;  TRUE, following SD120PPD.MPD (its ConfigInfo+58h). This asks SCSIPORT to
;  hand us a mapped, locked data buffer rather than leaving us to reach the
;  caller's. XTIDEMP.MPD gets away with FALSE because it is a directly
;  addressable disk; this transport copies every byte itself, through a
;  parallel-port bridge, which is the case the flag exists for.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.BufferAccessScsiPortControlled, TRUE

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
; LsParseMode - read `MODE=SPP` or `MODE=ECP` out of the Settings-tab string.
;
;   In:   ESI -> ASCIIZ ArgumentString, or NULL
;   Out:  EAX = 0 auto-detect (nothing said), 1 = force SPP, 2 = force ECP
;   Uses: EAX, EBX, ECX, EDX, ESI, EDI
;
;   SPP/nibble works on every parallel port and is what the driver falls back
;   to; ECP is faster and is what the vendor's own driver selects on this
;   machine. Auto-detection probes for an ECR, but a probe can be wrong in
;   either direction on hardware nobody here has seen, so BOTH answers are
;   forceable - a user with a port that lies about its ECR can pin SPP, and a
;   user whose ECR does not answer the probe can pin ECP.
;
;   Same shape as LsParseBase deliberately: case-insensitive, scans for the
;   tag anywhere in the string, and anything unrecognised returns 0 so a typo
;   auto-detects rather than selecting something the caller did not ask for.
;============================================================================
LsParseMode PROC

	push	ebx
	push	edi
	xor	eax, eax
	or	esi, esi
	jz	lpm_out

lpm_next:
	cmp	byte ptr [esi], 0
	je	lpm_out
	mov	edi, esi
	mov	edx, OFFSET LsModeTag
	mov	ecx, 5
lpm_cmp:
	mov	bl, [edi]
	or	bl, 20h
	cmp	bl, [edx]
	jne	lpm_advance
	inc	edi
	inc	edx
	dec	ecx
	jnz	lpm_cmp
	jmp	lpm_value
lpm_advance:
	inc	esi
	jmp	lpm_next

;  EDI now points at the value. Three characters, case-insensitive.
lpm_value:
	mov	esi, edi
	mov	edx, OFFSET LsSppTag
	mov	ecx, 3
	call	lpm_match
	jc	lpm_tryecp
	mov	eax, 1			; SPP
	jmp	lpm_out
lpm_tryecp:
	mov	edx, OFFSET LsEcpTag
	mov	ecx, 3
	call	lpm_match
	jc	lpm_out			; unrecognised - leave EAX 0, auto-detect
	mov	eax, 2			; ECP
	jmp	lpm_out

;  lpm_match: ESI vs EDX for ECX chars, case-insensitive. CF=0 on a match.
lpm_match:
	push	esi
	push	edx
	push	ecx
lpm_mloop:
	mov	bl, [esi]
	or	bl, 20h
	cmp	bl, [edx]
	jne	lpm_mno
	inc	esi
	inc	edx
	dec	ecx
	jnz	lpm_mloop
	pop	ecx
	pop	edx
	pop	esi
	clc
	ret
lpm_mno:
	pop	ecx
	pop	edx
	pop	esi
	stc
	ret

lpm_out:
	pop	edi
	pop	ebx
	ret

LsParseMode ENDP


;============================================================================
; LsInitialize - returns TRUE, and touches nothing.
;
;   RESTRUCTURED 2026-09-11. This used to call LS_BringUp: open the bridge,
;   pulse SRST, issue an INQUIRY and drain the sense - seconds of device I/O
;   inside the call the OS makes while the boot waits on it. That is what
;   stalled the boot, and every attempt to fix it by allowing MORE time made
;   it worse, because the time was being spent somewhere the OS does not
;   expect a driver to spend any.
;
;   Two independent implementations agree on this shape. Imation's own
;   HwInitialize (SD120PPD.MPD rva 0x3277) is twenty instructions with no
;   device I/O and returns TRUE unconditionally; the DDK's PC2X.C sample is
;   the same. IMPLEMENTATION.md section 1.1.
;
;   The bring-up still has to happen - the drive powers up held in ATA reset
;   and answers nothing until SRST is pulsed - so it is now the first state of
;   the request state machine, where it costs ticks instead of a blocked boot.
;============================================================================
LsInitialize PROC STDCALL DeviceExtension:DWORD

	mov	eax, DeviceExtension
	mov	[eax].DEVEXT.dxState, LS_ST_IDLE
	mov	[eax].DEVEXT.dxPendSrb, 0
	mov	[eax].DEVEXT.dxColdDone, 0
	mov	eax, TRUE
	ret

LsInitialize ENDP

;============================================================================
; LsTimer - the request state machine. All the waiting lives here.
;
;   Registered per-request by ScsiPortNotification(RequestTimerCall, ...), so
;   there is nothing to declare in HW_INITIALIZATION_DATA. It re-arms itself
;   while work is outstanding and completes the SRB only when the device is
;   actually done.
;
;   This is the shape both references use. Imation's miniport re-arms at rva
;   0x48bb by pushing ITS OWN linked address back into ScsiPortNotification;
;   PC2X.C does the same in PC2xTimer.
;
;   Each tick does ONE bounded step. Nothing here waits for a drive to spin
;   up, or for a bridge to appear - it asks once, and if the answer is "not
;   yet" it re-arms and asks again in a millisecond. A drive that needs eight
;   seconds gets eight thousand ticks and blocks nothing.
;============================================================================
LsTimer PROC STDCALL DeviceExtension:DWORD

	push	ebx
	push	esi
	push	edi

	mov	esi, DeviceExtension
	mov	ebx, [esi].DEVEXT.dxPendSrb
	or	ebx, ebx
	jz	lt_idle			; nothing outstanding - do not re-arm

	inc	[esi].DEVEXT.dxTicks

ifdef LS_PHASE2
	mov	eax, [esi].DEVEXT.dxState

	cmp	eax, LS_ST_COLDSTART
	je	lt_coldstart
	cmp	eax, LS_ST_READY
	je	lt_ready
	cmp	eax, LS_ST_ISSUE
	je	lt_issue
	cmp	eax, LS_ST_SENSE
	je	lt_sense
	cmp	eax, LS_ST_FINISH
	je	lt_finishwait
;  LS_ST_DONE, or any state we do not recognise: complete with what we have.
	jmp	lt_finish

;----------------------------------------------------------------------------
;  COLDSTART - open the bridge and take the drive out of ATA reset.
;
;  Retried every tick until it answers or the budget runs out, because a
;  bridge that is powered but not yet settled is a normal thing to meet on a
;  cold boot and is not a reason to fail the request.
;----------------------------------------------------------------------------
lt_coldstart:
	call	LS_ColdStart
	jnc	lt_cold_ok
	mov	eax, [esi].DEVEXT.dxTicks
	cmp	eax, LS_TICKS_COLD
	jb	lt_rearm		; not yet - ask again next tick
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lt_setstat
lt_cold_ok:
	mov	[esi].DEVEXT.dxColdDone, 1
	mov	[esi].DEVEXT.dxState, LS_ST_READY
	mov	[esi].DEVEXT.dxTicks, 0
	jmp	lt_rearm

;----------------------------------------------------------------------------
;  READY - one status read per tick until the drive is idle.
;
;  This is where a spin-up is waited for. pf.c allows 8 s (PF_SPIN) and so do
;  we; the difference is that those 8 s are 8000 callbacks rather than a
;  blocked CPU. IMPLEMENTATION.md section 4 - the spin constants were never
;  the bug, their location was.
;----------------------------------------------------------------------------
lt_ready:
	call	LS_ReadyPoll
	jc	lt_ready_dead
	jz	lt_ready_ok		; BSY and DRQ both clear
	mov	eax, [esi].DEVEXT.dxTicks
	cmp	eax, LS_TICKS_READY
	jb	lt_rearm
	mov	al, SRB_STATUS_TIMEOUT
	jmp	lt_setstat
lt_ready_dead:
	mov	eax, [esi].DEVEXT.dxTicks
	cmp	eax, LS_TICKS_READY
	jb	lt_rearm
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lt_setstat
lt_ready_ok:
	mov	[esi].DEVEXT.dxState, LS_ST_ISSUE
	jmp	lt_rearm

;----------------------------------------------------------------------------
;  ISSUE - hand the packet over.
;
;  The drive was idle as of the previous tick, so the waits inside
;  LS_PacketCommand only have to cover the latency of a command already
;  accepted. That is why LS_SPIN_BSY could be cut to ~90 ms.
;----------------------------------------------------------------------------
lt_issue:
;  LS_PacketCommand takes ESI = packet, EDI = buffer, ECX = length,
;  EDX = direction. Read the extension fields BEFORE ESI is repointed.
	mov	edi, [esi].DEVEXT.dxBuf
	mov	ecx, [esi].DEVEXT.dxLen
	mov	edx, [esi].DEVEXT.dxDir
	lea	esi, LsPacket
	call	LS_PacketCommand
	mov	esi, DeviceExtension
	jc	lt_issue_err

	mov	al, SRB_STATUS_SUCCESS
	jmp	lt_setstat

lt_issue_err:
;  ERR in the status means the device is reporting CHECK CONDITION, and the
;  class driver cannot act on that without the sense data. Most often it is
;  UNIT ATTENTION after a media change or a reset, which it clears by reading
;  the sense and retrying. Reporting a bare SRB_STATUS_ERROR with ScsiStatus
;  left at 0 says "the command failed but the device says GOOD", which is
;  incoherent and gets treated as unreadable media - "media is not formatted".
;  Measured on the real drive: error register 64h. IMPLEMENTATION.md section 3.
	mov	eax, LS_LastStatus
	test	al, LS_ATA_ST_ERR
	jnz	lt_issue_check

;  No ERR, but the drive is still BSY: the command is simply STILL RUNNING.
;  Re-issuing it here restarts a command already in flight, which is what made
;  every media command fail after LS_MAX_RETRY attempts - measured in the bed
;  2026-09-13, twenty READ CAPACITYs each abandoned after exactly LS_SPIN_BSY
;  polls. Wait it out in the state machine instead, on pf.c's full PF_SPIN
;  budget. Only a drive that is NOT busy justifies retrying the command.
	test	al, LS_ATA_ST_BSY
	jz	lt_issue_retry
	mov	[esi].DEVEXT.dxState, LS_ST_FINISH
	mov	[esi].DEVEXT.dxTicks, 0
	jmp	lt_rearm

lt_issue_check:

	mov	[ebx].SCSI_REQUEST_BLOCK.ScsiStatus, LS_SCSISTAT_CHECK

	mov	eax, [ebx].SCSI_REQUEST_BLOCK.SrbFlags
	test	eax, SRB_FLAGS_DISABLE_AUTOSENSE
	jnz	lt_plainerr
	mov	eax, [ebx].SCSI_REQUEST_BLOCK.SenseInfoBuffer
	or	eax, eax
	jz	lt_plainerr
	movzx	eax, [ebx].SCSI_REQUEST_BLOCK.SenseInfoBufferLength
	or	eax, eax
	jz	lt_plainerr

	mov	[esi].DEVEXT.dxState, LS_ST_SENSE
	mov	[esi].DEVEXT.dxTicks, 0
	jmp	lt_rearm

lt_issue_retry:
;  No ERR bit: the transport gave up rather than the device refusing. Try the
;  whole command again, up to pf.c's PF_MAX_RETRIES, before calling it a day.
	inc	[esi].DEVEXT.dxRetry
	mov	eax, [esi].DEVEXT.dxRetry
	cmp	eax, LS_MAX_RETRY
	jae	lt_plainerr
	mov	[esi].DEVEXT.dxState, LS_ST_READY
	mov	[esi].DEVEXT.dxTicks, 0
	jmp	lt_rearm

lt_plainerr:
	mov	al, SRB_STATUS_ERROR
	jmp	lt_setstat

;----------------------------------------------------------------------------
;  FINISH - the packet was issued and its data phase completed, but the drive
;  is still BSY. One status read per tick until it settles.
;
;  Budget is LS_TICKS_READY - 8 s, pf.c's PF_SPIN. Three independent sources
;  agree on seconds here and we were allowing ~60 ms: Linux pf.c computes
;  PF_SPIN = 160,000 x udelay(50) = 8 s; Imation's own SD120PPD.MPD spins
;  500,000 x ScsiPortStallExecution(10) = 5 s for BSY and 1,000,000 = 10 s for
;  BSY-or-DRQ; and this driver's own LS_TICKS_READY already says 8 s.
;  docs/ls120_constant_sweep_2026_09_13.md.
;----------------------------------------------------------------------------
lt_finishwait:
	call	LS_ReadyPoll
	jc	lt_fin_dead
	jz	lt_fin_done
	mov	eax, [esi].DEVEXT.dxTicks
	cmp	eax, LS_TICKS_READY
	jb	lt_rearm
	mov	al, SRB_STATUS_TIMEOUT
	jmp	lt_setstat
lt_fin_dead:
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lt_setstat
lt_fin_done:
;  Settled. ERR here is a real CHECK CONDITION and takes the sense path.
	mov	eax, LS_LastStatus
	test	al, LS_ATA_ST_ERR
	jnz	lt_issue_check
	mov	al, SRB_STATUS_SUCCESS
	jmp	lt_setstat

;----------------------------------------------------------------------------
;  SENSE - fetch the sense data into the SRB's own buffer.
;----------------------------------------------------------------------------
lt_sense:
;  LS_RequestSense takes EDI = buffer, ECX = length - the SRB's own sense
;  buffer, so the class driver reads it where it expects to.
	mov	edi, [ebx].SCSI_REQUEST_BLOCK.SenseInfoBuffer
	movzx	ecx, [ebx].SCSI_REQUEST_BLOCK.SenseInfoBufferLength
	push	ebx
	call	LS_RequestSense
	pop	ebx
	mov	esi, DeviceExtension
	jc	lt_plainerr
	mov	al, SRB_STATUS_ERROR OR SRB_STATUS_AUTOSENSE_VALID
	jmp	lt_setstat
endif

;  Phase 0 has no state machine: the verdict was decided in LsStartIo and is
;  already in dxPendStat, so go straight to completing it.
	jmp	lt_finish

lt_setstat:
	movzx	eax, al
	mov	[esi].DEVEXT.dxPendStat, eax

lt_finish:
;  Complete, and stop. Do NOT re-arm - there is nothing left to do and a timer
;  that keeps firing on an idle adapter is a tick the machine cannot spare.
	mov	ebx, [esi].DEVEXT.dxPendSrb
	mov	[esi].DEVEXT.dxPendSrb, 0
	mov	[esi].DEVEXT.dxState, LS_ST_IDLE
	mov	eax, [esi].DEVEXT.dxPendStat
	mov	[ebx].SCSI_REQUEST_BLOCK.SrbStatus, al

	push	ebx
	push	DeviceExtension
	push	RequestComplete
	call	ScsiPortNotification
	add	esp, 12

	push	DeviceExtension
	push	NextRequest
	call	ScsiPortNotification
	add	esp, 8
	jmp	lt_idle

lt_rearm:
;  Still working. Ask to be called again in a millisecond.
	push	LS_TIMER_US
	push	OFFSET LsTimer
	push	DeviceExtension
	push	RequestTimerCall
	call	ScsiPortNotification
	add	esp, 16

lt_idle:
	pop	edi
	pop	esi
	pop	ebx
	ret

LsTimer ENDP

;============================================================================
; LsResetBus - nothing to reset. Returning FALSE would tell SCSIPORT the
;              adapter is broken; nothing is wrong, so say so.
;============================================================================
LsResetBus PROC STDCALL DeviceExtension:DWORD, PathId:DWORD

	mov	eax, TRUE
	ret

LsResetBus ENDP

;============================================================================
; LsStartIo - validate, stash, arm the timer, RETURN.
;
;   RESTRUCTURED 2026-09-11. This used to run the entire ATAPI command inline
;   and then arm a timer to report the answer it already had, which made the
;   deferral cosmetic. The vendor's HwStartIo (rva 0x29ac) returns without
;   completing, and so does PC2X's. IMPLEMENTATION.md section 1.2.
;
;   NO DEVICE I/O HAPPENS IN THIS FUNCTION.
;============================================================================
LsStartIo PROC STDCALL DeviceExtension:DWORD, Srb:DWORD

	push	ebx
	push	esi
	push	edi

	mov	ebx, Srb
	mov	esi, DeviceExtension
	inc	[esi].DEVEXT.dxSrbCount

	movzx	eax, [ebx].SCSI_REQUEST_BLOCK.Function
	cmp	eax, SRB_FUNCTION_EXECUTE_SCSI
	jne	lsi_badfunc

ifndef LS_PHASE2
;  PHASE 0: nothing is attached, as far as this build is concerned.
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lsi_defer
else
;----------------------------------------------------------------------------
;  PHASE 2: SRB -> ATAPI, and it is very nearly a pass-through.
;
;  An ATAPI command packet IS a SCSI CDB. SCSIPORT hands us a CDB in the SRB
;  and, for a command of twelve bytes or fewer, it goes on the wire unchanged
;  - zero-padded if the class driver sent a 6- or 10-byte form. So this layer
;  does no command translation at all, which is the whole reason an ATAPI
;  device is worth presenting as SCSI.
;----------------------------------------------------------------------------
	cmp	[ebx].SCSI_REQUEST_BLOCK.Lun, 0
	jne	lsi_noselect
	cmp	[ebx].SCSI_REQUEST_BLOCK.TargetId, 0
	jne	lsi_noselect

;  Copy the CDB into a zero-filled twelve-byte packet. SCSIPORT serialises
;  requests for one adapter, so a single static packet is safe.
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

	mov	esi, DeviceExtension
	mov	[esi].DEVEXT.dxDir, edx
	mov	eax, [ebx].SCSI_REQUEST_BLOCK.DataBuffer
	mov	[esi].DEVEXT.dxBuf, eax
	mov	eax, [ebx].SCSI_REQUEST_BLOCK.DataTransferLength
	mov	[esi].DEVEXT.dxLen, eax
	mov	[esi].DEVEXT.dxRetry, 0

;  First request of all opens the bridge; after that go straight to waiting
;  for the drive. Either way the work happens in the timer, not here.
	mov	eax, [esi].DEVEXT.dxColdDone
	or	eax, eax
	jnz	lsi_warm
	mov	[esi].DEVEXT.dxState, LS_ST_COLDSTART
	jmp	lsi_arm
lsi_warm:
	mov	[esi].DEVEXT.dxState, LS_ST_READY
	jmp	lsi_arm

lsi_badreq:
	mov	al, SRB_STATUS_INVALID_REQUEST
	jmp	lsi_defer
lsi_noselect:
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	lsi_defer
endif

lsi_badfunc:
	mov	al, SRB_STATUS_BAD_FUNCTION

lsi_defer:
;  A verdict we already have. Still completed from the timer, because
;  completing inside HwStartIo is the thing this restructure removes.
	movzx	eax, al
	mov	esi, DeviceExtension
	mov	[esi].DEVEXT.dxPendStat, eax
	mov	[esi].DEVEXT.dxState, LS_ST_DONE

lsi_arm:
	mov	esi, DeviceExtension
	mov	[esi].DEVEXT.dxPendSrb, ebx
	mov	[esi].DEVEXT.dxTicks, 0

;  ScsiPortNotification is cdecl and varargs, so we clean the stack.
	push	LS_TIMER_US
	push	OFFSET LsTimer
	push	DeviceExtension
	push	RequestTimerCall
	call	ScsiPortNotification
	add	esp, 16

	pop	edi
	pop	esi
	pop	ebx
	mov	eax, TRUE
	ret

LsStartIo ENDP

	END
