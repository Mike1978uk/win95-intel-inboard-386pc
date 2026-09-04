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
	include	blockdev.inc	; ior.inc .errnz-checks against BD_CB_* from here
	include	dcb.inc
	include	ior.inc
	include	iop.inc
	include	ilb.inc		; the IOS linkage block, for ILB_Service_rtn
	include	shell.inc		; _SHELL_CallAtAppyTime
	include	isp.inc		; ISP_CREATE_DCB / _INSERT_CALLDOWN / _ASSOCIATE_DCB

;  Store to a diagnostic field. Expands to NOTHING without XT_REQ_MARKER,
;  so a release build carries neither the store nor the field behind it.
;  Used instead of nineteen inline ifdef/endif pairs, one of which would
;  certainly have been wrong.
ifdef XT_REQ_MARKER
MARK	macro	fld, val
	mov	fld, val
	endm
else
MARK	macro	fld, val
	endm
endif
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

;  Shutdown fast-fail limit. See docs/next_session_2026_09_04.md. A wait that
;  expires at the FULL limit while shutting down means the drive has stopped
;  answering, so every later wait uses this instead of costing another 0.6 s.
;  A CF card can legitimately stall for hundreds of ms on an internal erase,
;  which is why this is gated on shutdown and never armed while Windows runs.
XT_SPIN_SHUT	equ	4000		; ~6 ms, once the drive has gone quiet

;  Inner-loop count for one unit of the boot-log delay channel. Calibrated on
;  86Box's Deskpro bed, where one unit is about 11.6 log ticks. The 5160 is
;  several times faster, so the same constant compresses every fail code into
;  a handful of ticks and the channel stops being readable - which is how a
;  hardware run on 2026-09-01 produced a 4-tick failure that could not be
;  decoded. Scale it with -TimeBase for a hardware diagnostic build.
ifndef XT_TIMEBASE
XT_TIMEBASE	equ	725000
endif

;  IDENTIFY DEVICE result layout (ATA-1 onwards)
ID_MODEL_OFF	equ	54		; 40-byte model string, word 27..46
ID_MODEL_LEN	equ	40
ID_MIN_TEXT	equ	4		; minimum non-blank printable chars
ID_MIN_RUN	equ	4		; and they must include one unbroken run

