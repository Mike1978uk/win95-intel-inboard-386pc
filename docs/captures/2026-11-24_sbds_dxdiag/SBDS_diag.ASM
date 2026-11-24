;   SBDS.VXD - DirectSound driver (primary buffer only) for the Sound Blaster Pro 2
;   on the 5160 + Inboard 386/PC. Design: docs/sbds_design_2026_11_23.md.
;
;   MSSBLST.VXD stays the wave driver and keeps IRQ 5 and VDMAD's channel 1. This
;   VxD drives the DSP and the 8237 directly from ring 0 while DirectSound holds the
;   wave device open (DSDDESC_DOMMSYSTEMOPEN), and takes the play position from the
;   DMA count, so it needs no interrupt of its own.

	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VXDLDR.Inc
	INCLUDE CONFIGMG.Inc
	INCLUDE VMMREG.Inc
	INCLUDE VMCPD.Inc
	INCLUDE PAGEFILE.Inc
	.LIST

Begin_Service_Table PERF
PERF_Service PERF_Get_Version, LOCAL
PERF_Service PERF_Server_Register, LOCAL
PERF_Service PERF_Server_Deregister, LOCAL
PERF_Service PERF_Server_Add_Stat, LOCAL
PERF_Service PERF_Server_Remove_Stat, LOCAL
End_Service_Table PERF

DSOUND_Device_ID	EQU	357Eh
Begin_Service_Table DSOUND
DSOUND_Service _DSOUND_GetVersion, LOCAL
DSOUND_Service _DSOUND_RegisterDeviceDriver, LOCAL
DSOUND_Service _DSOUND_DeregisterDeviceDriver, LOCAL
End_Service_Table DSOUND

PSTF_COUNT	EQU	0

;   Card resources (docs/5160_tuned_config.md). DMA channel 1 is fixed below.
SB_BASE 	EQU	220h
SB_MIXADDR	EQU	SB_BASE + 4
SB_MIXDATA	EQU	SB_BASE + 5
SB_RESET	EQU	SB_BASE + 6
SB_READ 	EQU	SB_BASE + 0Ah
SB_WRITE	EQU	SB_BASE + 0Ch
SB_RSTATUS	EQU	SB_BASE + 0Eh		; reading it also acknowledges the 8-bit IRQ

DMA1_ADDR	EQU	02h
DMA1_COUNT	EQU	03h
DMA_MASK	EQU	0Ah
DMA_MODE	EQU	0Bh
DMA_FLIPFLOP	EQU	0Ch
DMA1_PAGE	EQU	83h
DMA1_MODE_AUTO	EQU	59h			; single, auto-init, memory to device, ch 1

;   Must stay below the XT's DMA reach (4-bit page latch); 9Fh keeps it in RAM.
BUF_MAXPHYS	EQU	9Fh
BUF_PAGES	EQU	4			; 16 KB; halved until it is granted

;   Normal-mode 8-bit DMA on DSP 3.xx tops out near 23 kHz of bytes a second.
HS_ABOVE	EQU	23000

;   MSSBLST.VXD's device ID. With the devnode, DSOUND uses it to find the wave
;   driver it opens for DSDDESC_DOMMSYSTEMOPEN.
MSSBLST_ID	EQU	044Eh

DSDDESC_DOMMSYSTEMOPEN		EQU	1
DSDDESC_DONTNEEDPRIMARYLOCK	EQU	8
DSCAPS_PRIMARY_FLAGS		EQU	7	; MONO + STEREO + 8BIT
DSBCAPS_PRIMARYBUFFER		EQU	1
DSBCAPS_CTRL_ANY		EQU	0E0h	; FREQUENCY + PAN + VOLUME

DS_OK			EQU	0
DSERR_ALLOCATED 	EQU	8878000Ah
DSERR_CONTROLUNAVAIL	EQU	8878001Eh
DSERR_BADFORMAT 	EQU	88780064h
DSERR_NODRIVER		EQU	88780078h
DSERR_UNSUPPORTED	EQU	80004001h
E_NOINTERFACE		EQU	80004002h

;   State bits in dwState.
ST_BUFFER	EQU	1
ST_DEVNODE	EQU	2
ST_DSOUND	EQU	4
ST_REGISTERED	EQU	8

DSDRIVERDESC_SIZE	EQU	544
DSDRIVERCAPS_SIZE	EQU	76

Declare_Virtual_Device SBDS, 1, 0, SBDS_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

VxD_LOCKED_DATA_SEG

	ALIGN 4
;   The two objects DirectSound holds: first dword is the vtable.
DrvObj		dd	OFFSET32 DrvVtbl
BufObj		dd	OFFSET32 BufVtbl

;   Every entry goes through a trace wrapper (T_*, ids in Trace_Add).
DrvVtbl dd	OFFSET32 T_Drv_QueryInterface, OFFSET32 T_Drv_AddRef, OFFSET32 T_Drv_Release
	dd	OFFSET32 T_Drv_GetDriverDesc, OFFSET32 T_Drv_Open, OFFSET32 T_Drv_Close
	dd	OFFSET32 T_Drv_GetCaps, OFFSET32 T_Drv_CreateSoundBuffer
	dd	OFFSET32 T_Drv_DuplicateSoundBuffer

BufVtbl dd	OFFSET32 T_Buf_QueryInterface, OFFSET32 T_Buf_AddRef, OFFSET32 T_Buf_Release
	dd	OFFSET32 T_Buf_Lock, OFFSET32 T_Buf_Unlock, OFFSET32 T_Buf_SetFormat
	dd	OFFSET32 T_Buf_SetFrequency, OFFSET32 T_Buf_SetVolumePan
	dd	OFFSET32 T_Buf_SetPosition, OFFSET32 T_Buf_GetPosition
	dd	OFFSET32 T_Buf_Play, OFFSET32 T_Buf_Stop

