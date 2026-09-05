;============================================================================
; XTIDEMP.ASM - Windows 95 SCSI miniport for the Lo-tech XT-CF / XT-IDE
;
;   Part of https://github.com/Mike1978uk/win95-intel-inboard-386pc (issue #21).
;
;   This replaces the IOS port driver (`PORT.PDR`) that reached the same disk
;   and then wedged Windows at shutdown. The justification is a measurement,
;   not an argument: Adaptec's T130.MPD - a polling, no-IRQ SCSI miniport -
;   holds a written FAT16 volume through a clean teardown on the same Windows
;   image the wedge reproduces on. So the SCSI stack is sound on this machine
;   and the failing layer is the one this file deletes. Technique 94, and
;   docs/scsi_miniport_costing.md.
;
;   What is new here is ONLY the SRB-to-ATA translation. The transport in
;   XTIDETR.ASM is carried across unchanged; the ~1,500 lines of IOS glue that
;   used to surround it - the calldown insert, the DCB lifecycle, the
;   self-published volume, the completion path - are gone, and every bug in
;   this investigation lived in them.
;
;   Calling convention: stdcall. SRB.INC declares the callbacks `PROTO C`,
;   which is an H2INC artefact - the header was generated without the
;   compiler's -Gz. Settled against a working driver instead of the sample:
;   T130.MPD's .text contains exactly one `ret 18h`, which is HwFindAdapter's
;   six dword arguments being cleaned by the callee. Technique 81.
;============================================================================

	.386
	.MODEL	FLAT, STDCALL
	OPTION	CASEMAP:NONE

	INCLUDE	miniport.inc
	INCLUDE	srb.inc

;----------------------------------------------------------------------------
;  SCSIPORT services. Names taken from BLOCK/LIB/SCSIPORT.LIB's own symbol
;  table, not from a header - the decoration has to match the library exactly
;  and a header cannot prove it does.
;
;  ScsiPortNotification is varargs and therefore cdecl, undecorated. The rest
;  are stdcall.
;----------------------------------------------------------------------------
ScsiPortInitialize	PROTO	STDCALL :DWORD, :DWORD, :DWORD, :DWORD
ScsiPortGetDeviceBase	PROTO	STDCALL :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD, :DWORD
ScsiPortNotification	PROTO	C	:DWORD, :VARARG

;----------------------------------------------------------------------------
;  From the transport (XTIDETR.ASM), carried over from the port driver.
;----------------------------------------------------------------------------
EXTERNDEF	XTIDE_Probe		: NEAR
EXTERNDEF	XTIDE_XferRun		: NEAR
EXTERNDEF	XTIDE_SelectUnit	: NEAR

EXTERNDEF	XTIDE_BasePort		: WORD
EXTERNDEF	XTIDE_Stride		: DWORD
EXTERNDEF	XTIDE_Transport		: DWORD
EXTERNDEF	XTIDE_MasterPresent	: DWORD
EXTERNDEF	XTIDE_SlavePresent	: DWORD
EXTERNDEF	XTIDE_TotalSec		: DWORD
EXTERNDEF	XTIDE_Cyls		: DWORD
EXTERNDEF	XTIDE_Heads		: DWORD
EXTERNDEF	XTIDE_Spt		: DWORD
EXTERNDEF	XTIDE_ReqLba		: DWORD
EXTERNDEF	XTIDE_ReqCount		: DWORD
EXTERNDEF	XTIDE_ReqBuf		: DWORD
EXTERNDEF	XTIDE_ReqIsWrite	: DWORD
EXTERNDEF	XTIDE_UnitModel		: BYTE