;============================================================================
VxD_LOCKED_DATA_SEG

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
ifndef XT_STRIDE
XT_STRIDE	equ	2		; -DXT_STRIDE=1 for a classic XTIDE (86Box's model)
endif					; -DXT_STRIDE=0 to autodetect
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
extrn	port_ilb:dword		; IOS linkage block, defined in PORT.ASM

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

;  Scatter/gather walk.
;
;  `IOR_buffer_ptr` points to a data buffer ONLY while IORF_SCATTER_GATHER is
;  clear. With it set the DDK says outright that it "points to a list of linear
;  (logical) scatter-gather descriptors" - `BlockDev_Scatter_Gather` in
;  BLOCKDEV.INC, two dwords per entry, SECTOR COUNT first and the buffer
;  pointer SECOND.
;
;  Reading that field blind is what put ring-0 heap on the disk on 2026-09-01:
;  VFAT's mount-time reads are single-buffer, so the read path looked correct,
;  and its first write was scatter/gather, so we transmitted the descriptor
;  list itself into the root directory. A round trip cannot catch this either -
;  both halves would move the same wrong bytes.
;
;  The list is terminated by a null pointer or a zero count, and separately
;  bounded by IOR_xfer_count; honour both, and clamp the last segment to
;  whatever the bound has left. Same shape as zikolas/cfu1-win9x's rr_sg/rw_sg,
;  which is a shipping driver that has actually executed this path.
XTIDE_SgCur			dd	0	; -> current descriptor
XTIDE_SgLeft			dd	0	; sectors the IOR still accounts for
;  Poll-limit state. XTIDE_ShutDown is armed at exactly one call site, the
;  AEP_SYSTEM_SHUTDOWN branch of Port_Async_Request, and never cleared - so
;  while Windows is running, SpinLimit is XT_SPIN and behaviour is unchanged.
;  A successful wait restores the full limit, so even after the clamp arms, a
;  drive that comes back gets its patience back.
;  Completion ladder. OWN STATE - shares nothing with the transport, which is
;  what voided the on-disk request marker as a diagnostic (technique 88).
;  Reported through the emulator debug port at 0E0h, so measuring the disk
;  path costs no disk access.
XTIDE_DbgOffered		dd	0	; IOPs XTIDE_WantIop was asked about
XTIDE_DbgAccepted		dd	0	; ...of which we claimed
XTIDE_DbgStarted		dd	0	; XTIDE_StartRequest entries
XTIDE_DbgDone			dd	0	; IOPs handed back through IOP_callback_ptr

XTIDE_ShutDown			dd	0	; set only by XTIDE_ShutdownArm
XTIDE_SpinLimit			dd	XT_SPIN	; live poll limit, read by both wait loops

XTIDE_ReqIsWrite		dd	0	; direction, so the transfer loop does
					; not have to hold the IOP in EBX

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

;  Request-path marker. Opt-in, -DXT_REQ_MARKER, and NOT in a shipped driver.
;  The delay channel in PORT_Device_Init can only report registration, so it
;  cannot answer "did an IOS request ever reach XTIDE_StartRequest". This sector
;  can: the host reads it back out of the image afterwards. Technique 79 - the
;  proof of an address has to come from outside the guest.
ifdef XT_REQ_MARKER
public XTIDE_Marker
XTIDE_Marker		db	'XTIDEREQ'
XTIDE_MkCount		dd	0
XTIDE_MkEntry		dd	0
XTIDE_MkFunc		dd	0
XTIDE_MkLba		dd	0
XTIDE_MkXfer		dd	0
XTIDE_MkUnit		dd	0
XTIDE_MkCfg		dd	0	; AEP_CONFIG_DCB calls seen
XTIDE_MkWant		dd	0	; ...of which we claimed
XTIDE_MkIsp		dd	0	; ISP_result of the last calldown insert
XTIDE_MkIopDcb		dd	0	; raw IOP_physical_dcb of the last request
XTIDE_MkOurDcb		dd	0	; the DCB we actually inserted a calldown on
XTIDE_MkRefused		dd	0	; requests refused as not ours
XTIDE_MkStage		dd	0	; how far Port_request got - see XTIDE_MarkStage
XTIDE_MkOrigDcb		dd	0	; IOP_original_dcb - the logical view
XTIDE_MkCdDdb		dd	0	; DDB on the calldown entry IOS reached us through
XTIDE_MkOurDdb	dd	0	; report copy of the working XTIDE_OurDdb
XTIDE_MkIop		dd	0	; EBX as Port_request received it
XTIDE_MkRaw		dd	8 dup (0)	; first 32 bytes at that address, verbatim

;  The first eight requests, as (function, LBA) pairs. A raw surface with no
;  partition table draws exactly one read and then silence; mounting a real
;  volume draws the partition sector, the boot sector, both FATs and the root
;  directory, at addresses the host can predict. The difference between those
;  two shapes is the difference between a driver that answered once and a
;  driver that is carrying a filesystem.
XTIDE_MkRing		dd	16 dup (0)

;  What XTIDE_ConfigDcb actually put in the DCB. Every theory about why the
;  volume will not mount assumes these are sane, and not one run has checked.
XTIDE_MkCyls		dd	0
XTIDE_MkHeads		dd	0
XTIDE_MkSpt		dd	0
XTIDE_MkTotal		dd	0
XTIDE_MkDevFlags	dd	0
XTIDE_MkVolCreate	dd	0	; ISP_result of ISP_CREATE_DCB
XTIDE_MkVolCd		dd	0	; ISP_result of the logical calldown insert
XTIDE_MkVolAssoc	dd	0	; ISP_result of ISP_ASSOCIATE_DCB
XTIDE_MkVolDrive	dd	0	; drive number it was associated with, 3 = D:
XTIDE_MkPartStart	dd	0
XTIDE_MkPartLen		dd	0
XTIDE_MkVolStep		dd	0	; 0 = XTIDE_VolCreate never ran; 10 = volume up
			dd	0	; placeholder: XTIDE_VolBusy is working storage now
XTIDE_MkSchedRc		dd	0FFFFFFFFh	; -1 = never even tried to queue;
					; 0 = _SHELL_CallAtAppyTime refused
			dd	0	; placeholder: XTIDE_OurUnit is working storage now
XTIDE_MkStride		dd	0	; what XTIDE_DetectStride settled on, 0 = never ran

;  First WRITE the request path ever saw, captured verbatim. The write on
;  2026-09-01 reached LBA 126 and put ring-0 heap there instead of the
;  directory sector, so every field read through the assumption "this pointer
;  is a flat data buffer" is worthless (technique 82). Dump the bytes and the
;  flags and let them say what the pointer is.
XTIDE_MkWrFlags		dd	0	; IOR_flags - IORF_SCATTER_GATHER lives here
XTIDE_MkWrBuf		dd	0	; IOR_buffer_ptr as read
XTIDE_MkWrSgd		dd	0	; IOR_sgd_lin_phys
XTIDE_MkWrNsgd		dd	0	; IOR_num_sgds
XTIDE_MkWrLba		dd	0
XTIDE_MkWrCnt		dd	0
XTIDE_MkWrEsi		dd	0	; ESI as XTIDE_WriteSectors actually used it
XTIDE_MkWrData		db	32 dup (0)	; first 32 bytes at IOR_buffer_ptr

;  How many requests arrived with IORF_SCATTER_GATHER set, by direction. A
;  snapshot of the FIRST write cannot answer "does this ever happen", which is
;  the question that decides whether the descriptor walk is load-bearing or
;  dead code - a counter reports whether anything happened, a captured value
;  only reports the last thing that did (technique 83).
XTIDE_MkSgRd		dd	0
XTIDE_MkSgWr		dd	0
			db	208 dup (0)	; pads XTIDE_Marker to exactly 512 bytes
endif		; XT_REQ_MARKER: no symbol and no payload in a shipped build


;  State the DRIVER needs, as distinct from the report. These used to live
;  inside the marker sector, which meant a build with no diagnostic could
;  not assemble at all - the diagnostic was load-bearing by accident.
XTIDE_OurDdb		dd	0	; our own DDB, from AEP_ddb at config time
XTIDE_OurUnit		dd	0	; the unit we claimed - addressing, never reported
XTIDE_PartStart		dd	0	; partition LBA. Read on EVERY request for the bias
XTIDE_PartLen		dd	0
XTIDE_VolDrive		dd	0	; drive-letter walk: 3=D, incremented until one is free
XTIDE_VolBusy		dd	0	; one appy-time queue attempt in flight is enough
;  Working storage - deliberately AFTER the marker sector, not inside it.
XTIDE_OurLgn		dd	0	; load group number, for the logical calldown
XTIDE_LogDcb		dd	0	; the logical volume DCB, once created
XTIDE_MbrBuf		db	512 dup (0)
public XTIDE_RbBuf
XTIDE_RbBuf			db	512 dup (0)

;----------------------------------------------------------------------------
;  Per-unit geometry. Phase 2b kept one global set, which was fine while the
;  probe was the only caller - but the moment IOS asks about two units, the
;  slave's IDENTIFY overwrites the master's numbers and every later request
;  addresses the wrong shape of disk.
;
;  Index 0 = master, 1 = slave, matching DCB_unit_on_ctl.
;----------------------------------------------------------------------------
XTIDE_UnitPresent	dd	2 dup (0)
XTIDE_UnitCyls		dd	2 dup (0)
XTIDE_UnitHeads		dd	2 dup (0)
XTIDE_UnitSpt		dd	2 dup (0)
XTIDE_UnitTotal		dd	2 dup (0)
XTIDE_UnitLbaOk		dd	2 dup (0)

;  Non-zero for each unit inquiry actually claimed - the mask and the presence
;  test have both already been applied by then, so this is the one flag that
;  says "we said yes to this unit".
;
;  It holds the inquiry-time DCB pointer, but ONLY as a flag. Do not compare it
;  against the DCB that arrives with AEP_CONFIG_DCB: STORAGE.DOC says the IOS
;  "creates a real DCB and fills it with information supplied by the driver",
;  so the two are different objects. Measured 2026-09-01 - a pointer-identity
;  guard reported cfg=1 claimed=0 and declined our own device, and no request
;  was ever issued.
XTIDE_ClaimedDcb	dd	2 dup (0)

;  The DCB we inserted a calldown on, per unit. AEP_CONFIG_DCB is broadcast for
;  DCBs that are not ours and the same DCB can come round again on a
;  re-broadcast; inserting twice is a wild calldown. zikolas/cfu1-win9x guards
;  the same way and says why - the DDK sample guards neither.
XTIDE_InsertedDcb	dd	2 dup (0)

;  Which units this driver is willing to claim, one bit each. The default
;  claims both; the emulator loop builds with 2 so the first request-path run
;  is aimed at a scratch slave and not at the boot disk.
ifndef XT_CLAIM_MASK
XT_CLAIM_MASK	equ	3
endif

;  Sector-count register is 8 bits, and 0 there means 256. IOS can ask for far
;  more than one command can carry, so requests are chunked.
XT_MAX_CHUNK	equ	128		; 64 KB per command

XTIDE_ReqUnit		dd	0

;  Per-unit model string, de-swapped. ATA stores its text two characters per
;  16-bit word with the pair reversed, so it has to be unswapped before it is
;  fit to hand to anything that will display it.
XTIDE_UnitModel		db	80 dup (32)	; 2 units x 40 bytes, space filled

VxD_LOCKED_DATA_ENDS

;============================================================================
VxD_LOCKED_CODE_SEG

	extrn	Port_request:near	; our own calldown entry, in PORTREQ.ASM

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
BeginProc XTIDE_DetectStride

	mov	XTIDE_Stride, 1
	call	xds_try
	jnc	xds_ok
	mov	XTIDE_Stride, 2
	call	xds_try
	jnc	xds_ok
	stc
	ret
xds_ok:
	mov	eax, XTIDE_Stride
	MARK	XTIDE_MkStride, eax
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

EndProc XTIDE_DetectStride

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
ifdef XT_REQ_MARKER
;  What the transport ACTUALLY transmitted from, recorded here rather than at
;  the call site, so a clobber between the two shows up as a mismatch instead
;  of being invisible. MkWrBuf is set by XTIDE_MarkWrite on the first data
;  write and is still zero for the marker's own writes, so this gate picks the
;  first REAL write and nothing else.
	cmp	XTIDE_MkWrBuf, 0
	je	xwr_noesi
	cmp	XTIDE_MkWrEsi, 0
	jne	xwr_noesi
	MARK	XTIDE_MkWrEsi, esi
xwr_noesi:
endif
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
; XTIDE_CopyId - de-swap this unit's 40-byte model string out of the IDENTIFY
;                buffer into XTIDE_UnitModel.
;
;   In:   EAX = unit index, ESI -> IDENTIFY buffer
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_CopyId

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

EndProc XTIDE_CopyId

;----------------------------------------------------------------------------
; XTIDE_CopyStr - copy ECX bytes from EDX to EDI, advancing EDX.
;
;   Uses: EAX, ECX, EDX, EDI
;----------------------------------------------------------------------------
XTIDE_RevText		db	'1.0 '

BeginProc XTIDE_CopyStr

xcs_loop:
	mov	al, [edx]
	mov	[edi], al
	inc	edx
	inc	edi
	dec	ecx
	jnz	xcs_loop
	ret

EndProc XTIDE_CopyStr

;----------------------------------------------------------------------------
; XTIDE_StashGeometry / XTIDE_SelectUnit - move geometry between the working
; globals and the per-unit arrays.
;
;   In:   EAX = unit index (0 master, 1 slave)
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_StashGeometry

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

EndProc XTIDE_StashGeometry

BeginProc XTIDE_SelectUnit

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

EndProc XTIDE_SelectUnit

;----------------------------------------------------------------------------
; XTIDE_WantDcb - PUBLIC. Should we insert our request routine into this DCB's
;                 calldown list?
;
;   In:   ESI -> DCB
;   Out:  EAX = 0 yes (and the claim is consumed), 1 no
;   Uses: EAX, ECX
;----------------------------------------------------------------------------
BeginProc XTIDE_WantDcb

ifdef XT_REQ_MARKER
	inc	XTIDE_MkCfg
endif
	or	esi, esi
	jz	xwd_no

;  Ours is decided by the UNIT, not by the DCB's address - see XTIDE_ClaimedDcb.
	movzx	eax, byte ptr [esi].DCB_unit_on_ctl
	cmp	eax, 1
	ja	xwd_no
	mov	ecx, eax
	shl	ecx, 2

	cmp	XTIDE_ClaimedDcb[ecx], 0	; did inquiry claim this unit?
	je	xwd_no
	cmp	XTIDE_InsertedDcb[ecx], 0	; already in this unit's calldown?
	jne	xwd_no

	mov	XTIDE_InsertedDcb[ecx], esi
	xor	eax, eax
	ret

xwd_no:
	mov	eax, 1
	ret

EndProc XTIDE_WantDcb

;----------------------------------------------------------------------------
; XTIDE_ForgetDcb - PUBLIC. A DCB we may have claimed is being destroyed.
;
;   In:   ESI -> the DCB from AEP_d_u_dcb, may be 0
;   Destroys: EAX, ECX
;
;   STORAGE.DOC describes AEP_UNCONFIG_DCB's contract as exactly this: "a layer
;   typically just makes a note of the fact that the DCB is going away". The
;   DDK sample makes no note of anything - it does not handle the message, and
;   its AER answers AEP_FAILURE to every code it does not recognise.
;
;   Clearing InsertedDcb also lets IOS re-offer the same unit later and have
;   XTIDE_WantDcb accept it. That is correct: the guard exists to stop a DOUBLE
;   insert on a live DCB, not to make the claim permanent for the session.
;----------------------------------------------------------------------------
BeginProc XTIDE_ForgetDcb

	or	esi, esi
	jz	xfd_done

;  The published volume. Drop the pointer and release the appy-time latch, so
;  a later remount can queue again instead of finding a stale "in flight".
	cmp	esi, XTIDE_LogDcb
	jne	xfd_phys
	mov	XTIDE_LogDcb, 0
	mov	XTIDE_VolBusy, 0

;  Both physical units. Compared by POINTER here, unlike XTIDE_WantDcb which
;  matches on the unit number - the question is not "is this our device" but
;  "is this the exact object we stored", and only the object is being freed.
xfd_phys:
	xor	ecx, ecx
xfd_scan:
	cmp	XTIDE_InsertedDcb[ecx], esi
	jne	xfd_next
	mov	XTIDE_InsertedDcb[ecx], 0
xfd_next:
	add	ecx, 4
	cmp	ecx, 8
	jb	xfd_scan

xfd_done:
	ret

EndProc XTIDE_ForgetDcb

;----------------------------------------------------------------------------
; XTIDE_Uninit - PUBLIC. Actually release what AEP_INITIALIZE acquired.
;
;   AEP_UNINITIALIZE is a COMMAND, not a notification. The DDK sample answers it
;   AEP_SUCCESS from its catch-all and does nothing, so the driver reports it has
;   released everything while still holding the DDB created by ISP_CREATE_DDB at
;   init - measured 2026-09-04: IOS sends 15 (UNINITIALIZE) then 14 (SHUTDOWN),
;   and Windows then never finishes shutting down.
;
;   The dealloc sequence is the sample's own, from Port_initialize's failure path.
;   Destroys: nothing (pushad/popad).
;----------------------------------------------------------------------------
	public XTIDE_Uninit
BeginProc XTIDE_Uninit

	pushad
	mov	edi, XTIDE_OurDdb
	or	edi, edi
	jz	xui_done

	sub	esp, size ISP_ddb_dealloc
	mov	eax, esp
	mov	[eax].ISP_ddb_d_ddb, edi
	mov	ecx, [port_ilb].ILB_dvt
	mov	[eax].ISP_ddb_d_dvt, ecx
	mov	[eax].ISP_func, ISP_DEALLOC_DDB
	mov	[eax].ISP_result, 0
	push	eax
	call	[port_ilb.ILB_Service_rtn]
	add	esp, size ISP_ddb_dealloc + 4

	mov	XTIDE_OurDdb, 0

xui_done:
	popad
	ret

EndProc XTIDE_Uninit

;----------------------------------------------------------------------------
; XTIDE_ShutdownArm - PUBLIC. Arm the shutdown fast-fail. Destroys: nothing
;
;   Called from the AEP_SYSTEM_SHUTDOWN branch of Port_Async_Request and from
;   nowhere else - that single call site IS the gate. Until it runs, both wait
;   loops use the full XT_SPIN and the driver behaves exactly as before, which
;   is what a live Windows session needs: a CF card doing an internal erase can
;   be legitimately busy for hundreds of ms, and failing that write would be
;   worse than waiting for it.
;
;   It does NOT shorten the first wait. The flush still gets the full limit;
;   only after one full-length expiry - the drive has stopped answering - does
;   the clamp take effect, and any later success lifts it again.
;----------------------------------------------------------------------------
	public XTIDE_ShutdownArm
; XTIDE_DbgAep - PUBLIC. Report one AEP function code the instant it arrives.
;
;   In:  SI = AEP_Func.  Destroys: nothing (pushad/popad).
;
;   Emitted through the emulator debug port rather than the on-disk marker
;   because the question is which AEPs we get AT SHUTDOWN, when writing to the
;   disk is exactly what we must not do. Tag 10h.
;
;   The counters at AEP_SYSTEM_SHUTDOWN reported nothing on 2026-09-04, which
;   only proves code 14 never arrived - it says nothing about what DID. This
;   answers that, with ordering.
;----------------------------------------------------------------------------
	public XTIDE_DbgAep
BeginProc XTIDE_DbgAep

	pushad
	movzx	edx, si
	mov	al, 10h
	call	XTIDE_DbgEmit
	popad
	ret

EndProc XTIDE_DbgAep

;----------------------------------------------------------------------------
XTIDE_DbgEmit:
;  EAX = tag, EDX = value. Writes tag to 0E0h then the dword LSB-first to 0E1h.
;  Destroys nothing the callers need; EAX/ECX/EDX only.
	push	ecx
	push	edx
	push	eax
	mov	dx, 0e0h
	out	dx, al
	pop	eax
	pop	edx
	mov	ecx, 4
xde_loop:
	push	edx
	mov	al, dl
	mov	dx, 0e1h
	out	dx, al
	pop	edx
	shr	edx, 8
	dec	ecx
	jnz	xde_loop
	pop	ecx
	ret

BeginProc XTIDE_ShutdownArm

	mov	XTIDE_ShutDown, 1

;  Report the completion ladder at exactly the moment the hang begins. If
;  ACCEPTED does not equal DONE, an IOP went into this driver and never came
;  back out - which is what IFSMGR would be waiting on.
	mov	al, 01h
	mov	edx, XTIDE_DbgOffered
	call	XTIDE_DbgEmit
	mov	al, 02h
	mov	edx, XTIDE_DbgAccepted
	call	XTIDE_DbgEmit
	mov	al, 03h
	mov	edx, XTIDE_DbgStarted
	call	XTIDE_DbgEmit
	mov	al, 04h
	mov	edx, XTIDE_DbgDone
	call	XTIDE_DbgEmit

	ret

EndProc XTIDE_ShutdownArm

;----------------------------------------------------------------------------
; XTIDE_VolDown - PUBLIC. Take our published logical volume back down.
;
;   Called from AEP_SYSTEM_SHUTDOWN. Destroys: EAX, EBX, ECX, EDI
;
;   Deliberately FORCED - no ISP_QUERY_REMOVE_DCB veto first. cfu1-win9x offers
;   both, and the veto is right for a user-initiated eject: it lets IFSMGR
;   refuse while files are open. At system shutdown there is nothing useful to
;   do with a refusal, and honouring one would leave exactly the state this
;   routine exists to clear.
;----------------------------------------------------------------------------
BeginProc XTIDE_VolDown

	mov	eax, XTIDE_LogDcb
	or	eax, eax
	jz	xvd_done

	sub	esp, size ISP_DCB_DESTROY
	mov	edi, esp
	mov	[edi].ISP_func, ISP_DESTROY_DCB
	mov	[edi].ISP_result, 0
	mov	[edi].ISP_dcb_d_dcb, eax
	push	edi
	call	[port_ilb.ILB_Service_rtn]
	add	esp, 4
	add	esp, size ISP_DCB_DESTROY

;  EAX still holds the DCB IOS has now freed. Unlink it from whichever
;  physical DCB points at it, or the chain is left dangling. Matched by
;  POINTER, not by unit: the question is which chain holds this exact object.
	xor	ecx, ecx
xvd_scan:
	mov	ebx, XTIDE_InsertedDcb[ecx]
	or	ebx, ebx
	jz	xvd_next
	cmp	[ebx].DCB_next_logical_dcb, eax
	jne	xvd_next
	mov	[ebx].DCB_next_logical_dcb, 0
xvd_next:
	add	ecx, 4
	cmp	ecx, 8
	jb	xvd_scan

	mov	XTIDE_LogDcb, 0

xvd_done:
	mov	XTIDE_VolBusy, 0
	ret

EndProc XTIDE_VolDown



;----------------------------------------------------------------------------
; XTIDE_Inquiry - PUBLIC. Does a device exist at this unit, and if so describe
;                 it in the DCB.
;
;   In:   EAX = unit number, ESI -> DCB (both already live at the call site)
;   Out:  EAX = 0 present, 1 absent
;   Uses: EAX, ECX, EDX, ESI
;----------------------------------------------------------------------------
BeginProc XTIDE_Inquiry

	push	ebx
	push	edi
	push	eax			; unit index, read back at [esp+8]

	cmp	eax, 1
	ja	xin_absent

;  Claim only what this build is configured to claim.
	mov	ecx, 1
	push	eax
	mov	edx, eax
	and	edx, 1Fh
xin_shift:
	or	edx, edx
	jz	xin_shifted
	shl	ecx, 1
	dec	edx
	jmp	xin_shift
xin_shifted:
	test	ecx, XT_CLAIM_MASK
	pop	eax
	jz	xin_absent

	mov	ecx, eax
	shl	ecx, 2
	cmp	XTIDE_UnitPresent[ecx], 0
	je	xin_absent

	mov	XTIDE_ClaimedDcb[ecx], esi	; remember it for Port_cfg_device
	mov	XTIDE_OurUnit, eax	; and which unit to write the marker to

	call	XTIDE_SelectUnit		; geometry for THIS unit

ifdef XT_NO_DCB
	jmp	xin_present		; BISECT: claim the unit, describe nothing
endif

;  What the IOS actually asks for here: who is this unit? The three strings
;  below are real DCB fields. Geometry is NOT this function's job - see the
;  note further down about DCB_BLOCKDEV.
	mov	edx, eax			; unit index - XTIDE_SelectUnit preserves EAX.
					; This read WAS [esp+8], which is the saved EBX:
					; EBX*40 as an array index, then copied from it.
					; Do not reintroduce stack-offset arithmetic here.
	imul	edx, edx, 40
	add	edx, OFFSET32 XTIDE_UnitModel

	lea	edi, [esi].DCB_vendor_id
	mov	ecx, 8
	call	XTIDE_CopyStr
	lea	edi, [esi].DCB_product_id
	mov	ecx, 16
	call	XTIDE_CopyStr
	lea	edi, [esi].DCB_rev_level
	mov	ecx, 4
	mov	edx, OFFSET32 XTIDE_RevText
	call	XTIDE_CopyStr

;  Geometry does NOT go here. STORAGE.DOC: the driver sets DCB_product_id,
;  DCB_vendor_id and DCB_rev_level at inquiry, and "processes the
;  AEP_CONFIG_DCB function by setting the various members of the DCB structure
;  to values specifying geometry information, such as cylinder count and
;  sector count", after which "the IOS creates a real DCB and fills it with
;  information supplied by the driver". Written here it describes a DCB that
;  is not the one IOS keeps - so the disk TSD sees a zero-length device,
;  declines it, and no request is ever issued. See XTIDE_ConfigDcb.

xin_present:
	xor	eax, eax			; present
	pop	edx			; discard the saved unit index
	pop	edi
	pop	ebx
	ret

xin_absent:
	mov	eax, 1
	pop	edx
	pop	edi
	pop	ebx
	ret

EndProc XTIDE_Inquiry

;----------------------------------------------------------------------------
; XTIDE_NoteIsp - PUBLIC. Stash the ISP_result Port_cfg_device read back off
;                 the stack. In: EAX.
;----------------------------------------------------------------------------
BeginProc XTIDE_NoteIsp

ifdef XT_REQ_MARKER
	MARK	XTIDE_MkIsp, eax
endif
	ret

EndProc XTIDE_NoteIsp

;----------------------------------------------------------------------------
; XTIDE_VolCreate - PUBLIC. Be our own TSD: publish the volume ourselves.
;
;   In:   nothing; uses the physical DCB and DDB recorded during registration
;   Out:  EAX = 0 a drive letter was assigned, 1 otherwise
;
;   Windows' own disk TSD never engages a DYNAMICALLY registered port driver's
;   DCB. We are dynamically registered by necessity - without DRP_FC_DYNALOAD
;   the image does not load at all - so IOS runs our AEP_CONFIG_DCB and nobody
;   else's, and no layer above us ever parses a partition table. Measured
;   2026-09-01 on a freshly formatted FAT16 volume: exactly three requests,
;   LBA 0 then LBA 63 twice, and then silence. The transport was never at
;   fault; there was simply nothing above it.
;
;   So do the TSD's job: read the MBR, create a logical DCB for partition 1,
;   give it its own calldown entry, and associate it with a drive letter.
;
;   The recipe, the ordering and the reason are Nick's (@zikolas), from
;   cfu1-win9x's vol_create - MIT licensed. Notably his warning that the
;   logical DCB must have its OWN calldown node: ISP_DCB_DESTROY frees a DCB's
;   nodes, so a shared chain leaves the physical DCB pointing at freed memory.
;----------------------------------------------------------------------------
BeginProc XTIDE_VolCreate

	push	ebx
	push	esi
	push	edi

	MARK	XTIDE_MkVolStep, 1
	cmp	port_ilb, 0
	je	xvc_fail
	MARK	XTIDE_MkVolStep, 2
	cmp	XTIDE_LogDcb, 0
	jne	xvc_fail		; already published

	mov	esi, XTIDE_OurUnit
	mov	esi, XTIDE_InsertedDcb[esi*4]
	or	esi, esi
	jz	xvc_fail		; no physical DCB of ours

;  1) the partition table
	mov	eax, XTIDE_OurUnit
	call	XTIDE_SelectUnit
	mov	XTIDE_ReqLba, 0
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_MbrBuf
	MARK	XTIDE_MkVolStep, 4
	call	XTIDE_ReadSectors
	jc	xvc_fail
	MARK	XTIDE_MkVolStep, 5
	cmp	word ptr XTIDE_MbrBuf+510, 0AA55h
	jne	xvc_fail

	mov	eax, dword ptr XTIDE_MbrBuf+1BEh+8
	or	eax, eax
	jz	xvc_fail
	MARK	XTIDE_MkVolStep, 6
	mov	XTIDE_PartStart, eax
	mov	eax, dword ptr XTIDE_MbrBuf+1BEh+12
	or	eax, eax
	jz	xvc_fail
	MARK	XTIDE_MkVolStep, 7
	mov	XTIDE_PartLen, eax