;   Call trace: dwTraceN calls so far, then a ring of TRACE_N entries of
;   (id, a1, a2, result). Written to HKLM\Software\SBDS "Trace" at Close.
TRACE_N 	EQU	128
dwTraceN	dd	0
TraceRing	dd	TRACE_N * 4 dup (0)
dwPosLogged	dd	0		; GetPosition entries since the last Play
hkTrace 	dd	0

;   Mixer probe: a 10 ms global time-out of our own while playing. Every 100 ms
;   it logs DSOUND.VXD's mixer lock (its data + 18h, DDB at + 45Ch) and our tick
;   count, so a dead DSOUND timer is told from a dead VMM timer.
PROBE_MS	EQU	10
DS_DDB_OFS	EQU	45Ch
dwDsBase	dd	0
hProbe		dd	0
dwTicks 	dd	0
dwHookAll	dd	0
DS_OBJ1_SIZE	EQU	0A38h		; DSOUND.VXD's first object: its timer helpers
Prev_Async	dd	0
Prev_Global	dd	0

;   DSOUND.VXD's DeviceIoControl table: 64 handlers at its data + 4ACh, entry 0
;   at + 606h. Every entry is pointed at Ioc_Thunk, which counts the code and
;   jumps on; the counts go into the trace at Close (id 26), and the first three
;   calls of each code as they happen (id 25).
DS_IOC_TABLE	EQU	4ACh
DS_IOC_ENTRY0	EQU	606h
IOC_N		EQU	64
dsIocOrig	dd	IOC_N dup (0)
dwIocCount	dd	IOC_N dup (0)
szTraceKey	db	"Software\SBDS",0
szTraceVal	db	"Trace",0

;   {8C4233C0-B4CC-11CE-9294-444553540000}, ...C1, and IUnknown.
IID_IDsDriver		dd	8C4233C0h
			dw	0B4CCh, 11CEh
			db	92h, 94h, 44h, 45h, 53h, 54h, 00h, 00h
IID_IDsDriverBuffer	dd	8C4233C1h
			dw	0B4CCh, 11CEh
			db	92h, 94h, 44h, 45h, 53h, 54h, 00h, 00h
IID_IUnknown		dd	0
			dw	0, 0
			db	0C0h, 0, 0, 0, 0, 0, 0, 46h

dnDevNode	dd	0
dwBufPhys	dd	0
dwBufLin	dd	0
dwBufBytes	dd	0
dwDrvRef	dd	1
dwBufRef	dd	0
dwOpen		dd	0
dwPlaying	dd	0
dwRate		dd	22050
dwChannels	dd	2
bTimeConst	db	0
bHighSpeed	db	0
bMixer0E	db	0
		db	0

;   PERF counters.
dwState 	dd	0
dwRegResult	dd	0
dwOpens 	dd	0
dwPlays 	dd	0
dwStops 	dd	0
dwGetPos	dd	0
dwFormat	dd	0		; rate | channels << 24, last accepted
dwBadFormat	dd	0
dwDspFail	dd	0

szDevId 	db	"ROOT\MEDIA\0000",0
szDsVxd 	db	"dsound.vxd",0
szDesc		db	"Sound Blaster Pro (SBDS, Inboard)",0
szDrvName	db	"SBDS.VXD",0

szSrvName	db	"SB Pro DirectSound",0
szSrvKey	db	"SBDS",0
szState 	db	"State",0
szPhys		db	"BufPhys",0
szBytes 	db	"BufBytes",0
szReg		db	"RegResult",0
szOpens 	db	"Opens",0
szPlays 	db	"Plays",0
szStops 	db	"Stops",0
szGetPos	db	"GetPosition",0
szFormat	db	"Format",0
szBadFormat	db	"BadFormat",0
szDspFail	db	"DspTimeouts",0

VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc SBDS_Control
	Control_Dispatch Device_Init, SBDS_Device_Init
	Control_Dispatch Init_Complete, SBDS_Init_Complete
	clc
	ret
EndProc SBDS_Control

;   ---- hardware ---------------------------------------------------------------

;   AL = byte. Waits for the DSP's write buffer; counts a timeout and gives up.
BeginProc Dsp_Write
	push	ecx
	push	edx
	mov	ah, al
	mov	dx, SB_WRITE
	mov	ecx, 10000
dw_wait:
	in	al, dx
	test	al, 80h
	jz	SHORT dw_go
	loop	dw_wait
	inc	[dwDspFail]
	jmp	SHORT dw_done
dw_go:
	mov	al, ah
	out	dx, al
dw_done:
	pop	edx
	pop	ecx
	ret
EndProc Dsp_Write

;   Returns CF set if the DSP did not answer AAh. Also the only way out of high speed.
BeginProc Dsp_Reset
	push	ecx
	push	edx
	mov	dx, SB_RESET
	mov	al, 1
	out	dx, al
	mov	dx, SB_WRITE			; >= 3 us: each read here crosses the ISA bus
	in	al, dx
	in	al, dx
	in	al, dx
	in	al, dx
	mov	dx, SB_RESET
	xor	al, al
	out	dx, al
	mov	ecx, 1000
dr_wait:
	mov	dx, SB_RSTATUS
	in	al, dx
	test	al, 80h
	jz	SHORT dr_next
	mov	dx, SB_READ
	in	al, dx
	cmp	al, 0AAh
	je	SHORT dr_ok