;----------------------------------------------------------------------------
;  SCSI opcodes we answer. Kept local rather than pulled from SCSI.INC so the
;  set this driver actually implements is visible in one place.
;----------------------------------------------------------------------------
SCSIOP_TEST_UNIT_READY	equ	000h
SCSIOP_REZERO_UNIT	equ	001h
SCSIOP_REQUEST_SENSE	equ	003h
SCSIOP_READ6		equ	008h
SCSIOP_WRITE6		equ	00Ah
SCSIOP_INQUIRY		equ	012h
SCSIOP_MODE_SELECT6	equ	015h
SCSIOP_MODE_SENSE6	equ	01Ah
SCSIOP_START_STOP_UNIT	equ	01Bh
SCSIOP_MEDIUM_REMOVAL	equ	01Eh
SCSIOP_READ_CAPACITY	equ	025h
SCSIOP_READ		equ	028h
SCSIOP_WRITE		equ	02Ah
SCSIOP_SEEK		equ	02Bh
SCSIOP_VERIFY		equ	02Fh
SCSIOP_SYNCHRONIZE_CACHE equ	035h
SCSIOP_MODE_SENSE10	equ	05Ah

;  Our one adapter. The card is a single-channel, single-target 8-bit ATA
;  bridge, so target 0 is the ATA master and target 1 would be the slave.
;  Only the master is claimed - see XT_ALLOW_SLAVE below.
XT_MAX_TARGETS		equ	2
XT_SECTOR_SIZE		equ	512

;  64 KB. XTIDE_XferRun already chunks at 128 sectors per taskfile command, so
;  this is the transfer SCSIPORT may hand us in one SRB, not the ATA limit.
XT_MAX_XFER		equ	65536

;  Default I/O base if the device node supplies none. MEASURED on the real
;  card 2026-08-31; the INF sets it too, so this is a floor and not a policy.
ifndef XT_DEFAULT_BASE
XT_DEFAULT_BASE		equ	0300h
endif

;  Number of ports the card decodes. Stride 2 puts 16 registers across 32
;  bytes; the silkscreen on the real card agrees - "IO: 300-31Fh".
XT_PORT_COUNT		equ	32

;----------------------------------------------------------------------------
;  Per-adapter storage. The transport keeps its state in module variables, so
;  this is small on purpose - but SCSIPORT needs a non-zero size and hands the
;  pointer back to every callback, so keep the base here too rather than
;  relying only on the global.
;----------------------------------------------------------------------------
DEVEXT	STRUCT
dxBase		DWORD	?
dxFound		DWORD	?
dxReserved	DWORD	6 DUP (?)
DEVEXT	ENDS

	.DATA

;  Inquiry template. Byte 0 = 00h: direct-access device, connected.
;  Byte 2/3 = 02h: claims SCSI-2 and the matching response format, which is
;  what DiskTSD expects to see before it will take the device.
XtInquiry	LABEL	BYTE
	db	000h			; peripheral qualifier / device type
	db	000h			; RMB clear - not removable
	db	002h			; ANSI version
	db	002h			; response data format
	db	31			; additional length (total 36)
	db	0, 0, 0
	db	'XT-IDE  '		; vendor id, 8
	db	'                '	; product id, 16 - filled from IDENTIFY
	db	'1.0 '			; revision, 4
XT_INQUIRY_LEN	equ	$ - XtInquiry

;  Mode sense (6) reply: a header claiming no block descriptors and no pages.
;  Enough to satisfy a class driver that asks; this card has nothing to report.
XtModeSense	LABEL	BYTE
	db	3			; mode data length (excludes this byte)
	db	0			; medium type
	db	0			; device-specific (bit 7 = write protected)
	db	0			; block descriptor length
XT_MODESENSE_LEN equ	$ - XtModeSense

;  Read capacity (10) reply, big-endian: last LBA then block length.
XtCapacity	db	8 dup (0)

	.CODE

;============================================================================
; DriverEntry - the PE entry point named in XTIDEMP.LNK.
;
;   Builds HW_INITIALIZATION_DATA and hands it to SCSIPORT. Everything after
;   this is SCSIPORT calling us.
;============================================================================
	PUBLIC	DriverEntry
DriverEntry PROC STDCALL PUBLIC DriverObject:DWORD, Argument2:DWORD

	LOCAL	hwInit	: HW_INITIALIZATION_DATA
	LOCAL	context	: DWORD

	push	ebx
	push	esi
	push	edi