;  2) a logical DCB
	sub	esp, size ISP_dcb_create
	mov	edi, esp
	mov	[edi].ISP_func, ISP_CREATE_DCB
	mov	[edi].ISP_result, 0
	mov	[edi].ISP_dcb_size, size DCB
	mov	[edi].ISP_dcb_ptr, 0
	push	edi
	call	[port_ilb.ILB_Service_rtn]
	add	esp, 4
	movzx	eax, [edi].ISP_result
	MARK	XTIDE_MkVolCreate, eax
	mov	ebx, [edi].ISP_dcb_ptr
	add	esp, size ISP_dcb_create
	or	eax, eax
	jnz	xvc_fail
	or	ebx, ebx
	jz	xvc_fail
	MARK	XTIDE_MkVolStep, 8
	mov	XTIDE_LogDcb, ebx

;  3) describe it, and link it to the physical DCB both ways
	mov	[ebx].DCB_physical_dcb, esi
	mov	[esi].DCB_next_logical_dcb, ebx

	sub	esp, size ISP_calldown_insert
	mov	edi, esp
	mov	[edi].ISP_func, ISP_INSERT_CALLDOWN
	mov	[edi].ISP_result, 0
	mov	[edi].ISP_i_cd_dcb, ebx
	mov	[edi].ISP_i_cd_req, OFFSET32 Port_Request
	mov	eax, XTIDE_OurDdb
	mov	[edi].ISP_i_cd_ddb, eax
	mov	[edi].ISP_i_cd_expan_len, 0
	mov	[edi].ISP_i_cd_flags, DCB_dmd_small_memory
	mov	eax, XTIDE_OurLgn
	mov	[edi].ISP_i_cd_lgn, al
	push	edi
	call	[port_ilb.ILB_Service_rtn]
	add	esp, 4
	movzx	eax, [edi].ISP_result
	MARK	XTIDE_MkVolCd, eax
	add	esp, size ISP_calldown_insert
	or	eax, eax
	jnz	xvc_fail

	mov	[ebx].DCB_dmd_flags, DCB_dmd_small_memory
	mov	[ebx].DCB_device_flags, DCB_DEV_LOGICAL or DCB_DEV_WRITEABLE
	mov	eax, XTIDE_PartStart
	mov	[ebx].DCB_Partition_Start, eax
	mov	byte ptr [ebx].DCB_apparent_blk_shift, 9
	mov	[ebx].DCB_sig, 'CB'
	mov	byte ptr [ebx].DCB_device_type, DCB_type_disk
	mov	[ebx].DCB_max_xfer_len, 1000h	; 4 KB - keep the PIO bursts short
	mov	eax, XTIDE_PartLen
	mov	[ebx].DCB_actual_sector_cnt, eax
	mov	dword ptr [ebx].DCB_actual_sector_cnt+4, 0
	mov	[ebx].DCB_actual_blk_size, 512
	mov	eax, XTIDE_Heads
	mov	[ebx].DCB_actual_head_cnt, eax
	mov	eax, XTIDE_Spt
	mov	[ebx].DCB_actual_spt, eax
	mov	eax, XTIDE_Heads
	imul	eax, XTIDE_Spt
	mov	ecx, eax
	mov	eax, XTIDE_PartLen
	xor	edx, edx
	div	ecx
	mov	[ebx].DCB_actual_cyl_cnt, eax