dr_next:
	loop	dr_wait
	inc	[dwDspFail]
	stc
	jmp	SHORT dr_done
dr_ok:
	clc
dr_done:
	pop	edx
	pop	ecx
	ret
EndProc Dsp_Reset

;   ESI = WAVEFORMATEX. CF set if the SB Pro cannot play it; otherwise the rate,
;   channels, time constant and speed mode are stored.
BeginProc Set_Format
	push	eax
	push	ecx
	push	edx
	cmp	word ptr [esi], 1		; WAVE_FORMAT_PCM
	jne	SHORT sf_bad
	cmp	word ptr [esi+14], 8
	jne	SHORT sf_bad
	movzx	ecx, word ptr [esi+2]
	cmp	ecx, 1
	jb	SHORT sf_bad
	cmp	ecx, 2
	ja	SHORT sf_bad
	mov	eax, [esi+4]
	cmp	eax, 4000
	jb	SHORT sf_bad
	mov	[dwRate], eax
	mov	[dwChannels], ecx
	mul	ecx				; bytes a second
	cmp	eax, 44100
	ja	SHORT sf_bad
	cmp	eax, HS_ABOVE
	seta	[bHighSpeed]
	mov	ecx, eax
	mov	eax, 1000000
	xor	edx, edx
	div	ecx
	mov	ecx, 256
	sub	ecx, eax
	mov	[bTimeConst], cl
	mov	eax, [dwChannels]
	shl	eax, 24
	or	eax, [dwRate]
	mov	[dwFormat], eax
	clc
	jmp	SHORT sf_done
sf_bad:
	inc	[dwBadFormat]
	stc
sf_done:
	pop	edx
	pop	ecx
	pop	eax
	ret
EndProc Set_Format

BeginProc Start_Playback
	pushfd
	cli
	mov	al, 05h 			; mask channel 1
	out	DMA_MASK, al
	out	DMA_FLIPFLOP, al
	mov	al, DMA1_MODE_AUTO
	out	DMA_MODE, al
	mov	eax, [dwBufPhys]
	out	DMA1_ADDR, al
	mov	al, ah
	out	DMA1_ADDR, al
	shr	eax, 16
	out	DMA1_PAGE, al
	mov	eax, [dwBufBytes]
	dec	eax
	out	DMA1_COUNT, al
	mov	al, ah
	out	DMA1_COUNT, al
	mov	al, 01h 			; unmask channel 1
	out	DMA_MASK, al
	popfd

	mov	al, 0D1h			; speaker on
	call	Dsp_Write
	mov	dx, SB_MIXADDR
	mov	al, 0Eh
	out	dx, al
	mov	dx, SB_MIXDATA
	in	al, dx
	mov	[bMixer0E], al
	and	al, NOT 2
	cmp	[dwChannels], 2
	jne	SHORT sp_mono
	or	al, 2				; stereo
sp_mono:
	out	dx, al
	mov	al, 40h
	call	Dsp_Write
	mov	al, [bTimeConst]
	call	Dsp_Write
	mov	al, 48h 			; block = half the buffer
	call	Dsp_Write
	mov	eax, [dwBufBytes]
	shr	eax, 1
	dec	eax
	push	eax
	call	Dsp_Write
	pop	eax
	mov	al, ah
	call	Dsp_Write
	mov	al, 1Ch 			; auto-init 8-bit
	cmp	[bHighSpeed], 0
	je	SHORT sp_go
	mov	al, 90h 			; high-speed auto-init 8-bit
sp_go:
	call	Dsp_Write
	mov	[dwPlaying], 1
	inc	[dwPlays]
	mov	[dwTicks], 0
	call	Probe_Arm
	ret
EndProc Start_Playback

BeginProc Probe_Arm
	push	esi
	mov	eax, PROBE_MS
	xor	edx, edx
	mov	esi, OFFSET32 Probe_Tick
	VMMCall Set_Global_Time_Out
	mov	[hProbe], esi
	pop	esi
	ret
EndProc Probe_Arm

;   Global time-out: interrupts may be off; logs into the ring only.
BeginProc Probe_Tick
	mov	[hProbe], 0
	cmp	[dwPlaying], 0
	je	SHORT pt_done
	inc	[dwTicks]
	mov	eax, [dwTicks]
	xor	edx, edx
	mov	ecx, 10
	div	ecx
	or	edx, edx
	jnz	SHORT pt_arm
	VMMCall Get_System_Time
	mov	ecx, eax			; a2 = ms
	mov	eax, [dwHookAll]		; a1 = time-outs set system-wide
pt_log:
	mov	edx, [dwTraceN]
	and	edx, TRACE_N - 1
	shl	edx, 4
	add	edx, OFFSET32 TraceRing
	mov	dword ptr [edx], 22
	mov	[edx+4], eax
	mov	[edx+8], ecx
	mov	eax, [dwTicks]
	mov	[edx+12], eax
	inc	[dwTraceN]
pt_arm:
	call	Probe_Arm
pt_done:
	ret
EndProc Probe_Tick

;   Nothing moves on the bus after this: DSP reset, channel masked.
BeginProc Stop_Playback
	cmp	[dwPlaying], 0
	je	SHORT st_done
	push	esi
	pushfd
	cli					; a tick between the two would re-arm
	xor	esi, esi
	xchg	esi, [hProbe]
	VMMCall Cancel_Time_Out 		; a zero handle is ignored
	popfd
	pop	esi
	call	Dsp_Reset
	mov	al, 05h
	out	DMA_MASK, al
	mov	al, 0D3h			; speaker off
	call	Dsp_Write
	mov	dx, SB_MIXADDR
	mov	al, 0Eh
	out	dx, al
	mov	dx, SB_MIXDATA
	mov	al, [bMixer0E]
	out	dx, al
	mov	[dwPlaying], 0
	inc	[dwStops]