;  Zero it. Every field we do not set has to read as FALSE/NULL, and the
;  sample does this by hand for the same reason - there is no initialiser.
	lea	edi, hwInit
	mov	ecx, SIZEOF HW_INITIALIZATION_DATA
	xor	eax, eax
	cld
	rep	stosb

	mov	hwInit.HwInitializationDataSize, SIZEOF HW_INITIALIZATION_DATA
	mov	hwInit.AdapterInterfaceType, Isa

	mov	hwInit.HwInitialize, OFFSET XtInitialize
	mov	hwInit.HwStartIo, OFFSET XtStartIo
	mov	hwInit.HwFindAdapter, OFFSET XtFindAdapter
	mov	hwInit.HwResetBus, OFFSET XtResetBus

;  HwInterrupt and HwDmaStarted stay NULL, and that is deliberate.
;
;  The XT-CF is jumpered without an interrupt, so this driver polls. Declaring
;  an interrupt handler with BusInterruptLevel = 0 would ask SCSIPORT to
;  connect IRQ 0 - the system timer. That exact mistake, made by a shipping
;  vendor driver, is what killed the keyboard on this machine in issue #22
;  (technique 75). A NULL handler and a zero level say "we poll" without
;  asking for anything.

	mov	hwInit.DeviceExtensionSize, SIZEOF DEVEXT
	mov	hwInit.SpecificLuExtensionSize, 0
	mov	hwInit.SrbExtensionSize, 0
	mov	hwInit.NumberOfAccessRanges, 1

;  MapBuffers TRUE is the single most valuable line in this file.
;
;  It makes SCSIPORT hand us Srb.DataBuffer as a flat, mapped virtual address
;  covering the whole transfer, and do any scatter/gather decomposition itself.
;  The port driver had to walk the descriptor list by hand, and misreading it
;  is what wrote ring-0 heap into the volume's root directory on 2026-09-01
;  (technique 85). That entire bug class is now somebody else's to get right,
;  and they already have.
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
;  stdcall - ScsiPortInitialize cleans its own arguments. EAX is the status
;  SCSIPORT wants returned verbatim.

	pop	edi
	pop	esi
	pop	ebx
	ret

DriverEntry ENDP

;============================================================================
; XtFindAdapter - decide whether our card is here, and describe it.
;
;   SCSIPORT calls this once per candidate configuration. We have exactly one
;   card at one base, so the second call reports not-found and `Again` stays
;   FALSE.
;============================================================================
XtFindAdapter PROC STDCALL DeviceExtension:DWORD, Context:DWORD,
			BusInformation:DWORD, ArgumentString:DWORD,
			ConfigInfo:DWORD, Again:DWORD

	push	ebx
	push	esi
	push	edi

	mov	esi, ConfigInfo

;  Only ever one adapter.
	mov	eax, Again
	mov	byte ptr [eax], FALSE

;  The I/O range comes from the device node, through SCSIPORT's own parsed
;  AccessRanges. Reading the node rather than hardcoding is what lets the
;  Settings tab move the card without a rebuild - the port driver learned that
;  from the LS-120's AdapterSettings (technique 77).
	mov	edi, [esi].PORT_CONFIGURATION_INFORMATION.AccessRanges
	or	edi, edi
	jz	xfa_default
	cmp	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfAccessRanges, 0
	je	xfa_default
	mov	eax, [edi].ACCESS_RANGE.RangeStart.LowPart
	or	eax, eax
	jz	xfa_default
	jmp	xfa_havebase

xfa_default:
	mov	eax, XT_DEFAULT_BASE

xfa_havebase:
	mov	ebx, DeviceExtension
	mov	[ebx].DEVEXT.dxBase, eax

;  Claim the range through SCSIPORT rather than just using the number.
;
;  The transport moves every byte with raw IN and OUT, so strictly we only need
;  the port number - and on x86 ISA this call returns the port number back,
;  identity-mapped. It is here because the working driver on this machine does
;  it: T130.MPD imports ScsiPortGetDeviceBase and ScsiPortValidateRange, and a
;  shipping implementation outranks the sample (technique 81). Costs one call
;  at init and makes the claim explicit to SCSIPORT.
	push	TRUE			; InIoSpace
	push	XT_PORT_COUNT		; NumberOfBytes
	push	0			; IoAddress.HighPart
	push	eax			; IoAddress.LowPart
	push	[esi].PORT_CONFIGURATION_INFORMATION.SystemIoBusNumber
	push	Isa			; BusType
	push	ebx			; HwDeviceExtension
	call	ScsiPortGetDeviceBase
	or	eax, eax
	jnz	xfa_mapped