;  4) a drive letter. 0=A, so 3=D - the first one a single-partition boot
;     disk leaves free. Walk up if it is taken.
	MARK	XTIDE_MkVolStep, 9
	mov	XTIDE_VolDrive, 3
xvc_assoc:
	sub	esp, size ISP_dcb_associate
	mov	edi, esp
	mov	[edi].ISP_func, ISP_ASSOCIATE_DCB
	mov	[edi].ISP_result, 0
	mov	[edi].ISP_d_a_dcb, ebx
	mov	eax, XTIDE_VolDrive
	mov	[edi].ISP_d_a_drive, al
	mov	[edi].ISP_d_a_flags, 0
	push	edi
	call	[port_ilb.ILB_Service_rtn]
	add	esp, 4
	movzx	eax, [edi].ISP_result
	add	esp, size ISP_dcb_associate
	MARK	XTIDE_MkVolAssoc, eax
	or	eax, eax
	jz	xvc_ok
	inc	XTIDE_VolDrive
	cmp	XTIDE_VolDrive, 10
	jb	xvc_assoc
	jmp	xvc_fail

xvc_ok:
	MARK	XTIDE_MkVolStep, 10
	xor	eax, eax
	pop	edi
	pop	esi
	pop	ebx
	ret

xvc_fail:
	mov	eax, 1
	pop	edi
	pop	esi
	pop	ebx
	ret