st_done:
	ret
EndProc Stop_Playback

;   ESI = IID, EDI = the IID to compare with. ZF set if equal.
BeginProc Iid_Equal
	push	ecx
	push	esi
	push	edi
	mov	ecx, 4
	cld
	repe	cmpsd
	pop	edi
	pop	esi
	pop	ecx
	ret
EndProc Iid_Equal

;   ---- IDsDriver (stdcall; the first argument is the object) --------------------

BeginProc Drv_QueryInterface
	push	esi
	push	edi
	mov	esi, [esp+16]			; riid
	mov	edx, [esp+20]			; ppv
	mov	edi, OFFSET32 IID_IDsDriver
	call	Iid_Equal
	je	SHORT dq_yes
	mov	edi, OFFSET32 IID_IUnknown
	call	Iid_Equal
	je	SHORT dq_yes
	mov	dword ptr [edx], 0
	mov	eax, E_NOINTERFACE
	jmp	SHORT dq_done
dq_yes:
	mov	eax, [esp+12]
	mov	[edx], eax
	inc	[dwDrvRef]
	xor	eax, eax
dq_done:
	pop	edi
	pop	esi
	ret	12
EndProc Drv_QueryInterface

BeginProc Drv_AddRef
	inc	[dwDrvRef]
	mov	eax, [dwDrvRef]
	ret	4
EndProc Drv_AddRef

BeginProc Drv_Release
	cmp	[dwDrvRef], 1			; the static object never goes
	jbe	SHORT dr_keep
	dec	[dwDrvRef]
dr_keep:
	mov	eax, [dwDrvRef]
	ret	4
EndProc Drv_Release

BeginProc Drv_GetDriverDesc
	push	esi
	push	edi
	mov	edi, [esp+16]
	push	edi
	mov	ecx, DSDRIVERDESC_SIZE
	xor	eax, eax
	cld
	rep	stosb
	pop	edi
	mov	dword ptr [edi], DSDDESC_DOMMSYSTEMOPEN + DSDDESC_DONTNEEDPRIMARYLOCK
	push	edi
	lea	edi, [edi+4]
	mov	esi, OFFSET32 szDesc
	mov	ecx, SIZEOF szDesc
	rep	movsb
	mov	edi, [esp]
	lea	edi, [edi+260]
	mov	esi, OFFSET32 szDrvName
	mov	ecx, SIZEOF szDrvName
	rep	movsb
	pop	edi
	mov	eax, [dnDevNode]
	mov	[edi+516], eax
	mov	word ptr [edi+520], MSSBLST_ID	; wVxdId
	xor	eax, eax
	pop	edi
	pop	esi
	ret	8
EndProc Drv_GetDriverDesc

BeginProc Drv_Open
	cmp	[dwOpen], 0
	je	SHORT do_try
	mov	eax, DSERR_ALLOCATED
	ret	4
do_try:
	call	Dsp_Reset
	jnc	SHORT do_ok
	mov	eax, DSERR_NODRIVER
	ret	4
do_ok:
	mov	[dwOpen], 1
	inc	[dwOpens]
	xor	eax, eax
	ret	4
EndProc Drv_Open

BeginProc Drv_Close
	call	Ioc_Dump
	call	Stop_Playback
	mov	[dwOpen], 0
	xor	eax, eax
	ret	4
EndProc Drv_Close

BeginProc Drv_GetCaps
	push	edi
	mov	edi, [esp+12]
	mov	ecx, DSDRIVERCAPS_SIZE / 4
	xor	eax, eax
	cld
	rep	stosd
	mov	edi, [esp+12]
	mov	dword ptr [edi], DSCAPS_PRIMARY_FLAGS
	mov	dword ptr [edi+12], 1		; dwPrimaryBuffers
	pop	edi
	ret	8
EndProc Drv_GetCaps

;   (this, pwfx, dwFlags, dwCardAddress, pdwSize, ppbBuffer, ppvObj)
BeginProc Drv_CreateSoundBuffer
	push	esi
	mov	eax, [esp+16]			; dwFlags
	test	eax, DSBCAPS_PRIMARYBUFFER
	jnz	SHORT dc_primary
	mov	eax, DSERR_UNSUPPORTED		; DSOUND mixes secondaries itself
	jmp	SHORT dc_done
dc_primary:
	test	eax, DSBCAPS_CTRL_ANY
	jz	SHORT dc_ctrl_ok
	mov	eax, DSERR_CONTROLUNAVAIL
	jmp	SHORT dc_done
dc_ctrl_ok:
	cmp	[dwBufRef], 0
	je	SHORT dc_free
	mov	eax, DSERR_ALLOCATED
	jmp	SHORT dc_done
dc_free:
	mov	esi, [esp+12]			; pwfx
	or	esi, esi
	jz	SHORT dc_fmt_ok
	call	Set_Format
	jnc	SHORT dc_fmt_ok
	mov	eax, DSERR_BADFORMAT
	jmp	SHORT dc_done
dc_fmt_ok:
	mov	eax, [esp+24]
	mov	ecx, [dwBufBytes]
	mov	[eax], ecx
	mov	eax, [esp+28]
	mov	ecx, [dwBufLin]
	mov	[eax], ecx
	mov	eax, [esp+32]
	mov	dword ptr [eax], OFFSET32 BufObj
	mov	[dwBufRef], 1
	xor	eax, eax