;  A refusal is not fatal on this bus - fall back to the raw port number, which
;  is what the mapping would have been.
	mov	ebx, DeviceExtension
	mov	eax, [ebx].DEVEXT.dxBase
xfa_mapped:
	mov	XTIDE_BasePort, ax

;  Probe. This is the transport's own entry point, unchanged from the port
;  driver: settle the register stride by reading only, settle the data
;  transport, IDENTIFY the master, capture geometry, and read LBA 0 back to
;  prove the path end to end.
	call	XTIDE_Probe
	jc	xfa_notfound

	mov	ebx, DeviceExtension
	mov	[ebx].DEVEXT.dxFound, 1

;  Describe the adapter. Everything not set here was zeroed by SCSIPORT.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfBuses, 1
	mov	[esi].PORT_CONFIGURATION_INFORMATION.MaximumTransferLength, XT_MAX_XFER
	mov	[esi].PORT_CONFIGURATION_INFORMATION.NumberOfPhysicalBreaks, 0
	mov	[esi].PORT_CONFIGURATION_INFORMATION.MaximumNumberOfTargets, XT_MAX_TARGETS
	mov	[esi].PORT_CONFIGURATION_INFORMATION.AlignmentMask, 0

;  Initiator id 7 is the conventional host id. Nothing on this card cares, but
;  a class driver will not enumerate a target it believes is the initiator, so
;  it must not collide with target 0.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.InitiatorBusId, 7

;  No interrupt, no DMA, no bus mastering. Everything moves through IN and OUT
;  on an 8-bit bus. See the note in DriverEntry about NOT asking for IRQ 0.
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

;  RealModeInitialized - the one genuinely open field.
;
;  Documented as "indicates the real-mode driver has initialized the card;
;  always initialized by ScsiPort". On this machine that is literally true:
;  the XT-IDE Universal BIOS option ROM has already set the card up and INT 13h
;  is serving the boot disk through the real-mode mapper when we are called.
;
;  It is left FALSE anyway, because FALSE is what PC2X and every sample use and
;  it is therefore the only value with evidence behind it. If the miniport
;  loads and the boot volume is NOT taken over from RMM, this is the first
;  thing to flip - it is one rebuild, and -DXT_REAL_MODE_INIT does it.
ifdef XT_REAL_MODE_INIT
	mov	[esi].PORT_CONFIGURATION_INFORMATION.RealModeInitialized, TRUE
else
	mov	[esi].PORT_CONFIGURATION_INFORMATION.RealModeInitialized, FALSE
endif

;  BufferAccessScsiPortControlled says the miniport will NOT touch the data
;  buffers directly. We move every byte ourselves with IN and OUT, so it is
;  FALSE. An earlier session recorded this field as "makes SCSIPORT emulate
;  scatter/gather", which was wrong; that is SCSIPORT_GF_EMUL_SG.
	mov	[esi].PORT_CONFIGURATION_INFORMATION.BufferAccessScsiPortControlled, FALSE

;  Fill the INQUIRY product id from the model string IDENTIFY returned, so the
;  device shows up in Device Manager as the drive that is actually fitted
;  rather than as a generic name.
	call	XtFillProductId

	pop	edi
	pop	esi
	pop	ebx
	mov	eax, SP_RETURN_FOUND
	ret

xfa_notfound:
	pop	edi
	pop	esi
	pop	ebx
	mov	eax, SP_RETURN_NOT_FOUND
	ret

XtFindAdapter ENDP

;============================================================================
; XtFillProductId - copy 16 characters of the de-swapped IDENTIFY model string
;                   into the INQUIRY template's product-id field.
;
;   Uses: EAX, ECX, ESI, EDI
;============================================================================
XtFillProductId PROC

	mov	esi, OFFSET XTIDE_UnitModel	; unit 0 = master, 40 bytes
	mov	edi, OFFSET XtInquiry + 16
	mov	ecx, 16
	cld