EndProc XTIDE_VolCreate

;----------------------------------------------------------------------------
; XTIDE_NoteLgn - PUBLIC. Remember the load group number. In: EAX.
;----------------------------------------------------------------------------
BeginProc XTIDE_NoteLgn

	mov	XTIDE_OurLgn, eax
	ret

EndProc XTIDE_NoteLgn

;----------------------------------------------------------------------------
; XTIDE_VolUp / XTIDE_SchedVol - publish the volume AT APPY TIME.
;
;   The ISP volume services are appy-time only. Called from an AEP handler
;   they succeed - drive D: really is created, every ISP_result 0 - and then
;   kill the boot, because AEP_CONFIG_DCB runs inside IOS_Register.
;   CAAFL_RING0 lets the callback run before the GUI is up, which is the only
;   reason this works during boot at all.
;
;   Mechanism copied from zikolas/cfu1-win9x sched_volup (MIT).
;----------------------------------------------------------------------------
BeginProc XTIDE_VolUp

	call	XTIDE_VolCreate
	mov	XTIDE_VolBusy, 0
	ret

EndProc XTIDE_VolUp

BeginProc XTIDE_SchedVol

	cmp	XTIDE_VolBusy, 0
	jne	xsv_done
	mov	XTIDE_VolBusy, 1

	push	0		; ulTimeout
	push	1		; CAAFL_RING0
	push	0		; dwRefData
	push	OFFSET32 XTIDE_VolUp
	VxDCall	_SHELL_CallAtAppyTime
	add	esp, 16
	MARK	XTIDE_MkSchedRc, eax
	or	eax, eax
	jnz	xsv_done
	mov	XTIDE_VolBusy, 0	; could not queue

xsv_done:
	ret

EndProc XTIDE_SchedVol

;----------------------------------------------------------------------------
; XTIDE_NoteDdb - PUBLIC. Remember our own DDB. In: EAX = AEP_ddb.
;----------------------------------------------------------------------------
BeginProc XTIDE_NoteDdb

	mov	XTIDE_OurDdb, eax
	ret

EndProc XTIDE_NoteDdb

;----------------------------------------------------------------------------
; XTIDE_MarkStage - PUBLIC. Flush the marker with a progress number.
;
;   In:   EAX = stage
;   Uses: EAX, ECX, EDX
;
;   A disk write is the only channel out of this driver, so anything that
;   happens after the last write is invisible - which is how "requests refused
;   0" came to be reported by a sector written before the refusal was decided.
;   The fix is not a better-placed single write, it is a write at EVERY
;   milestone: whatever the last stage on disk is, the fault is in the code
;   between it and the next one.
;
;   Stages: 2 passed the function check   3 the DCB is ours
;           5 enqueued                    6 about to dequeue
;           7 about to drive the hardware  9 refusing the request
;----------------------------------------------------------------------------
BeginProc XTIDE_MarkStage

ifdef XT_REQ_MARKER
	push	ebx
	push	esi
	push	edi

	MARK	XTIDE_MkStage, eax
	mov	eax, XTIDE_OurUnit
	call	XTIDE_SelectUnit

	mov	XTIDE_ReqLba, XTIDE_MARK_LBA
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_Marker
	call	XTIDE_WriteSectors

	pop	edi
	pop	esi
	pop	ebx
endif
	ret

EndProc XTIDE_MarkStage