dc_done:
	pop	esi
	ret	28
EndProc Drv_CreateSoundBuffer

BeginProc Drv_DuplicateSoundBuffer
	mov	eax, DSERR_UNSUPPORTED
	ret	12
EndProc Drv_DuplicateSoundBuffer

;   ---- IDsDriverBuffer ----------------------------------------------------------

BeginProc Buf_QueryInterface
	push	esi
	push	edi
	mov	esi, [esp+16]
	mov	edx, [esp+20]
	mov	edi, OFFSET32 IID_IDsDriverBuffer
	call	Iid_Equal
	je	SHORT bq_yes
	mov	edi, OFFSET32 IID_IUnknown
	call	Iid_Equal
	je	SHORT bq_yes
	mov	dword ptr [edx], 0		; no property sets (3D, speaker config)
	mov	eax, E_NOINTERFACE
	jmp	SHORT bq_done
bq_yes:
	mov	eax, [esp+12]
	mov	[edx], eax
	inc	[dwBufRef]
	xor	eax, eax
bq_done:
	pop	edi
	pop	esi
	ret	12
EndProc Buf_QueryInterface

BeginProc Buf_AddRef
	inc	[dwBufRef]
	mov	eax, [dwBufRef]
	ret	4
EndProc Buf_AddRef

BeginProc Buf_Release
	cmp	[dwBufRef], 0
	je	SHORT br_done
	dec	[dwBufRef]
	jnz	SHORT br_done
	call	Stop_Playback
br_done:
	mov	eax, [dwBufRef]
	ret	4
EndProc Buf_Release

BeginProc Buf_Lock
	mov	eax, DSERR_UNSUPPORTED		; DONTNEEDPRIMARYLOCK
	ret	32
EndProc Buf_Lock

BeginProc Buf_Unlock
	mov	eax, DSERR_UNSUPPORTED
	ret	20
EndProc Buf_Unlock

;   DSOUND never calls this while the buffer plays (DDK guide, SetFormat).
BeginProc Buf_SetFormat
	push	esi
	mov	esi, [esp+12]
	call	Set_Format
	mov	eax, DSERR_BADFORMAT
	jc	SHORT bf_done
	xor	eax, eax
bf_done:
	pop	esi
	ret	8
EndProc Buf_SetFormat

BeginProc Buf_SetFrequency
	mov	eax, DSERR_UNSUPPORTED
	ret	8
EndProc Buf_SetFrequency

BeginProc Buf_SetVolumePan
	mov	eax, DSERR_UNSUPPORTED
	ret	8
EndProc Buf_SetVolumePan

BeginProc Buf_SetPosition
	mov	eax, DSERR_UNSUPPORTED
	ret	8
EndProc Buf_SetPosition

;   (this, pdwPlay, pdwWrite). Bytes played = size - (count + 1); the 8237 counts
;   down and reloads itself. Reading 22Eh acknowledges a pending DSP interrupt, so
;   auto-init never waits on an ISR that is not ours.
BeginProc Buf_GetPosition
	push	ebx
	xor	eax, eax
	cmp	[dwPlaying], 0
	je	SHORT bg_store
	pushfd
	cli
	out	DMA_FLIPFLOP, al
	in	al, DMA1_COUNT
	mov	bl, al
	in	al, DMA1_COUNT
	mov	bh, al
	out	DMA_FLIPFLOP, al
	in	al, DMA1_COUNT
	mov	cl, al
	in	al, DMA1_COUNT
	mov	ch, al
	popfd
	cmp	bh, ch				; low byte rolled between reads: take the later
	je	SHORT bg_have
	mov	bx, cx
bg_have:
	movzx	ebx, bx
	inc	ebx
	mov	eax, [dwBufBytes]
	sub	eax, ebx
	jae	SHORT bg_inrange
	xor	eax, eax
bg_inrange:
	cmp	eax, [dwBufBytes]
	jb	SHORT bg_ack
	xor	eax, eax
bg_ack:
	push	eax
	mov	dx, SB_RSTATUS
	in	al, dx
	pop	eax
	inc	[dwGetPos]
bg_store:
	mov	ecx, [esp+12]
	or	ecx, ecx
	jz	SHORT bg_nowrite1
	mov	[ecx], eax
bg_nowrite1:
	mov	ecx, [esp+16]
	or	ecx, ecx
	jz	SHORT bg_nowrite2
	mov	[ecx], eax
bg_nowrite2:
	xor	eax, eax
	pop	ebx
	ret	12
EndProc Buf_GetPosition

BeginProc Buf_Play
	cmp	[dwPlaying], 0
	jne	SHORT bp_done
	call	Start_Playback
bp_done:
	xor	eax, eax
	ret	16
EndProc Buf_Play

BeginProc Buf_Stop
	call	Stop_Playback
	xor	eax, eax
	ret	4
EndProc Buf_Stop

;   ECX = id, EDX = a1, EBX = a2, EAX = result. All registers kept.
BeginProc Ring_Put
	push	edi
	mov	edi, [dwTraceN]
	and	edi, TRACE_N - 1
	shl	edi, 4
	add	edi, OFFSET32 TraceRing
	mov	[edi], ecx
	mov	[edi+4], edx
	mov	[edi+8], ebx
	mov	[edi+12], eax
	inc	[dwTraceN]
	pop	edi
	ret
EndProc Ring_Put