xfp_loop:
	lodsb
	cmp	al, 20h				; scrub anything unprintable -
	jb	xfp_space			; a garbled byte here would be
	cmp	al, 7Eh				; visible in Device Manager
	jbe	xfp_store
xfp_space:
	mov	al, 20h
xfp_store:
	stosb
	loop	xfp_loop
	ret

XtFillProductId ENDP

;============================================================================
; XtInitialize - called after FindAdapter succeeded.
;
;   The card needs no initialisation beyond what the probe already did: the
;   option ROM has set it up and the drive has answered IDENTIFY.
;============================================================================
XtInitialize PROC STDCALL DeviceExtension:DWORD

	mov	eax, TRUE
	ret

XtInitialize ENDP

;============================================================================
; XtResetBus - there is no bus to reset.
;
;   Returning FALSE would tell SCSIPORT the adapter is broken. There is
;   nothing to do and nothing wrong, so say so.
;============================================================================
XtResetBus PROC STDCALL DeviceExtension:DWORD, PathId:DWORD

	mov	eax, TRUE
	ret

XtResetBus ENDP

;============================================================================
; XtStartIo - the SRB dispatch. This is the only genuinely new code in the
;             port; everything below it is the transport we already had.
;
;   Completion is synchronous: the transfer runs inline and the request is
;   completed before returning. That is what zikolas/cfu1-win9x's synchronous
;   polled engine does, and his own notes record the trade - it freezes the UI
;   during a transfer and it tears down cleanly. Tearing down cleanly is the
;   entire point of this port, so take that trade first and make it
;   asynchronous later, from the timer callback SCSIPORT already provides
;   (ScsiPortNotification RequestTimerCall). Doing both at once would be two
;   variables in one build.
;============================================================================
XtStartIo PROC STDCALL DeviceExtension:DWORD, Srb:DWORD

	push	ebx
	push	esi
	push	edi

	mov	ebx, Srb

;  Everything that is not an EXECUTE_SCSI is either a no-op we can honestly
;  succeed, or something this adapter does not implement. Answering FLUSH and
;  SHUTDOWN honestly matters: a class driver that gets INVALID_REQUEST back
;  from a shutdown flush may not proceed, and shutdown is the path this whole
;  port exists to fix.
	movzx	eax, [ebx].SCSI_REQUEST_BLOCK.Function
	cmp	eax, SRB_FUNCTION_EXECUTE_SCSI
	je	xsi_execute
	cmp	eax, SRB_FUNCTION_FLUSH
	je	xsi_ok
	cmp	eax, SRB_FUNCTION_SHUTDOWN
	je	xsi_ok
	cmp	eax, SRB_FUNCTION_RESET_BUS
	je	xsi_ok
	cmp	eax, SRB_FUNCTION_RESET_DEVICE
	je	xsi_ok
	cmp	eax, SRB_FUNCTION_RELEASE_QUEUE
	je	xsi_ok
	jmp	xsi_badfunc

;----------------------------------------------------------------------------
xsi_execute:
;  Only target 0 (the ATA master) exists unless a slave was found. Report an
;  absent target as a selection timeout, which is what stops the class driver
;  walking the rest of the bus.
	cmp	[ebx].SCSI_REQUEST_BLOCK.Lun, 0
	jne	xsi_noselect
	movzx	eax, [ebx].SCSI_REQUEST_BLOCK.TargetId
	or	eax, eax
	jz	xsi_target_ok
	cmp	eax, 1
	jne	xsi_noselect
	cmp	XTIDE_SlavePresent, 0
	je	xsi_noselect
xsi_target_ok:

	movzx	eax, byte ptr [ebx].SCSI_REQUEST_BLOCK.Cdb

	cmp	al, SCSIOP_READ
	je	xsi_read10
	cmp	al, SCSIOP_WRITE
	je	xsi_write10
	cmp	al, SCSIOP_READ6
	je	xsi_read6
	cmp	al, SCSIOP_WRITE6
	je	xsi_write6
	cmp	al, SCSIOP_INQUIRY
	je	xsi_inquiry
	cmp	al, SCSIOP_READ_CAPACITY
	je	xsi_capacity
	cmp	al, SCSIOP_MODE_SENSE6
	je	xsi_modesense