;----------------------------------------------------------------------------
; XTIDE_WantIop - PUBLIC. Is this request for a device we actually claimed?
;
;   In:   EBX -> the IOP
;   Out:  EAX = 0 service it, 1 refuse it; ESI preserved
;   Uses: EAX
;
;   Ownership is the DDB on the calldown entry IOS reached us through, not the
;   DCB pointer. Measured 2026-09-01: the request carried a DCB we never
;   inserted on, but an IOP has TWO DCBs - IOP_physical_dcb and
;   IOP_original_dcb, the physical and logical views of one device - so
;   comparing against the one we happened to store proves nothing. The
;   calldown entry is unambiguous: IOS only routes an IOP down a calldown
;   entry it built for us, so its DCB_cd_ddb is our DDB exactly when the
;   request is ours.
;
;   Measured 2026-09-01: with the calldown inserted on exactly one DCB, a
;   request arrived carrying a DIFFERENT DCB - unit 0, the boot disk. Never
;   drive the hardware for a DCB we did not insert on, whatever the reason
;   turns out to be. Refusing costs a failed request; obeying costs the
;   volume.
;----------------------------------------------------------------------------
BeginProc XTIDE_WantIop

ifndef XT_NO_DBGCOUNT
	inc	XTIDE_DbgOffered
endif
	push	esi
	mov	esi, [ebx].IOP_calldown_ptr
	or	esi, esi
	jz	xwi_no
	mov	esi, [esi].DCB_cd_ddb

ifdef XT_REQ_MARKER
	MARK	XTIDE_MkCdDdb, esi
	mov	eax, [ebx].IOP_original_dcb
	MARK	XTIDE_MkOrigDcb, eax
endif

	cmp	esi, XTIDE_OurDdb
	jne	xwi_no

ifndef XT_NO_DBGCOUNT
	inc	XTIDE_DbgAccepted
endif
	pop	esi
	xor	eax, eax
	ret

xwi_no:
ifdef XT_REQ_MARKER
	inc	XTIDE_MkRefused
endif
	pop	esi
	mov	eax, 1
	ret

EndProc XTIDE_WantIop

;----------------------------------------------------------------------------
; XTIDE_MarkConfig - PUBLIC. Report what happened in Port_cfg_device.
;
;   In:   ESI -> the DCB this AEP_CONFIG_DCB was about (may be 0)
;
;   Addresses the marker at OUR claimed unit, never at the unit named in the
;   DCB - a DCB we declined belongs to somebody else and its unit number would
;   aim this write at the boot disk (technique 79).
;   Compiles to a bare RET without XT_REQ_MARKER.
;----------------------------------------------------------------------------
BeginProc XTIDE_MarkConfig

ifdef XT_REQ_MARKER
	push	ebx
	push	esi
	push	edi

	or	esi, esi
	jz	xmc_addr
	movzx	eax, byte ptr [esi].DCB_unit_on_ctl
	MARK	XTIDE_MkUnit, eax

xmc_addr:
	mov	eax, XTIDE_OurUnit
	call	XTIDE_SelectUnit

	mov	XTIDE_ReqLba, XTIDE_MARK_LBA
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_Marker
	call	XTIDE_WriteSectors

	pop	edi
	pop	esi
	pop	ebx
endif
	ret

EndProc XTIDE_MarkConfig

;----------------------------------------------------------------------------
; XTIDE_MarkEntry - PUBLIC. Record that IOS called our calldown routine at all.
;
;   In:   EBX -> IOP
;
;   Same sector as XTIDE_MarkRequest, a different counter. Port_request runs
;   before XTIDE_StartRequest, so reading both counters afterwards says which
;   of the two the request got as far as - one run, not two.
;   Compiles to a bare RET without XT_REQ_MARKER.
;----------------------------------------------------------------------------
BeginProc XTIDE_MarkEntry

ifdef XT_REQ_MARKER
	push	ebx
	push	esi
	push	edi

	inc	XTIDE_MkEntry

;  Copy the first 32 bytes of whatever we were handed, before interpreting any
;  of it. Every field read so far has been read THROUGH the assumption that
;  this is an IOP; if that assumption is wrong every one of them is fiction.
;  Raw bytes are the only reading that does not beg the question.
	MARK	XTIDE_MkIop, ebx
	mov	esi, ebx
	mov	edi, OFFSET32 XTIDE_MkRaw
	mov	ecx, 8
xme_copy:
	mov	eax, [esi]
	mov	[edi], eax
	add	esi, 4
	add	edi, 4
	dec	ecx
	jnz	xme_copy
	mov	eax, XTIDE_MkEntry
	cmp	eax, 1
	je	xme_write
	test	eax, 63
	jnz	xme_skip

xme_write:
;  Record the request's DCB and the one we inserted on, so the two can be
;  compared from the host. Both are data - neither may steer the write.
	mov	esi, [ebx].IOP_physical_dcb
	MARK	XTIDE_MkIopDcb, esi
	mov	eax, XTIDE_OurUnit
	mov	eax, XTIDE_InsertedDcb[eax*4]
	MARK	XTIDE_MkOurDcb, eax

	or	esi, esi
	jz	xme_addr
	movzx	eax, byte ptr [esi].DCB_unit_on_ctl
	cmp	eax, 1
	ja	xme_addr
	MARK	XTIDE_MkUnit, eax

;  ADDRESS AT OUR OWN UNIT, ALWAYS. Selecting the unit named in the request's
;  DCB is how this diagnostic wrote to the boot disk on 2026-09-01 - the DCB
;  belonged to somebody else and its unit number aimed the write at their
;  drive. Same rule as XTIDE_MarkConfig; it was not applied here.
xme_addr:
	mov	eax, XTIDE_OurUnit
	call	XTIDE_SelectUnit

	mov	XTIDE_ReqLba, XTIDE_MARK_LBA
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_Marker
	call	XTIDE_WriteSectors

xme_skip:
	pop	edi
	pop	esi
	pop	ebx
endif
	ret

EndProc XTIDE_MarkEntry

;----------------------------------------------------------------------------
; XTIDE_MarkRequest - record that a real IOS request arrived, on the disk.
;
;   In:   EBX -> IOP, ESI -> DCB, unit already selected by the caller
;   Uses: nothing the caller needs
;
;   Written on the first request and every 64th after it, so the on-disk copy
;   also reports roughly how many arrived without a write per request.
;   LBA 16000 is past the end of anything on the 8 MB scratch slave (16128
;   sectors) that a filesystem would use, and the scratch disk is disposable.
;----------------------------------------------------------------------------
ifndef XT_MARK_LBA
XT_MARK_LBA	equ	16000
endif
XTIDE_MARK_LBA	equ	XT_MARK_LBA

BeginProc XTIDE_MarkRequest
ifdef XT_REQ_MARKER

	push	ebx
	push	esi
	push	edi

	inc	XTIDE_MkCount

	movzx	eax, [ebx].IOP_ior.IOR_func
	MARK	XTIDE_MkFunc, eax
	mov	eax, dword ptr [ebx].IOP_ior.IOR_start_addr
	MARK	XTIDE_MkLba, eax

;  Keep the FIRST eight, not the last eight. The mount sequence is at the
;  start; whatever a running system does afterwards is noise by comparison.
	mov	eax, XTIDE_MkCount
	cmp	eax, 8
	ja	xmr_nostore
	dec	eax
	shl	eax, 3
	mov	ecx, XTIDE_MkFunc
	mov	XTIDE_MkRing[eax], ecx
	mov	ecx, XTIDE_MkLba
	mov	XTIDE_MkRing[eax+4], ecx

xmr_nostore:
	mov	eax, XTIDE_MkCount
	cmp	eax, 8
	jbe	xmr_write
	test	eax, 7
	jnz	xmr_skip

xmr_write:
	mov	eax, [ebx].IOP_ior.IOR_xfer_count
	MARK	XTIDE_MkXfer, eax
	movzx	eax, byte ptr [esi].DCB_unit_on_ctl
	MARK	XTIDE_MkUnit, eax

	mov	XTIDE_ReqLba, XTIDE_MARK_LBA
	mov	XTIDE_ReqCount, 1
	mov	XTIDE_ReqBuf, OFFSET32 XTIDE_Marker
	call	XTIDE_WriteSectors

xmr_skip:
	pop	edi
	pop	esi
	pop	ebx
endif
	ret

EndProc XTIDE_MarkRequest