;   Entered by DSOUND.VXD's dispatcher with ECX = the ioctl code (below 40h,
;   checked there) and the handler's argument on the stack.
BeginProc Ioc_Thunk
	inc	dword ptr [dwIocCount + ecx*4]
	cmp	dword ptr [dwIocCount + ecx*4], 3
	ja	SHORT it_go
	pushad
	VMMCall Get_System_Time
	mov	ebx, eax
	mov	edx, ecx
	mov	eax, [dwIocCount + ecx*4]
	mov	ecx, 25
	call	Ring_Put
	popad
it_go:
	jmp	dword ptr [dsIocOrig + ecx*4]
EndProc Ioc_Thunk

;   Points DSOUND.VXD's ioctl table at Ioc_Thunk, only if entry 0 is where its
;   DDB says it should be. dwState bit 10h records that it was done.
BeginProc Ioc_Install
	pushad
	mov	ebx, [dwDsBase]
	lea	esi, [ebx + DS_IOC_TABLE]
	lea	eax, [ebx + DS_IOC_ENTRY0]
	cmp	[esi], eax
	jne	SHORT ii_done
	xor	ecx, ecx
ii_next:
	mov	eax, [esi + ecx*4]
	mov	[dsIocOrig + ecx*4], eax
	mov	dword ptr [esi + ecx*4], OFFSET32 Ioc_Thunk
	inc	ecx
	cmp	ecx, IOC_N
	jb	ii_next
	or	[dwState], 10h
ii_done:
	popad
	ret
EndProc Ioc_Install

;   The two versions DSOUND.DLL checks before it will use DSOUND.VXD's timer
;   mixer (its ioctls 30 and 32): id 27 PAGEFILE (a1 EAX, a2 ECX, r BL),
;   id 28 VMCPD (a1 EAX, a2 ECX). A device that is not loaded logs -1.
BeginProc Log_Versions
	pushad
	mov	eax, PAGEFILE_Device_ID
	xor	edi, edi
	VMMCall Get_DDB
	mov	edx, -1
	jecxz	SHORT lv_nopf
	VxDCall PAGEFILE_Get_Version
	mov	edx, eax
	movzx	eax, bl
	mov	ebx, ecx
	jmp	SHORT lv_pf
lv_nopf:
	xor	ebx, ebx
	xor	eax, eax
lv_pf:
	mov	ecx, 27
	call	Ring_Put
	mov	eax, VMCPD_Device_ID
	xor	edi, edi
	VMMCall Get_DDB
	mov	edx, -1
	xor	ebx, ebx
	jecxz	SHORT lv_nocp
	VxDCall VMCPD_Get_Version
	mov	edx, eax
	mov	ebx, ecx
lv_nocp:
	xor	eax, eax
	mov	ecx, 28
	call	Ring_Put
	popad
	ret
EndProc Log_Versions

;   Into the trace before Close publishes it: one id 26 per code DSOUND used.
BeginProc Ioc_Dump
	call	Log_Versions
	pushad
	xor	esi, esi
id_next:
	mov	eax, [dwIocCount + esi*4]
	or	eax, eax
	jz	SHORT id_skip
	mov	edx, esi
	xor	ebx, ebx
	mov	ecx, 26
	call	Ring_Put
id_skip:
	inc	esi
	cmp	esi, IOC_N
	jb	id_next
	popad
	ret
EndProc Ioc_Dump

;   Time-outs DSOUND.VXD sets: id 23 async, 24 global; a1 = ms, a2 = time,
;   result = caller offset in DSOUND | callback offset << 16.
BeginProc Hook_Async, HOOK_PROC, Prev_Async
	pushad
	mov	ecx, 23
	call	Log_DsTimeout
	popad
	jmp	[Prev_Async]
EndProc Hook_Async

BeginProc Hook_Global, HOOK_PROC, Prev_Global
	pushad
	mov	ecx, 24
	call	Log_DsTimeout
	popad
	jmp	[Prev_Global]
EndProc Hook_Global

;   Called from a hook with PUSHAD on the stack: [esp+4] is the saved block,
;   [esp+4+32] the service caller's return address.
BeginProc Log_DsTimeout
	inc	[dwHookAll]			; proves the hook runs at all
	mov	edx, [dwDsBase]
	or	edx, edx
	jz	SHORT ld_done
	mov	ebx, [esp+4+32]
	sub	ebx, edx
	cmp	ebx, DS_OBJ1_SIZE
	jae	SHORT ld_done
	sub	esi, edx			; callback, relative
	shl	esi, 16
	or	ebx, esi
	mov	edi, eax			; ms
	VMMCall Get_System_Time
	mov	edx, [dwTraceN]
	and	edx, TRACE_N - 1
	shl	edx, 4
	add	edx, OFFSET32 TraceRing
	mov	[edx], ecx
	mov	[edx+4], edi
	mov	[edx+8], eax
	mov	[edx+12], ebx
	inc	[dwTraceN]
ld_done:
	ret
EndProc Log_DsTimeout

;   ---- call trace ---------------------------------------------------------------

;   T_name: copy the caller's nbytes of stdcall arguments, call name, log, return.
TRACED	MACRO	name, id, nbytes
BeginProc T_&name
	push	ebp
	mov	ebp, esp
	push	ebx
	mov	ecx, nbytes / 4
	lea	edx, [ebp + 4 + nbytes]
@@:
	push	dword ptr [edx]
	sub	edx, 4
	loop	@B
	call	name
	mov	ebx, ebp
	mov	ecx, id
	call	Trace_Add
	pop	ebx
	pop	ebp
	ret	nbytes