;  Commands with nothing to do on a CF card behind a PIO bridge. Each is a
;  deliberate success, not a catch-all: TEST UNIT READY is how a class driver
;  asks whether the medium is there, SYNCHRONIZE CACHE is the shutdown flush,
;  and VERIFY on a device with no write cache has already happened.
	cmp	al, SCSIOP_TEST_UNIT_READY
	je	xsi_ok
	cmp	al, SCSIOP_SYNCHRONIZE_CACHE
	je	xsi_ok
	cmp	al, SCSIOP_VERIFY
	je	xsi_ok
	cmp	al, SCSIOP_START_STOP_UNIT
	je	xsi_ok
	cmp	al, SCSIOP_REZERO_UNIT
	je	xsi_ok
	cmp	al, SCSIOP_SEEK
	je	xsi_ok
	cmp	al, SCSIOP_MEDIUM_REMOVAL
	je	xsi_ok
	cmp	al, SCSIOP_MODE_SELECT6
	je	xsi_ok

	jmp	xsi_badreq

;----------------------------------------------------------------------------
;  READ(10) / WRITE(10). CDB is big-endian: LBA at [2..5], block count at
;  [7..8]. Assembled a byte at a time rather than with BSWAP, because BSWAP is
;  486 and up and the target machine is a 386 accelerator card.
;----------------------------------------------------------------------------
xsi_read10:
	mov	XTIDE_ReqIsWrite, 0
	jmp	xsi_rw10
xsi_write10:
	mov	XTIDE_ReqIsWrite, 1
xsi_rw10:
	lea	esi, [ebx].SCSI_REQUEST_BLOCK.Cdb

	movzx	eax, byte ptr [esi+2]
	shl	eax, 8
	mov	al, [esi+3]
	shl	eax, 8
	mov	al, [esi+4]
	shl	eax, 8
	mov	al, [esi+5]
	mov	XTIDE_ReqLba, eax

	movzx	eax, byte ptr [esi+7]
	shl	eax, 8
	mov	al, [esi+8]
	mov	XTIDE_ReqCount, eax
	jmp	xsi_transfer

;----------------------------------------------------------------------------
;  READ(6) / WRITE(6). 21-bit LBA in [1..3] with the top three bits of [1]
;  reserved, and a byte count in [4] where zero means 256 blocks.
;----------------------------------------------------------------------------
xsi_read6:
	mov	XTIDE_ReqIsWrite, 0
	jmp	xsi_rw6
xsi_write6:
	mov	XTIDE_ReqIsWrite, 1
xsi_rw6:
	lea	esi, [ebx].SCSI_REQUEST_BLOCK.Cdb

	movzx	eax, byte ptr [esi+1]
	and	eax, 01Fh
	shl	eax, 8
	mov	al, [esi+2]
	shl	eax, 8
	mov	al, [esi+3]
	mov	XTIDE_ReqLba, eax

	movzx	eax, byte ptr [esi+4]
	or	eax, eax
	jnz	xsi_have6
	mov	eax, 256
xsi_have6:
	mov	XTIDE_ReqCount, eax

;----------------------------------------------------------------------------
xsi_transfer:
;  A zero-length transfer is legal and means "no data" - succeed without
;  touching the card.
	cmp	XTIDE_ReqCount, 0
	je	xsi_ok

;  Refuse anything past the end of the medium rather than letting the taskfile
;  wrap. XTIDE_TotalSec is zero on a CHS-only drive, in which case the
;  transport addresses in CHS and this bound does not apply.
	cmp	XTIDE_TotalSec, 0
	je	xsi_nobound
	mov	eax, XTIDE_ReqLba
	add	eax, XTIDE_ReqCount
	jc	xsi_badreq
	cmp	eax, XTIDE_TotalSec
	ja	xsi_badreq
xsi_nobound:

;  MapBuffers is TRUE, so DataBuffer is a flat mapped address covering the
;  whole transfer and there is no descriptor list to walk. That is the whole
;  reason this driver cannot repeat technique 85.
	mov	eax, [ebx].SCSI_REQUEST_BLOCK.DataBuffer
	or	eax, eax
	jz	xsi_badreq
	mov	XTIDE_ReqBuf, eax

;  Select the ATA unit this target maps to. Target 0 is the master.
	movzx	eax, [ebx].SCSI_REQUEST_BLOCK.TargetId
	call	XTIDE_SelectUnit

	call	XTIDE_XferRun
	jc	xsi_ioerror

;  Report what actually moved. XferRun drains ReqCount to zero on success, so
;  the whole request transferred.
	mov	eax, [ebx].SCSI_REQUEST_BLOCK.DataTransferLength
	jmp	xsi_ok

;----------------------------------------------------------------------------
xsi_inquiry:
	mov	esi, OFFSET XtInquiry
	mov	ecx, XT_INQUIRY_LEN
	jmp	xsi_copyout

xsi_modesense:
	mov	esi, OFFSET XtModeSense
	mov	ecx, XT_MODESENSE_LEN
	jmp	xsi_copyout

xsi_capacity:
;  Big-endian last-LBA then block length. A drive with no LBA28 capacity
;  reports its CHS product instead, so a CHS-only card still gets a size.
	mov	eax, XTIDE_TotalSec
	or	eax, eax
	jnz	xsi_cap_have
	mov	eax, XTIDE_Cyls
	imul	eax, XTIDE_Heads
	imul	eax, XTIDE_Spt
xsi_cap_have:
	or	eax, eax
	jz	xsi_badreq
	dec	eax			; READ CAPACITY reports the LAST LBA

	mov	edi, OFFSET XtCapacity
	mov	ecx, eax
	shr	ecx, 24
	mov	[edi+0], cl
	mov	ecx, eax
	shr	ecx, 16
	mov	[edi+1], cl
	mov	ecx, eax
	shr	ecx, 8
	mov	[edi+2], cl
	mov	[edi+3], al

	mov	byte ptr [edi+4], 0
	mov	byte ptr [edi+5], (XT_SECTOR_SIZE SHR 16) AND 0FFh
	mov	byte ptr [edi+6], (XT_SECTOR_SIZE SHR 8) AND 0FFh
	mov	byte ptr [edi+7], XT_SECTOR_SIZE AND 0FFh

	mov	esi, OFFSET XtCapacity
	mov	ecx, 8

;----------------------------------------------------------------------------
;  Copy ECX bytes from ESI into the SRB's buffer, clamped to what the caller
;  asked for, and report the clamped length. A reply longer than the buffer is
;  a legal short transfer, not an error.
;----------------------------------------------------------------------------
xsi_copyout:
	mov	edi, [ebx].SCSI_REQUEST_BLOCK.DataBuffer
	or	edi, edi
	jz	xsi_badreq
	cmp	ecx, [ebx].SCSI_REQUEST_BLOCK.DataTransferLength
	jbe	xsi_copy_go
	mov	ecx, [ebx].SCSI_REQUEST_BLOCK.DataTransferLength
xsi_copy_go:
	mov	eax, ecx
	cld
	rep	movsb
	mov	[ebx].SCSI_REQUEST_BLOCK.DataTransferLength, eax
	jmp	xsi_ok

;----------------------------------------------------------------------------
;  Completion. One exit, four statuses.
;----------------------------------------------------------------------------
xsi_ok:
	mov	al, SRB_STATUS_SUCCESS
	jmp	xsi_complete
xsi_ioerror:
	mov	al, SRB_STATUS_ERROR
	jmp	xsi_complete
xsi_noselect:
	mov	al, SRB_STATUS_SELECTION_TIMEOUT
	jmp	xsi_complete
xsi_badreq:
	mov	al, SRB_STATUS_INVALID_REQUEST
	jmp	xsi_complete
xsi_badfunc:
	mov	al, SRB_STATUS_BAD_FUNCTION

xsi_complete:
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

XtStartIo ENDP

	END