;----------------------------------------------------------------------------
; XTIDE_MarkWrite - snapshot the first WRITE the request path ever receives.
;
;   In:   EBX -> IOP, XTIDE_ReqBuf already loaded from IOR_buffer_ptr
;   Uses: EAX, ECX, EDX  (EBX, ESI, EDI preserved - the caller still needs all
;         three, and a helper eating them is the bug class technique 78 exists
;         for)
;
;   Deliberately raw. The write that motivated this put ring-0 heap on the
;   disk, which means the pointer is not what the code assumed it was - so
;   copy the bytes and the flags out and interpret them on the host, rather
;   than reading further fields through the same broken assumption.
;----------------------------------------------------------------------------
BeginProc XTIDE_MarkWrite
ifdef XT_REQ_MARKER

	push	ebx
	push	esi
	push	edi

	mov	eax, [ebx].IOP_ior.IOR_flags
	MARK	XTIDE_MkWrFlags, eax
	mov	eax, [ebx].IOP_ior.IOR_sgd_lin_phys
	MARK	XTIDE_MkWrSgd, eax
	movzx	eax, byte ptr [ebx].IOP_ior.IOR_num_sgds
	MARK	XTIDE_MkWrNsgd, eax
	mov	eax, dword ptr [ebx].IOP_ior.IOR_start_addr
	MARK	XTIDE_MkWrLba, eax
	mov	eax, [ebx].IOP_ior.IOR_xfer_count
	MARK	XTIDE_MkWrCnt, eax

	mov	eax, XTIDE_ReqBuf
	mov	esi, eax
	mov	edi, OFFSET32 XTIDE_MkWrData
	mov	ecx, 32
xmw_copy:
	mov	dl, [esi]
	mov	[edi], dl
	inc	esi
	inc	edi
	dec	ecx
	jnz	xmw_copy

;  LAST, so the "first write only" test in XTIDE_StartRequest stays false
;  until every other field above it is filled in.
	MARK	XTIDE_MkWrBuf, eax

	pop	edi
	pop	esi
	pop	ebx
endif
	ret

EndProc XTIDE_MarkWrite

;----------------------------------------------------------------------------
; XTIDE_ConfigDcb - PUBLIC. Describe the device in the DCB the IOS will keep.
;
;   In:   ESI -> DCB
;   Out:  ESI preserved
;   Uses: EAX, ECX, EDX
;
;   Called from Port_cfg_device on AEP_CONFIG_DCB, which is where STORAGE.DOC
;   puts geometry. The unit comes from the DCB itself rather than from
;   anything remembered at inquiry time, so a DCB arriving out of order still
;   gets its own drive's numbers.
;----------------------------------------------------------------------------
BeginProc XTIDE_ConfigDcb

	push	ebx
ifdef XT_REQ_MARKER
	inc	XTIDE_MkWant
endif
	movzx	eax, byte ptr [esi].DCB_unit_on_ctl
	cmp	eax, 1
	ja	xcd_done
	call	XTIDE_SelectUnit	; geometry for THIS unit

	mov	byte ptr [esi].DCB_device_type, DCB_type_disk
	mov	byte ptr [esi].DCB_bus_type, DCB_BUS_ESDI
	mov	byte ptr [esi].DCB_apparent_blk_shift, 9
	or	[esi].DCB_device_flags, DCB_DEV_PHYSICAL or DCB_DEV_WRITEABLE
	mov	[esi].DCB_actual_blk_size, 512
	mov	[esi].DCB_max_xfer_len, 65536	; one chunk, 128 sectors

;  A CHS-only drive reports no LBA total, so derive one - IOS wants a sector
;  count whatever the drive can address. Leave it zero and the TSD declines
;  the device.
	mov	eax, XTIDE_TotalSec
	or	eax, eax
	jnz	xcd_havetotal
	mov	eax, XTIDE_Cyls
	imul	eax, XTIDE_Heads
	imul	eax, XTIDE_Spt
xcd_havetotal:
	mov	[esi].DCB_actual_sector_cnt, eax
	mov	dword ptr [esi].DCB_actual_sector_cnt+4, 0

	mov	eax, XTIDE_Heads
	mov	[esi].DCB_actual_head_cnt, eax
	mov	eax, XTIDE_Cyls
	mov	[esi].DCB_actual_cyl_cnt, eax
	mov	eax, XTIDE_Spt
	mov	[esi].DCB_actual_spt, eax

ifdef XT_REQ_MARKER
	mov	eax, XTIDE_Cyls
	MARK	XTIDE_MkCyls, eax
	mov	eax, XTIDE_Heads
	MARK	XTIDE_MkHeads, eax
	mov	eax, XTIDE_Spt
	MARK	XTIDE_MkSpt, eax
	mov	eax, [esi].DCB_actual_sector_cnt
	MARK	XTIDE_MkTotal, eax
	mov	eax, [esi].DCB_device_flags
	MARK	XTIDE_MkDevFlags, eax
endif

xcd_done:
	pop	ebx
	ret

EndProc XTIDE_ConfigDcb

;----------------------------------------------------------------------------
; XTIDE_StartRequest - PUBLIC. Service one IOP to completion, synchronously.
;
;   In:   EBX -> IOP, ESI -> DCB (both already live at the call site)
;   Uses: everything bar EBP
;
;   Synchronous on purpose. The card is jumpered without an interrupt, so there
;   is no completion ISR to come back through; the transfer happens here and
;   the IOP is completed before returning. That is what a polled port driver
;   does, and it is why PORTISR.ASM is not in this path at all.
;----------------------------------------------------------------------------
BeginProc XTIDE_StartRequest

ifndef XT_NO_DBGCOUNT
	inc	XTIDE_DbgStarted
endif

	push	ebx
	push	esi
	push	edi

ifdef XT_NO_IO
	jmp	xsr_ioerror	; BISECT: complete every request as an error,
				; touching neither hardware nor geometry. Splits
				; a bad ISP packet from a bad request path.
endif
	movzx	eax, byte ptr [esi].DCB_unit_on_ctl
	mov	XTIDE_ReqUnit, eax
	call	XTIDE_SelectUnit

ifdef XT_REQ_MARKER
	call	XTIDE_MarkRequest	; leave proof on the disk
endif

	movzx	eax, [ebx].IOP_ior.IOR_func
	cmp	ax, IOR_VERIFY
	je	xsr_ok		; verify does not have to move data
	cmp	ax, IOR_READ
	je	xsr_io
	cmp	ax, IOR_WRITE
	je	xsr_io
	mov	word ptr [ebx].IOP_ior.IOR_status, IORS_INVALID_COMMAND
	jmp	xsr_complete

xsr_io:
ifdef XT_READ_ONLY
;  READ-ONLY BUILD. Refuse every write at the door, before any addressing is
;  computed, so no code path exists that can put a byte on the disk. This is
;  for claiming a volume on a machine whose only disk is the one it boots from:
;  reads can be wrong and cost nothing but a reboot, writes can be wrong and
;  cost the volume. Removing the capability beats trusting the addressing
;  (technique 79 - the round trip cannot see a wrong address).
	movzx	eax, [ebx].IOP_ior.IOR_func
	cmp	ax, IOR_WRITE
	jne	xsr_notwrite
	mov	word ptr [ebx].IOP_ior.IOR_status, IORS_WRITE_PROTECT
	jmp	xsr_complete
xsr_notwrite:
endif
;  A 64-bit start address with anything in the high dword is past LBA28 and
;  past this card, so refuse it rather than truncate it into a wild write.
	cmp	dword ptr [ebx].IOP_ior.IOR_start_addr+4, 0
	jne	xsr_badsector

	mov	eax, dword ptr [ebx].IOP_ior.IOR_start_addr