EndProc T_&name
	ENDM

	TRACED	Drv_QueryInterface, 1, 12
	TRACED	Drv_AddRef, 2, 4
	TRACED	Drv_Release, 3, 4
	TRACED	Drv_GetDriverDesc, 4, 8
	TRACED	Drv_Open, 5, 4
	TRACED	Drv_Close, 6, 4
	TRACED	Drv_GetCaps, 7, 8
	TRACED	Drv_CreateSoundBuffer, 8, 28
	TRACED	Drv_DuplicateSoundBuffer, 9, 12
	TRACED	Buf_QueryInterface, 10, 12
	TRACED	Buf_AddRef, 11, 4
	TRACED	Buf_Release, 12, 4
	TRACED	Buf_Lock, 13, 32
	TRACED	Buf_Unlock, 14, 20
	TRACED	Buf_SetFormat, 15, 8
	TRACED	Buf_SetFrequency, 16, 8
	TRACED	Buf_SetVolumePan, 17, 8
	TRACED	Buf_SetPosition, 18, 8
	TRACED	Buf_GetPosition, 19, 12
	TRACED	Buf_Play, 20, 16
	TRACED	Buf_Stop, 21, 4

;   WAVEFORMATEX at ESI -> rate | bits << 16 | channels << 24 in EDX (0 if no pointer)
BeginProc Pack_Format
	xor	edx, edx
	or	esi, esi
	jz	SHORT pf_done
	movzx	edx, byte ptr [esi+2]
	shl	edx, 8
	mov	dl, byte ptr [esi+14]
	shl	edx, 16
	mov	dx, word ptr [esi+4]
pf_done:
	ret
EndProc Pack_Format

;   ECX = id, EBX = the wrapper's frame ([ebx+12] first argument after this),
;   EAX = the method's result (kept). Pointer arguments are logged as what they
;   point to: an IID's first dword, a format, a position, amp factors.
BeginProc Trace_Add
	pushad
	mov	edi, [ebx+12]			; a1
	mov	ebp, [ebx+16]			; a2
	cmp	ecx, 1
	je	SHORT ta_iid
	cmp	ecx, 10
	jne	SHORT ta_notiid
ta_iid:
	mov	edi, [edi]
	jmp	SHORT ta_log
ta_notiid:
	cmp	ecx, 8				; CreateSoundBuffer: format, flags
	jne	SHORT ta_notcreate
	mov	esi, edi
	call	Pack_Format
	mov	edi, edx
	jmp	SHORT ta_log
ta_notcreate:
	cmp	ecx, 15 			; SetFormat: format
	jne	SHORT ta_notfmt
	mov	esi, edi
	call	Pack_Format
	mov	edi, edx
	jmp	SHORT ta_log
ta_notfmt:
	cmp	ecx, 17 			; SetVolumePan: left, right amp factors
	jne	SHORT ta_notvol
	mov	ebp, [edi+4]
	mov	edi, [edi]
	jmp	SHORT ta_log
ta_notvol:
	cmp	ecx, 21 			; Stop: time in ms
	jne	SHORT ta_notstop
	call	Trace_Ms
	jmp	SHORT ta_log
ta_notstop:
	cmp	ecx, 20 			; Play: flags (a3), time in ms
	jne	SHORT ta_notplay
	mov	edi, [ebx+20]
	mov	[dwPosLogged], 0
	call	Trace_Ms
	jmp	SHORT ta_log
ta_notplay:
	cmp	ecx, 19 			; GetPosition: play cursor, time; first 32 per Play
	jne	SHORT ta_log
	cmp	[dwPosLogged], 32
	jae	SHORT ta_done
	inc	[dwPosLogged]
	call	Trace_Ms
	or	edi, edi
	jz	SHORT ta_log
	mov	edi, [edi]
ta_log:
	mov	edx, [dwTraceN]
	and	edx, TRACE_N - 1
	shl	edx, 4
	add	edx, OFFSET32 TraceRing
	mov	[edx], ecx
	mov	[edx+4], edi
	mov	[edx+8], ebp
	mov	[edx+12], eax
	inc	[dwTraceN]
	cmp	ecx, 19 			; publish on every call but GetPosition and the
	je	SHORT ta_done			; reference counts, so a DirectSound that only
	cmp	ecx, 2				; asks for the description still leaves a trace
	je	SHORT ta_done
	cmp	ecx, 3
	je	SHORT ta_done
	cmp	ecx, 11
	je	SHORT ta_done
	cmp	ecx, 12
	je	SHORT ta_done
	call	Trace_Write
ta_done:
	popad
	ret
EndProc Trace_Add

;   EBP = system time in ms (Trace_Add's a2 slot); EAX, ECX kept.
BeginProc Trace_Ms
	push	eax
	push	ecx
	VMMCall Get_System_Time
	mov	ebp, eax
	pop	ecx
	pop	eax
	ret
EndProc Trace_Ms

;   HKLM\Software\SBDS "Trace" = dwTraceN, then the ring. Called at Close, in the
;   caller's thread, never at interrupt time.
BeginProc Trace_Write
	push	OFFSET32 hkTrace
	push	OFFSET32 szTraceKey
	push	HKEY_LOCAL_MACHINE
	VMMCall _RegCreateKey
	add	esp, 3*4
	or	eax, eax
	jnz	SHORT tw_done
	push	4 + TRACE_N * 16
	push	OFFSET32 dwTraceN
	push	REG_BINARY
	push	0
	push	OFFSET32 szTraceVal
	push	[hkTrace]
	VMMCall _RegSetValueEx
	add	esp, 6*4
	push	[hkTrace]
	VMMCall _RegCloseKey
	add	esp, 4
tw_done:
	ret
EndProc Trace_Write

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG

;   Device_Init: the DMA buffer, while low contiguous pages are still free.
BeginProc SBDS_Device_Init
	push	ebx
	mov	ebx, BUF_PAGES			; _PageAllocate is C: ECX does not survive
sdi_try:
	mov	eax, ebx
	dec	eax				; align to its own size: no 64 KB crossing
	VMMCall _PageAllocate <ebx, PG_SYS, 0, eax, 0, BUF_MAXPHYS, \
			       OFFSET32 dwBufPhys, PageUseAlign+PageContig+PageFixed>
	or	eax, eax
	jnz	SHORT sdi_got
	shr	ebx, 1
	jnz	sdi_try
	jmp	SHORT sdi_perf
sdi_got:
	mov	[dwBufLin], edx
	mov	ecx, ebx
	shl	ecx, 12
	mov	[dwBufBytes], ecx
	mov	edi, edx
	mov	al, 80h 			; 8-bit silence
	cld
	rep	stosb
	or	[dwState], ST_BUFFER

sdi_perf:
	push	0
	push	OFFSET32 szSrvKey
	push	OFFSET32 szSrvName
	push	0
	push	0
	push	esp
	VxDCall PERF_Server_Register
	add	esp, 6*4
	or	eax, eax
	jz	sdi_done
	mov	edx, eax
	mov	esi, OFFSET32 szState
	mov	edi, OFFSET32 dwState
	call	SB_Add_Stat
	mov	esi, OFFSET32 szPhys
	mov	edi, OFFSET32 dwBufPhys
	call	SB_Add_Stat
	mov	esi, OFFSET32 szBytes
	mov	edi, OFFSET32 dwBufBytes
	call	SB_Add_Stat
	mov	esi, OFFSET32 szReg
	mov	edi, OFFSET32 dwRegResult
	call	SB_Add_Stat
	mov	esi, OFFSET32 szOpens
	mov	edi, OFFSET32 dwOpens
	call	SB_Add_Stat
	mov	esi, OFFSET32 szPlays
	mov	edi, OFFSET32 dwPlays
	call	SB_Add_Stat
	mov	esi, OFFSET32 szStops
	mov	edi, OFFSET32 dwStops
	call	SB_Add_Stat
	mov	esi, OFFSET32 szGetPos
	mov	edi, OFFSET32 dwGetPos
	call	SB_Add_Stat
	mov	esi, OFFSET32 szFormat
	mov	edi, OFFSET32 dwFormat
	call	SB_Add_Stat
	mov	esi, OFFSET32 szBadFormat
	mov	edi, OFFSET32 dwBadFormat
	call	SB_Add_Stat
	mov	esi, OFFSET32 szDspFail
	mov	edi, OFFSET32 dwDspFail
	call	SB_Add_Stat
sdi_done:
	pop	ebx
	clc
	ret
EndProc SBDS_Device_Init

;   Init_Complete: find the SB Pro's devnode, load DSOUND.VXD, register. Any
;   failure leaves DirectSound on its own emulation.
BeginProc SBDS_Init_Complete
	call	Trace_Write			; replace the last boot's trace
	test	[dwState], ST_BUFFER
	jz	sic_done
	push	0				; CM_LOCATE_DEVNODE_NORMAL
	push	OFFSET32 szDevId
	push	OFFSET32 dnDevNode
	VxDCall _CONFIGMG_Locate_DevNode
	add	esp, 3*4
	or	eax, eax
	jnz	sic_fail
	or	[dwState], ST_DEVNODE
	mov	eax, VXDLDR_INIT_DEVICE
	mov	edx, OFFSET32 szDsVxd
	VxDCall VXDLDR_LoadDevice
	jc	sic_fail
	or	[dwState], ST_DSOUND
	push	edi
	mov	eax, DSOUND_Device_ID
	xor	edi, edi
	VMMCall Get_DDB
	pop	edi
	jecxz	SHORT sic_noddb
	sub	ecx, DS_DDB_OFS
	mov	[dwDsBase], ecx
	push	esi
	GetVxDServiceOrdinal eax, Set_Async_Time_Out
	mov	esi, OFFSET32 Hook_Async
	VMMCall Hook_Device_Service
	GetVxDServiceOrdinal eax, Set_Global_Time_Out
	mov	esi, OFFSET32 Hook_Global
	VMMCall Hook_Device_Service
	pop	esi
	call	Ioc_Install
	call	Log_Versions
sic_noddb:
	push	0
	push	OFFSET32 DrvObj
	VxDCall _DSOUND_RegisterDeviceDriver
	add	esp, 2*4
	mov	[dwRegResult], eax
	or	eax, eax
	jnz	SHORT sic_done
	or	[dwState], ST_REGISTERED
	jmp	SHORT sic_done
sic_fail:
	mov	[dwRegResult], eax
sic_done:
	clc
	ret
EndProc SBDS_Init_Complete

;   EDX = server handle, ESI = name, EDI = DWORD counter (as NMIWATCH.ASM)
BeginProc SB_Add_Stat
	push	edx
	push	0
	push	edi
	push	esi
	push	0
	push	esi
	push	esi
	push	PSTF_COUNT
	push	0
	push	esp
	push	edx
	VxDCall PERF_Server_Add_Stat
	add	esp, 10*4
	pop	edx
	ret
EndProc SB_Add_Stat

VxD_ICODE_ENDS

	END