;  A request on a volume WE published can arrive partition-relative. IOS says so
;  with IORF_LOGICAL_START_SECTOR, and expects the driver to add the partition
;  start itself; IORF_PARTITION_BIAS_ADDED means it has already been applied and
;  must not be added twice. zikolas/cfu1-win9x does exactly this.
;
;  Every request measured on the emulator's SECOND disk arrived absolute, so this
;  path never mattered there - but that volume was one nobody else owned. On a
;  boot volume taken over from the Real Mode Mapper the flag is live, and being
;  wrong about it puts every write 63 sectors from where it belongs, inside a
;  filesystem, every single time.
	test	dword ptr [ebx].IOP_ior.IOR_flags, IORF_LOGICAL_START_SECTOR
	jz	xsr_absolute
	test	dword ptr [ebx].IOP_ior.IOR_flags, IORF_PARTITION_BIAS_ADDED
	jnz	xsr_absolute
	add	eax, XTIDE_PartStart	; 0 until the volume is published = no bias
xsr_absolute:
	mov	XTIDE_ReqLba, eax
	mov	eax, [ebx].IOP_ior.IOR_xfer_count
	mov	XTIDE_SgLeft, eax
	mov	XTIDE_ReqIsWrite, 0
	movzx	eax, [ebx].IOP_ior.IOR_func
	cmp	ax, IOR_WRITE
	jne	xsr_dirset
	mov	XTIDE_ReqIsWrite, 1
xsr_dirset:

ifdef XT_REQ_MARKER
	cmp	XTIDE_ReqIsWrite, 0
	je	xsr_nowrsnap
	cmp	XTIDE_MkWrBuf, 0	; the FIRST write only
	jne	xsr_nowrsnap
	mov	eax, [ebx].IOP_ior.IOR_buffer_ptr
	mov	XTIDE_ReqBuf, eax	; raw, whatever it turns out to point at
	call	XTIDE_MarkWrite
xsr_nowrsnap:
endif

;  Bounds check the whole request against the drive's own capacity, before
;  touching hardware. Done on the IOR's own count, so it holds for the
;  scatter/gather path too - the segments only partition that same count.
	mov	eax, XTIDE_TotalSec
	or	eax, eax
	jz	xsr_dispatch		; CHS-only drive, no total to check against
	mov	ecx, XTIDE_ReqLba
	add	ecx, XTIDE_SgLeft
	jc	xsr_badsector
	cmp	ecx, eax
	ja	xsr_badsector

xsr_dispatch:
	test	dword ptr [ebx].IOP_ior.IOR_flags, IORF_SCATTER_GATHER
	jz	xsr_plain
ifdef XT_REQ_MARKER
	cmp	XTIDE_ReqIsWrite, 0
	je	xsr_sgrd
	inc	XTIDE_MkSgWr
	jmp	xsr_sg
xsr_sgrd:
	inc	XTIDE_MkSgRd
endif
	jmp	xsr_sg
xsr_plain:

;  Plain case: one linear buffer for the whole transfer.
	mov	eax, [ebx].IOP_ior.IOR_buffer_ptr
	mov	XTIDE_ReqBuf, eax
	mov	eax, XTIDE_SgLeft
	mov	XTIDE_ReqCount, eax
	call	XTIDE_XferRun
	jc	xsr_ioerror
	jmp	xsr_ok

;  Scatter/gather: IOR_buffer_ptr is a list of {sector count, linear buffer}.
xsr_sg:
	mov	eax, [ebx].IOP_ior.IOR_buffer_ptr
	mov	XTIDE_SgCur, eax
xsr_sgloop:
	cmp	XTIDE_SgLeft, 0
	je	xsr_ok			; the IOR's own count is satisfied
	mov	edx, XTIDE_SgCur
	mov	eax, [edx+4]		; buffer pointer is the SECOND dword
	or	eax, eax
	jz	xsr_ok			; null terminates the list
	mov	XTIDE_ReqBuf, eax
	mov	ecx, [edx]		; sector count is the first
	or	ecx, ecx
	jz	xsr_ok
	cmp	ecx, XTIDE_SgLeft
	jbe	xsr_sghave
	mov	ecx, XTIDE_SgLeft	; clamp the last segment to the bound
xsr_sghave:
	mov	XTIDE_ReqCount, ecx
	push	ecx
	call	XTIDE_XferRun
	pop	ecx
	jc	xsr_ioerror
	add	XTIDE_ReqLba, ecx	; XferRun does not advance past its last
	sub	XTIDE_SgLeft, ecx	; chunk, so step the run on from here
	add	XTIDE_SgCur, 8
	jmp	xsr_sgloop

xsr_ok:
	mov	word ptr [ebx].IOP_ior.IOR_status, IORS_SUCCESS
	jmp	xsr_complete

xsr_badsector:
	mov	word ptr [ebx].IOP_ior.IOR_status, IORS_INVALID_SECTOR
	jmp	xsr_complete

xsr_ioerror:
	mov	word ptr [ebx].IOP_ior.IOR_status, IORS_DEVICE_ERROR

;  Hand the IOP back up the callback stack. Same sequence the sample uses for
;  a command it cannot service.
xsr_complete:
	mov	eax, [ebx].IOP_callback_ptr
	sub	eax, size IOP_CallBack_Entry
	mov	[ebx].IOP_callback_ptr, eax
	push	ebx
	call	dword ptr [eax]
	add	esp, 4
ifndef XT_NO_DBGCOUNT
	inc	XTIDE_DbgDone		; AFTER the callback returns, so a callback
				; that never returns is visible as a gap
endif

	pop	edi
	pop	esi
	pop	ebx
	ret

EndProc XTIDE_StartRequest

;----------------------------------------------------------------------------
; XTIDE_XferRun - move XTIDE_ReqCount sectors between XTIDE_ReqLba and
;                 XTIDE_ReqBuf, in chunks one taskfile command can carry.
;
;   In:   XTIDE_ReqLba / ReqCount / ReqBuf / ReqIsWrite
;   Out:  CF=0 on success. ReqCount is zero; ReqLba and ReqBuf are advanced
;         past every chunk EXCEPT the last, so a caller stepping through a
;         scatter/gather list must add the segment on itself.
;   Uses: EAX, ECX, EDX, ESI, EDI  (EBX preserved - the caller still needs the
;         IOP, and a helper eating a register its caller relies on is the bug
;         this file already paid for once, technique 78)
;
;   The direction comes from a variable rather than the IOP so this routine
;   does not have to hold EBX across the transfer loops.
;----------------------------------------------------------------------------
BeginProc XTIDE_XferRun

	push	ebx
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
	call	XTIDE_WriteSectors
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
	pop	ebx
	stc
	ret

EndProc XTIDE_XferRun

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
	mov	ecx, XT_TIMEBASE
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

ifdef XT_PROBE_SKIP
;  CALIBRATION BUILD. Do no probing at all - just report a KNOWN fail code, so
;  the boot log's gap for a known number of delay units can be compared against
;  a real run's gap directly. That comparison needs no algebra and no estimate
;  of how long a log unit is on this machine, which is the thing that has now
;  been guessed wrong twice. Everything before and after the probe is identical
;  between the two builds, so the overhead cancels.
	mov	XTIDE_FailCode, XT_PROBE_SKIP
	jmp	xp_exit
endif

	movzx	ebx, word ptr [edi].DDB_base_ioa
	or	bx, bx
	jz	xp_nobase

	cmp	XTIDE_Stride, 0		; 0 = -DXT_STRIDE=0, autodetect
	jne	xp_stridefixed
	call	XTIDE_DetectStride
	jc	xp_nostride
	jmp	xp_ports
xp_stridefixed:
	mov	eax, XTIDE_Stride	; report the pinned value too, so the
	MARK	XTIDE_MkStride, eax	; marker always names the map in use
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
ifdef XT_NO_WRITETEST
;  DEFAULT (no -WriteTest): do not go looking for a slave, and therefore never run the
;  write self-test. It writes 512 bytes to LBA 100 of whichever unit answered,
;  and on a machine with one disk that unit is the boot volume. The DEV-bit
;  guard below is the only thing standing between "no slave fitted" and a write
;  to the boot disk - and on a register map that has never executed, that guard
;  has never executed either. Do not spend a card on trusting it (technique 79).
	jmp	xp_noslave
endif
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

xp_nostride:
	mov	XTIDE_FailCode, 11	; neither stride made Status and AltStatus agree
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
