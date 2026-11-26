;   SBDS.VXD - DirectSound driver for the Sound Blaster Pro 2
;   on the 5160 + Inboard 386/PC. Design: docs/sbds_design_2026_11_23.md.
;
;   MSSBLST.VXD stays the wave driver and keeps IRQ 5 and VDMAD's channel 1. This
;   VxD drives the DSP and the 8237 directly from ring 0 while DirectSound holds the
;   wave device open (DSDDESC_DOMMSYSTEMOPEN), and paces itself from the DMA count
;   on a time-out, so it needs no interrupt of its own.
;
;   SBDS is the mixer: 8-bit secondaries become our channels, mixed with the
;   primary (where DirectSound mixes whatever it kept) into the DMA ring.
;
;   DirectX 7 mixes into the buffer on its own timer only when Windows reports a
;   486-class FPU (VMCPD coprocessor type 3 or more; the card's 387 reports 3).
;   Without one it mixes only when an application calls in.
;
;   Build with -Trace for the call trace in HKLM\Software\SBDS (sbds_trace.py).

	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE VXDLDR.Inc
	INCLUDE CONFIGMG.Inc
	INCLUDE VMMREG.Inc
	INCLUDE VWIN32.Inc
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
DSDDESC_USESYSTEMMEMORY 	EQU	4
DSDDESC_DONTNEEDSECONDARYLOCK	EQU	10h
DSCAPS_PRIMARY_FLAGS		EQU	7	; MONO + STEREO + 8BIT
DSCAPS_OUR_FLAGS		EQU	717h	; primary, CONTINUOUSRATE, secondary MONO + STEREO + 8BIT
DSBCAPS_PRIMARYBUFFER		EQU	1
DSBCAPS_CTRL_ANY		EQU	0E0h	; FREQUENCY + PAN + VOLUME
;   3D and position notification are left to DSOUND's own mixer.
DSBCAPS_NOT_OURS		EQU	110h
DSBPLAY_LOOPING 		EQU	1
SEC_RATE_MIN			EQU	100
SEC_RATE_MAX			EQU	100000

DS_OK			EQU	0
DSERR_ALLOCATED 	EQU	8878000Ah
DSERR_CONTROLUNAVAIL	EQU	8878001Eh
DSERR_BADFORMAT 	EQU	88780064h
DSERR_NODRIVER		EQU	88780078h
DSERR_UNSUPPORTED	EQU	80004001h
DSERR_INVALIDCALL	EQU	88780032h
DSERR_INVALIDPARAM	EQU	80070057h
E_NOINTERFACE		EQU	80004002h

;   State bits in dwState.
ST_BUFFER	EQU	1
ST_DEVNODE	EQU	2
ST_DSOUND	EQU	4
ST_REGISTERED	EQU	8

DSDRIVERDESC_SIZE	EQU	544
DSDRIVERCAPS_SIZE	EQU	76

;   Secondary channels: DSOUND owns each buffer's memory (USESYSTEMMEMORY); a
;   channel is a cursor, a rate and amp factors over it. Duplicates share memory.
SEC_N		EQU	64			; DSOUND holds one per sound, playing or not
SEC_SIZE	EQU	64			; Slot_Of shifts by 6
S_VTBL		EQU	0
S_REF		EQU	4
S_STATE 	EQU	8
S_MEM		EQU	12
S_BYTES 	EQU	16
S_ALIGN 	EQU	20			; bytes per sample frame (8-bit: channels)
S_FREQ		EQU	24
S_POS		EQU	28			; position at S_T0
S_T0		EQU	32			; system time in ms
S_AMPL		EQU	36			; 16.16
S_AMPR		EQU	40
S_CFLAGS	EQU	44
S_VOICE 	EQU	48			; voice number + 1, 0 = none
S_FRAC		EQU	52			; position fraction, 16 bits
SS_INUSE	EQU	1
SS_PLAYING	EQU	2
SS_LOOP 	EQU	4
SS_ENDED	EQU	8			; one-shot reached its end; DSOUND will Stop it
SS_LOCKED	EQU	10h

;   Voices: the channels being mixed, each with a left and a right table of 256
;   signed words. Doom plays at most 8 sounds at once.
VOICE_N 	EQU	16
VOICE_BYTES	EQU	1024
MIX_MAX 	EQU	4096			; bytes per tick at most; the scratch holds words
WORK_PAGES	EQU	(VOICE_N * VOICE_BYTES + MIX_MAX * 2) / 4096
TICK_MS 	EQU	20
SLEEP_TICK_MS	EQU	100			; DMA asleep: only DSOUND's primary is watched

;   The display handshake. The Mach8 driver's hot data (M8_HOT, m8.h) holds a busy
;   byte both halves set around engine work: 1 for a GDI or DirectDraw operation, 2
;   while M8FEED feeds the FIFO. A tick that lands then defers DEFER_MS if the ring
;   holds DEFER_MIN_MS or more, so mixing - which empties the cache - falls in the
;   display's gaps instead of mid-blit.
H_BUSY		EQU	12
DEFER_MS	EQU	5
DEFER_MIN_MS	EQU	40
;   A sound started by Play is added into the ring already written, from this far
;   ahead of the 8237, so it does not wait out the lead. The add must finish first.
LATE_GUARD_MS	EQU	10
IOCTL_SET_HOT	EQU	1			; in: the hot data's linear address (M8DD32)
LEAD_MS 	EQU	60			; how far ahead of the 8237 the ring is written
LEAD_MAX_MS	EQU	200			; grown 20 ms per underrun up to this ...
LEAD_CALM_TICKS EQU	250			; ... and shrunk 20 ms after this many clean ticks
QUIET_MS	EQU	300			; silence this long stops the DMA until a sound starts

Declare_Virtual_Device SBDS, 1, 0, SBDS_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

VxD_LOCKED_DATA_SEG

	ALIGN 4
;   The two objects DirectSound holds: first dword is the vtable.
DrvObj		dd	OFFSET32 DrvVtbl
BufObj		dd	OFFSET32 BufVtbl

	ALIGN 4
SecSlots	db	SEC_N * SEC_SIZE dup (0)

;   With SBDS_TRACE every entry goes through a trace wrapper (T_*, ids in Trace_Add).
IFDEF SBDS_TRACE
DrvVtbl dd	OFFSET32 T_Drv_QueryInterface, OFFSET32 T_Drv_AddRef, OFFSET32 T_Drv_Release
	dd	OFFSET32 T_Drv_GetDriverDesc, OFFSET32 T_Drv_Open, OFFSET32 T_Drv_Close
	dd	OFFSET32 T_Drv_GetCaps, OFFSET32 T_Drv_CreateSoundBuffer
	dd	OFFSET32 T_Drv_DuplicateSoundBuffer

BufVtbl dd	OFFSET32 T_Buf_QueryInterface, OFFSET32 T_Buf_AddRef, OFFSET32 T_Buf_Release
	dd	OFFSET32 T_Buf_Lock, OFFSET32 T_Buf_Unlock, OFFSET32 T_Buf_SetFormat
	dd	OFFSET32 T_Buf_SetFrequency, OFFSET32 T_Buf_SetVolumePan
	dd	OFFSET32 T_Buf_SetPosition, OFFSET32 T_Buf_GetPosition
	dd	OFFSET32 T_Buf_Play, OFFSET32 T_Buf_Stop

SecVtbl dd	OFFSET32 T_Sec_QueryInterface, OFFSET32 T_Sec_AddRef, OFFSET32 T_Sec_Release
	dd	OFFSET32 T_Sec_Lock, OFFSET32 T_Sec_Unlock, OFFSET32 T_Sec_SetFormat
	dd	OFFSET32 T_Sec_SetFrequency, OFFSET32 T_Sec_SetVolumePan
	dd	OFFSET32 T_Sec_SetPosition, OFFSET32 T_Sec_GetPosition
	dd	OFFSET32 T_Sec_Play, OFFSET32 T_Sec_Stop
ELSE
DrvVtbl dd	OFFSET32 Drv_QueryInterface, OFFSET32 Drv_AddRef, OFFSET32 Drv_Release
	dd	OFFSET32 Drv_GetDriverDesc, OFFSET32 Drv_Open, OFFSET32 Drv_Close
	dd	OFFSET32 Drv_GetCaps, OFFSET32 Drv_CreateSoundBuffer
	dd	OFFSET32 Drv_DuplicateSoundBuffer

BufVtbl dd	OFFSET32 Buf_QueryInterface, OFFSET32 Buf_AddRef, OFFSET32 Buf_Release
	dd	OFFSET32 Buf_Lock, OFFSET32 Buf_Unlock, OFFSET32 Buf_SetFormat
	dd	OFFSET32 Buf_SetFrequency, OFFSET32 Buf_SetVolumePan
	dd	OFFSET32 Buf_SetPosition, OFFSET32 Buf_GetPosition
	dd	OFFSET32 Buf_Play, OFFSET32 Buf_Stop

SecVtbl dd	OFFSET32 Sec_QueryInterface, OFFSET32 Sec_AddRef, OFFSET32 Sec_Release
	dd	OFFSET32 Sec_Lock, OFFSET32 Sec_Unlock, OFFSET32 Sec_SetFormat
	dd	OFFSET32 Sec_SetFrequency, OFFSET32 Sec_SetVolumePan
	dd	OFFSET32 Sec_SetPosition, OFFSET32 Sec_GetPosition
	dd	OFFSET32 Sec_Play, OFFSET32 Sec_Stop
ENDIF

IFDEF SBDS_TRACE
;   Call trace: dwTraceN calls so far, then a ring of TRACE_N entries of
;   (id, a1, a2, result). Rewritten to HKLM\Software\SBDS "Trace" on every call
;   except GetPosition and the reference counts.
TRACE_N 	EQU	512
dwTraceN	dd	0
TraceRing	dd	TRACE_N * 4 dup (0)
dwPosLogged	dd	0		; GetPosition entries since the last Play
dwTraceId	dd	0		; id | channel << 8, as stored
hkTrace 	dd	0

szTraceKey	db	"Software\SBDS",0
szTraceVal	db	"Trace",0
ENDIF

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
bTimeConst	db	0EAh			; 22,050 Hz stereo until DSOUND sets a format
bHighSpeed	db	1
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

;   The engine.
dwStageLin	dd	0		; the primary: DSOUND mixes its own buffers here
dwStageR	dd	0		; tick's read point in it = the primary's play cursor
dwWp		dd	0		; tick's write point in the DMA ring
dwLead		dd	0
dwPrimPlaying	dd	0
hTick		dd	0
dwVoiceTabs	dd	0
dwScratch	dd	0
dwVoiceUsed	dd	0
dwVoiceFull	dd	0
dwEngine	dd	0		; tick running (dwPlaying: DMA running)
dwQuietMs	dd	0
dwLastMs	dd	0
mcAudible	dd	0
dwVoicesNow	dd	0
;   "Stats" in HKLM\Software\SBDS, written when the engine stops; keep the order
;   (sbds_trace.py --stats).
dwTicks 	dd	0
dwUnderruns	dd	0
dwVoiceFull2	dd	0		; mirrors dwVoiceFull at write time
dwSleeps	dd	0
dwWakes 	dd	0
dwVoiceMax	dd	0
dwEngineRuns	dd	0
dwLeadMs	dd	LEAD_MS 	; the lead now; 'lead ms' in the stats
dwLeadMaxMs	dd	0
dwDeferred	dd	0		; ticks put off while the display was busy
dwBusyMixed	dd	0		; ticks that mixed while busy: too little in the ring
dwHotSets	dd	0		; IOCTL_SET_HOT calls received
dwHotBad	dd	0		; ... refused (no address, short buffer, lock failed)
dwHot		dd	0		; the display's hot data; 0 = never handed over
dwHotBusy	dd	0		; ticks that saw the busy byte set, deferred or not
dwLateStarts	dd	0		; Plays added into the ring already written
dwLateBytes	dd	0		; ... and the ring bytes they covered
STATS_DWORDS	EQU	17
dwDeferMin	dd	0		; DEFER_MIN_MS in bytes
dwLateGuard	dd	0		; LATE_GUARD_MS in bytes, whole frames
dwCalm		dd	0
hkStats 	dd	0
szSbdsKey	db	"Software\SBDS",0
szStatsVal	db	"Stats",0
mcBytes 	dd	0
mvCount 	dd	0
mvSlot		dd	0
mvStep		dd	0
mvTotal 	dd	0
mvLeft		dd	0
mvMem		dd	0
mvEnd		dd	0
mcPrimLoud	dd	0
mcRingEnd	dd	0
mvSrcSt 	db	0
		db	3 dup (0)

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
	Control_Dispatch W32_DEVICEIOCONTROL, SBDS_IoCtl
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
	ret
EndProc Start_Playback

;   Nothing moves on the bus after this: DSP reset, channel masked.
BeginProc Stop_Playback
	cmp	[dwPlaying], 0
	je	SHORT st_done
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
	mov	dword ptr [edi], DSDDESC_DOMMSYSTEMOPEN + DSDDESC_DONTNEEDPRIMARYLOCK + \
				 DSDDESC_USESYSTEMMEMORY + DSDDESC_DONTNEEDSECONDARYLOCK
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
	cmp	[dwStageLin], 0 		; no primary or voice memory: DSOUND emulates
	jne	SHORT do_mem
	mov	eax, DSERR_NODRIVER
	ret	4
do_mem:
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
	mov	[dwPrimPlaying], 0
	call	Engine_Stop
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
	mov	dword ptr [edi], DSCAPS_OUR_FLAGS
	mov	dword ptr [edi+4], SEC_RATE_MIN
	mov	dword ptr [edi+8], SEC_RATE_MAX
	mov	dword ptr [edi+12], 1		; dwPrimaryBuffers
	mov	eax, SEC_N
	mov	[edi+16], eax			; dwMaxHwMixing All, Static, Streaming
	mov	[edi+20], eax
	mov	[edi+24], eax
	call	Sec_Free_Count
	mov	[edi+28], eax			; dwFreeHwMixing All, Static, Streaming
	mov	[edi+32], eax
	mov	[edi+36], eax
	xor	eax, eax
	pop	edi
	ret	8
EndProc Drv_GetCaps

;   (this, pwfx, dwFlags, dwCardAddress, pdwSize, ppbBuffer, ppvObj)
BeginProc Drv_CreateSoundBuffer
	test	dword ptr [esp+12], DSBCAPS_PRIMARYBUFFER
	jz	Sec_Create
	push	esi
	mov	eax, [esp+16]			; dwFlags
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
	mov	ecx, [dwStageLin]
	mov	[eax], ecx
	mov	eax, [esp+32]
	mov	dword ptr [eax], OFFSET32 BufObj
	mov	[dwBufRef], 1
	xor	eax, eax
dc_done:
	pop	esi
	ret	28
EndProc Drv_CreateSoundBuffer

;   (this, pSource, ppv): a new channel over the same memory, stopped, at 0.
BeginProc Drv_DuplicateSoundBuffer
	push	esi
	push	edi
	mov	esi, [esp+16]
	mov	eax, DSERR_INVALIDCALL
	cmp	esi, OFFSET32 SecSlots		; the primary cannot be duplicated
	jb	SHORT dd_done
	cmp	esi, OFFSET32 SecSlots + SEC_N * SEC_SIZE
	jae	SHORT dd_done
	call	Sec_Find_Free
	mov	eax, DSERR_ALLOCATED
	jc	SHORT dd_done
	push	edi
	mov	ecx, SEC_SIZE / 4
	cld
	rep	movsd
	pop	edi
	mov	dword ptr [edi+S_REF], 1
	mov	dword ptr [edi+S_STATE], SS_INUSE
	mov	dword ptr [edi+S_POS], 0
	mov	eax, [esp+20]
	mov	[eax], edi
	xor	eax, eax
dd_done:
	pop	edi
	pop	esi
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
	mov	[dwPrimPlaying], 0
	call	Engine_Idle_Check
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

;   DSOUND sets it only with the primary stopped (DDK guide), but channels may
;   still be playing: the ring restarts in the new format.
BeginProc Buf_SetFormat
	push	esi
	push	ebx
	mov	ebx, [dwEngine]
	call	Engine_Stop
	mov	esi, [esp+16]
	call	Set_Format
	mov	eax, DSERR_BADFORMAT
	jc	SHORT bf_restart
	xor	eax, eax
bf_restart:
	or	ebx, ebx
	jz	SHORT bf_done
	call	Engine_Start
bf_done:
	pop	ebx
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

;   (this, pdwPlay, pdwWrite): how far the tick has read the primary.
BeginProc Buf_GetPosition
	xor	eax, eax
	cmp	[dwEngine], 0
	je	SHORT bg_store
	mov	eax, [dwStageR]
	inc	[dwGetPos]
bg_store:
	mov	ecx, [esp+8]
	jecxz	bg_w
	mov	[ecx], eax
bg_w:
	mov	ecx, [esp+12]
	jecxz	bg_done
	mov	[ecx], eax
bg_done:
	xor	eax, eax
	ret	12
EndProc Buf_GetPosition

BeginProc Buf_Play
	mov	[dwPrimPlaying], 1
	call	Engine_Start
	xor	eax, eax
	ret	16
EndProc Buf_Play

BeginProc Buf_Stop
	mov	[dwPrimPlaying], 0
	call	Engine_Idle_Check
	xor	eax, eax
	ret	4
EndProc Buf_Stop

;   ---- secondary channels ------------------------------------------------------
;   A channel is a cursor, a rate and amp factors over DSOUND's memory. DSOUND
;   holds one per sound, playing or not; only playing channels take a voice
;   (a pair of volume tables) and are mixed.

;   EAX = free channels.
BeginProc Sec_Free_Count
	push	esi
	xor	eax, eax
	mov	esi, OFFSET32 SecSlots
sfc_next:
	test	dword ptr [esi+S_STATE], SS_INUSE
	jnz	SHORT sfc_used
	inc	eax
sfc_used:
	add	esi, SEC_SIZE
	cmp	esi, OFFSET32 SecSlots + SEC_N * SEC_SIZE
	jb	sfc_next
	pop	esi
	ret
EndProc Sec_Free_Count

;   EDI = a free channel, zeroed, with its vtable; CF set if none. EAX lost.
BeginProc Sec_Find_Free
	mov	edi, OFFSET32 SecSlots
sff_next:
	test	dword ptr [edi+S_STATE], SS_INUSE
	jz	SHORT sff_got
	add	edi, SEC_SIZE
	cmp	edi, OFFSET32 SecSlots + SEC_N * SEC_SIZE
	jb	sff_next
	stc
	ret
sff_got:
	push	edi
	push	ecx
	xor	eax, eax
	mov	ecx, SEC_SIZE / 4
	cld
	rep	stosd
	pop	ecx
	pop	edi
	mov	dword ptr [edi+S_VTBL], OFFSET32 SecVtbl
	clc
	ret
EndProc Sec_Find_Free

;   (this, pwfx, dwFlags, dwCardAddress, pdwSize, ppbBuffer, ppvObj), reached by
;   jump from Drv_CreateSoundBuffer. 8-bit PCM only: the SB Pro plays 8 bits, so
;   a 16-bit buffer is better left to DSOUND than truncated here.
BeginProc Sec_Create
	push	esi
	push	edi
	mov	eax, DSERR_CONTROLUNAVAIL
	test	dword ptr [esp+20], DSBCAPS_NOT_OURS
	jnz	sc_done
	mov	eax, DSERR_BADFORMAT
	mov	esi, [esp+16]
	or	esi, esi
	jz	sc_done
	cmp	word ptr [esi], 1		; WAVE_FORMAT_PCM
	jne	sc_done
	cmp	word ptr [esi+14], 8
	jne	sc_done
	movzx	ecx, word ptr [esi+2]
	or	ecx, ecx
	jz	sc_done
	cmp	ecx, 2
	ja	sc_done
	mov	edx, [esi+4]
	cmp	edx, SEC_RATE_MIN
	jb	sc_done
	cmp	edx, SEC_RATE_MAX
	ja	sc_done
	mov	eax, DSERR_INVALIDPARAM
	mov	esi, [esp+28]
	mov	esi, [esi]			; size
	or	esi, esi
	jz	sc_done
	call	Sec_Find_Free
	mov	eax, DSERR_ALLOCATED
	jc	sc_done
	mov	dword ptr [edi+S_REF], 1
	mov	dword ptr [edi+S_STATE], SS_INUSE
	mov	[edi+S_BYTES], esi
	mov	[edi+S_ALIGN], ecx
	mov	[edi+S_FREQ], edx
	mov	eax, [esp+32]
	mov	eax, [eax]			; DSOUND's memory for it
	mov	[edi+S_MEM], eax
	mov	eax, 10000h
	mov	[edi+S_AMPL], eax
	mov	[edi+S_AMPR], eax
	mov	eax, [esp+20]
	mov	[edi+S_CFLAGS], eax
	mov	eax, [esp+36]
	mov	[eax], edi
	xor	eax, eax
sc_done:
	pop	edi
	pop	esi
	ret	28
EndProc Sec_Create

;   ESI = channel. EAX = play position; at the end of a one-shot, the buffer
;   size (DSOUND then calls Stop - DDK, GetPosition).
BeginProc Sec_Pos
	mov	eax, [esi+S_POS]
	test	dword ptr [esi+S_STATE], SS_ENDED
	jz	SHORT sp_done
	mov	eax, [esi+S_BYTES]
sp_done:
	ret
EndProc Sec_Pos

;   ESI = channel. Lock its memory: the mixer reads it at interrupt time.
;   Runs in the caller's thread, where a page-in may wait.
BeginProc Sec_Lock_Mem
	test	dword ptr [esi+S_STATE], SS_LOCKED
	jnz	SHORT slm_done
	call	Sec_Page_Range
	VMMCall _LinPageLock, <eax, ecx, 0>
	or	eax, eax
	jz	SHORT slm_done
	or	dword ptr [esi+S_STATE], SS_LOCKED
slm_done:
	ret
EndProc Sec_Lock_Mem

BeginProc Sec_Unlock_Mem
	test	dword ptr [esi+S_STATE], SS_LOCKED
	jz	SHORT sum_done
	and	dword ptr [esi+S_STATE], NOT SS_LOCKED
	call	Sec_Page_Range
	VMMCall _LinPageUnlock, <eax, ecx, 0>
sum_done:
	ret
EndProc Sec_Unlock_Mem

;   ESI = channel -> EAX first page, ECX page count.
BeginProc Sec_Page_Range
	mov	eax, [esi+S_MEM]
	mov	ecx, eax
	add	ecx, [esi+S_BYTES]
	dec	ecx
	shr	eax, 12
	shr	ecx, 12
	sub	ecx, eax
	inc	ecx
	ret
EndProc Sec_Page_Range

;   ESI = channel. Give it a voice and build its tables; no voice free leaves it
;   silent (counted).
BeginProc Sec_Get_Voice
	cmp	dword ptr [esi+S_VOICE], 0
	jne	SHORT sgv_build
	push	ecx
	xor	ecx, ecx
sgv_next:
	bt	[dwVoiceUsed], ecx
	jnc	SHORT sgv_got
	inc	ecx
	cmp	ecx, VOICE_N
	jb	sgv_next
	pop	ecx
	inc	[dwVoiceFull]
	ret
sgv_got:
	bts	[dwVoiceUsed], ecx
	inc	ecx
	mov	[esi+S_VOICE], ecx
	inc	[dwVoicesNow]
	mov	ecx, [dwVoicesNow]
	cmp	ecx, [dwVoiceMax]
	jbe	SHORT sgv_max
	mov	[dwVoiceMax], ecx
sgv_max:
	pop	ecx
sgv_build:
	jmp	Sec_Build_Tables
EndProc Sec_Get_Voice

BeginProc Sec_Free_Voice
	mov	eax, [esi+S_VOICE]
	or	eax, eax
	jz	SHORT sfv_done
	mov	dword ptr [esi+S_VOICE], 0
	dec	eax
	btr	[dwVoiceUsed], eax
	dec	[dwVoicesNow]
sfv_done:
	ret
EndProc Sec_Free_Voice

;   ESI = channel with a voice: table[s] = (s - 80h) x amp >> 16, signed words,
;   left then right. No multiply is left for the mixer.
BeginProc Sec_Build_Tables
	pushad
	mov	edi, [esi+S_VOICE]
	or	edi, edi
	jz	SHORT sbt_done
	dec	edi
	shl	edi, 10
	add	edi, [dwVoiceTabs]
	mov	ebx, [esi+S_AMPL]
	call	Build_One_Table
	mov	ebx, [esi+S_AMPR]
	call	Build_One_Table
sbt_done:
	popad
	ret
EndProc Sec_Build_Tables

;   EBX = 16.16 amp, EDI = 256 words to fill (advanced past them).
BeginProc Build_One_Table
	mov	ecx, -80h
bot_next:
	mov	eax, ecx
	imul	eax, ebx
	sar	eax, 16
	mov	[edi], ax
	add	edi, 2
	inc	ecx
	cmp	ecx, 80h
	jl	bot_next
	ret
EndProc Build_One_Table

;   ESI = channel: stop it. Clears PLAYING before unlocking so the mixer, which
;   only interrupts us, never reads memory that is no longer locked.
BeginProc Sec_Halt
	test	dword ptr [esi+S_STATE], SS_PLAYING
	jz	SHORT sh_done
	and	dword ptr [esi+S_STATE], NOT (SS_PLAYING + SS_LOOP)
	test	dword ptr [esi+S_STATE], SS_ENDED
	jz	SHORT sh_keep
	and	dword ptr [esi+S_STATE], NOT SS_ENDED
	mov	dword ptr [esi+S_POS], 0	; a finished one-shot restarts at 0 (DDK, Play)
	mov	dword ptr [esi+S_FRAC], 0
sh_keep:
	call	Sec_Free_Voice
	call	Sec_Unlock_Mem
	call	Engine_Idle_Check
sh_done:
	ret
EndProc Sec_Halt

BeginProc Sec_QueryInterface
	push	esi
	push	edi
	mov	esi, [esp+16]
	mov	edx, [esp+20]
	mov	edi, OFFSET32 IID_IDsDriverBuffer
	call	Iid_Equal
	je	SHORT sq_yes
	mov	edi, OFFSET32 IID_IUnknown
	call	Iid_Equal
	je	SHORT sq_yes
	mov	dword ptr [edx], 0		; no property sets (3D)
	mov	eax, E_NOINTERFACE
	jmp	SHORT sq_done
sq_yes:
	mov	eax, [esp+12]
	mov	[edx], eax
	inc	dword ptr [eax+S_REF]
	xor	eax, eax
sq_done:
	pop	edi
	pop	esi
	ret	12
EndProc Sec_QueryInterface

BeginProc Sec_AddRef
	mov	eax, [esp+4]
	inc	dword ptr [eax+S_REF]
	mov	eax, [eax+S_REF]
	ret	4
EndProc Sec_AddRef

BeginProc Sec_Release
	push	esi
	mov	esi, [esp+8]
	xor	eax, eax
	cmp	[esi+S_REF], eax
	je	SHORT sl_done
	dec	dword ptr [esi+S_REF]
	jnz	SHORT sl_count
	call	Sec_Halt
	mov	dword ptr [esi+S_STATE], 0	; channel free
sl_count:
	mov	eax, [esi+S_REF]
sl_done:
	pop	esi
	ret	4
EndProc Sec_Release

BeginProc Sec_Lock
	mov	eax, DSERR_UNSUPPORTED		; DONTNEEDSECONDARYLOCK
	ret	32
EndProc Sec_Lock

BeginProc Sec_Unlock
	mov	eax, DSERR_UNSUPPORTED
	ret	20
EndProc Sec_Unlock

BeginProc Sec_SetFormat
	mov	eax, DSERR_UNSUPPORTED		; a channel keeps its creation format
	ret	8
EndProc Sec_SetFormat

BeginProc Sec_SetFrequency
	mov	ecx, [esp+4]
	mov	edx, [esp+8]
	mov	eax, DSERR_INVALIDPARAM
	cmp	edx, SEC_RATE_MIN
	jb	SHORT sfq_done
	cmp	edx, SEC_RATE_MAX
	ja	SHORT sfq_done
	mov	[ecx+S_FREQ], edx
	xor	eax, eax
sfq_done:
	ret	8
EndProc Sec_SetFrequency

;   DSVOLUMEPAN starts with dwTotalLeftAmpFactor, dwTotalRightAmpFactor (16.16).
BeginProc Sec_SetVolumePan
	push	esi
	mov	esi, [esp+8]
	mov	edx, [esp+12]
	mov	eax, [edx]
	mov	[esi+S_AMPL], eax
	mov	eax, [edx+4]
	mov	[esi+S_AMPR], eax
	call	Sec_Build_Tables
	xor	eax, eax
	pop	esi
	ret	8
EndProc Sec_SetVolumePan

BeginProc Sec_SetPosition
	mov	ecx, [esp+4]
	mov	edx, [esp+8]
	mov	eax, DSERR_INVALIDPARAM
	cmp	edx, [ecx+S_BYTES]
	jae	SHORT ss_done
	cmp	dword ptr [ecx+S_ALIGN], 2
	jne	SHORT ss_set
	and	edx, NOT 1
ss_set:
	pushfd
	cli
	mov	[ecx+S_POS], edx
	mov	dword ptr [ecx+S_FRAC], 0
	and	dword ptr [ecx+S_STATE], NOT SS_ENDED
	popfd
	xor	eax, eax
ss_done:
	ret	8
EndProc Sec_SetPosition

;   (this, pdwPlay, pdwWrite): write cursor = play cursor, as for the primary.
BeginProc Sec_GetPosition
	push	esi
	mov	esi, [esp+8]
	call	Sec_Pos
	mov	ecx, [esp+12]
	jecxz	sg_w
	mov	[ecx], eax
sg_w:
	mov	ecx, [esp+16]
	jecxz	sg_done
	mov	[ecx], eax
sg_done:
	xor	eax, eax
	pop	esi
	ret	12
EndProc Sec_GetPosition

;   (this, reserved, reserved, dwFlags)
BeginProc Sec_Play
	push	esi
	mov	esi, [esp+8]
	test	dword ptr [esi+S_STATE], SS_ENDED
	jz	SHORT sy_new
	and	dword ptr [esi+S_STATE], NOT SS_ENDED
	mov	dword ptr [esi+S_POS], 0
	mov	dword ptr [esi+S_FRAC], 0
sy_new:
	test	dword ptr [esi+S_STATE], SS_PLAYING
	jnz	SHORT sy_flags
	call	Sec_Lock_Mem
	test	dword ptr [esi+S_STATE], SS_LOCKED
	jz	SHORT sy_done			; cannot lock: stays silent rather than fault
	call	Sec_Get_Voice
sy_flags:
	pushfd
	cli
	and	dword ptr [esi+S_STATE], NOT SS_LOOP
	test	dword ptr [esp+24], DSBPLAY_LOOPING
	jz	SHORT sy_once
	or	dword ptr [esi+S_STATE], SS_LOOP
sy_once:
	test	dword ptr [esi+S_STATE], SS_PLAYING
	jnz	SHORT sy_on
	call	Late_Start
sy_on:
	or	dword ptr [esi+S_STATE], SS_PLAYING
	popfd
	call	Engine_Start
sy_done:
	xor	eax, eax
	pop	esi
	ret	16
EndProc Sec_Play

;   ESI = channel about to play, interrupts off, so no tick runs meanwhile. Adds
;   it into the ring between the 8237 (plus the guard) and the write point and
;   moves its cursor on; the next tick carries on from the write point. Asleep,
;   the ring is silence about to be rewritten: nothing to add to.
BeginProc Late_Start
	pushad
	cmp	[dwEngine], 0
	je	ls_done
	cmp	[dwPlaying], 0
	je	ls_done
	cmp	dword ptr [esi+S_VOICE], 0
	je	ls_done
	test	dword ptr [esi+S_STATE], SS_LOCKED
	jz	ls_done
	mov	ebx, [dwBufBytes]
	dec	ebx
	call	Dma_Pos
	add	eax, [dwLateGuard]
	and	eax, ebx
	cmp	[dwChannels], 2
	jne	SHORT ls_from
	and	eax, NOT 1
ls_from:
	mov	ecx, [dwWp]
	sub	ecx, eax
	and	ecx, ebx			; bytes from there to the write point
	jz	ls_done
	cmp	ecx, [dwLead]			; the 8237 is inside the guard: too late
	ja	ls_done
	cmp	ecx, MIX_MAX
	jbe	SHORT ls_n
	mov	ecx, MIX_MAX			; keep the end at the write point
	mov	eax, [dwWp]
	sub	eax, ecx
	and	eax, ebx
ls_n:
	mov	[mcBytes], ecx
	push	eax
	mov	edi, [dwScratch]
	xor	eax, eax
	cld
	rep	stosw
	call	Mix_Voice			; advances the cursor; may end a one-shot
	pop	edi
	mov	ecx, [mcBytes]
	mov	esi, [dwScratch]
	mov	ebp, [dwBufLin]
	mov	ebx, [dwBufBytes]
	dec	ebx
ls_add:
	movzx	eax, byte ptr [ebp+edi]
	sub	eax, 80h
	movsx	edx, word ptr [esi]
	add	eax, edx
	cmp	eax, 127
	jle	SHORT ls_hi
	mov	eax, 127
ls_hi:
	cmp	eax, -128
	jge	SHORT ls_lo
	mov	eax, -128
ls_lo:
	add	eax, 80h
	mov	[ebp+edi], al
	add	esi, 2
	inc	edi
	and	edi, ebx
	dec	ecx
	jnz	ls_add
	inc	[dwLateStarts]
	mov	eax, [mcBytes]
	add	[dwLateBytes], eax
ls_done:
	popad
	ret
EndProc Late_Start

BeginProc Sec_Stop
	push	esi
	mov	esi, [esp+8]
	call	Sec_Halt
	xor	eax, eax
	pop	esi
	ret	4
EndProc Sec_Stop

;   ---- the engine ----------------------------------------------------------------
;   DSOUND mixes its own buffers into the primary, which is ordinary memory here.
;   Every TICK_MS the tick adds the playing channels to it and writes the result
;   into the DMA ring, LEAD_MS ahead of where the 8237 reads. The primary's play
;   cursor is how far the tick has read it.

;   Runs the tick, and wakes the DMA if it sleeps. Caller's thread.
BeginProc Engine_Start
	cmp	[dwEngine], 0
	jne	SHORT es_wake
	pushad
	mov	[dwStageR], 0
	call	Engine_Wake
	mov	[dwEngine], 1
	inc	[dwEngineRuns]
	pushfd
	cli
	call	Engine_Tick			; the ring is written only the guard ahead
	popfd
	popad
	ret
es_wake:
	cmp	[dwPlaying], 0
	jne	SHORT es_done
	pushad
	pushfd
	cli					; the tick may be waking it too
	cmp	[dwPlaying], 0
	jne	SHORT es_up
	call	Engine_Wake
	inc	[dwWakes]
	mov	esi, [hTick]			; the slow sleeping tick would come too late
	mov	[hTick], 0
	or	esi, esi
	jz	SHORT es_arm
	VMMCall Cancel_Time_Out
es_arm:
	call	Engine_Tick			; mixes now and arms the next tick
es_up:
	popfd
	popad
es_done:
	ret
EndProc Engine_Start

;   Silence the ring, put the write point the guard ahead and start the DMA; the
;   caller ticks at once, so the sound that woke it does not wait out the lead.
;   The primary's read point carries on from where it was.
BeginProc Engine_Wake
	mov	edi, [dwBufLin]
	mov	ecx, [dwBufBytes]
	mov	al, 80h 			; 8-bit silence
	cld
	rep	stosb
	call	Lead_Bytes
	mov	eax, [dwLateGuard]
	mov	[dwWp], eax
	mov	[dwQuietMs], 0
	jmp	Start_Playback
EndProc Engine_Wake

;   [dwLead] = [dwLeadMs] in bytes, whole frames; also in EAX. ECX, EDX lost.
BeginProc Lead_Bytes
	mov	eax, [dwRate]
	mul	[dwChannels]
	mul	[dwLeadMs]
	mov	ecx, 1000
	div	ecx
	cmp	[dwChannels], 2
	jne	SHORT lb_set
	and	eax, NOT 1
lb_set:
	mov	[dwLead], eax
	push	eax
	mov	eax, [dwRate]
	mul	[dwChannels]
	mov	ecx, DEFER_MIN_MS
	mul	ecx
	mov	ecx, 1000
	div	ecx
	mov	[dwDeferMin], eax
	mov	eax, [dwRate]
	mul	[dwChannels]
	mov	ecx, LATE_GUARD_MS
	mul	ecx
	mov	ecx, 1000
	div	ecx
	cmp	[dwChannels], 2
	jne	SHORT lb_guard
	and	eax, NOT 1
lb_guard:
	mov	[dwLateGuard], eax
	pop	eax
	mov	ecx, [dwLeadMs]
	cmp	ecx, [dwLeadMaxMs]
	jbe	SHORT lb_done
	mov	[dwLeadMaxMs], ecx
lb_done:
	ret
EndProc Lead_Bytes

BeginProc Engine_Stop
	cmp	[dwEngine], 0
	je	SHORT et_done
	push	esi
	pushfd
	cli
	mov	[dwEngine], 0
	mov	esi, [hTick]
	mov	[hTick], 0
	or	esi, esi
	jz	SHORT et_none
	VMMCall Cancel_Time_Out
et_none:
	popfd
	pop	esi
	call	Stop_Playback
	call	Stats_Write
et_done:
	ret
EndProc Engine_Stop

;   HKLM\Software\SBDS "Stats": the engine's counters, for a run on the card.
BeginProc Stats_Write
	pushad
	mov	eax, [dwVoiceFull]
	mov	[dwVoiceFull2], eax
	push	OFFSET32 hkStats
	push	OFFSET32 szSbdsKey
	push	HKEY_LOCAL_MACHINE
	VMMCall _RegCreateKey
	add	esp, 3*4
	or	eax, eax
	jnz	SHORT sw_done
	push	STATS_DWORDS * 4
	push	OFFSET32 dwTicks
	push	REG_BINARY
	push	0
	push	OFFSET32 szStatsVal
	push	[hkStats]
	VMMCall _RegSetValueEx
	add	esp, 6*4
	push	[hkStats]
	VMMCall _RegCloseKey
	add	esp, 4
sw_done:
	popad
	ret
EndProc Stats_Write

BeginProc Engine_Idle_Check
	cmp	[dwPrimPlaying], 0
	jne	SHORT eic_done
	push	esi
	mov	esi, OFFSET32 SecSlots
eic_next:
	test	dword ptr [esi+S_STATE], SS_PLAYING
	jnz	SHORT eic_busy
	add	esi, SEC_SIZE
	cmp	esi, OFFSET32 SecSlots + SEC_N * SEC_SIZE
	jb	eic_next
	pop	esi
	jmp	Engine_Stop
eic_busy:
	pop	esi
eic_done:
	ret
EndProc Engine_Idle_Check

BeginProc Tick_Arm
	mov	eax, TICK_MS
	cmp	[dwPlaying], 0
	jne	SHORT Tick_Arm_Ms
	mov	eax, SLEEP_TICK_MS
	jmp	Tick_Arm_Ms
EndProc Tick_Arm

;   EAX = ms to the next tick.
BeginProc Tick_Arm_Ms
	push	esi
	xor	edx, edx
	mov	esi, OFFSET32 Engine_Tick
	VMMCall Set_Global_Time_Out
	mov	[hTick], esi
	pop	esi
	ret
EndProc Tick_Arm_Ms

;   EAX = the byte of the ring the 8237 reads next. Reading 22Eh acknowledges a
;   pending DSP interrupt, so auto-init never waits on an ISR that is not ours.
BeginProc Dma_Pos
	push	ebx
	push	ecx
	push	edx
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
	je	SHORT dp_have
	mov	bx, cx
dp_have:
	movzx	ebx, bx
	inc	ebx
	mov	eax, [dwBufBytes]
	sub	eax, ebx
	jae	SHORT dp_in
	xor	eax, eax
dp_in:
	cmp	eax, [dwBufBytes]
	jb	SHORT dp_ack
	xor	eax, eax
dp_ack:
	push	eax
	mov	dx, SB_RSTATUS
	in	al, dx
	pop	eax
	pop	edx
	pop	ecx
	pop	ebx
	ret
EndProc Dma_Pos

;   Time-out callback, interrupt time: touches only locked memory.
BeginProc Engine_Tick
	pushad
	mov	[hTick], 0
	cmp	[dwEngine], 0
	je	et_out
	inc	[dwTicks]
	mov	ebx, [dwBufBytes]
	dec	ebx				; ring mask (size is a power of two)
	cmp	[dwPlaying], 0
	je	et_asleep
	call	Dma_Pos
	mov	ecx, [dwWp]
	sub	ecx, eax
	and	ecx, ebx			; how far ahead of the 8237 we are
	mov	edx, [dwBufBytes]
	shr	edx, 1
	cmp	ecx, edx
	jbe	SHORT et_ahead
	inc	[dwUnderruns]			; the 8237 passed us: jump in front of it
	mov	[dwCalm], 0			; and keep further ahead from now on
	cmp	[dwLeadMs], LEAD_MAX_MS
	jae	SHORT et_bigger
	add	[dwLeadMs], 20
	push	eax
	call	Lead_Bytes
	pop	eax
et_bigger:
	mov	edx, eax
	add	edx, [dwChannels]
	add	edx, [dwChannels]
	and	edx, ebx
	cmp	[dwChannels], 2
	jne	SHORT et_jump
	and	edx, NOT 1
et_jump:
	mov	ecx, edx
	sub	ecx, [dwWp]
	and	ecx, ebx
	add	[dwStageR], ecx
	and	[dwStageR], ebx
	mov	[dwWp], edx
	xor	ecx, ecx
et_ahead:
	mov	eax, [dwHot]
	or	eax, eax
	jz	SHORT et_calm
	cmp	byte ptr [eax+H_BUSY], 0
	je	SHORT et_calm
	inc	[dwHotBusy]
	cmp	ecx, [dwDeferMin]
	jb	SHORT et_busymix
	inc	[dwDeferred]
	mov	eax, DEFER_MS
	call	Tick_Arm_Ms
	popad
	ret
et_busymix:
	inc	[dwBusyMixed]
et_calm:
	inc	[dwCalm]			; calm long enough: come back toward LEAD_MS
	cmp	[dwCalm], LEAD_CALM_TICKS
	jb	SHORT et_need
	mov	[dwCalm], 0
	cmp	[dwLeadMs], LEAD_MS
	jbe	SHORT et_need
	sub	[dwLeadMs], 20
	push	ecx
	call	Lead_Bytes
	pop	ecx
et_need:
	mov	eax, [dwLead]
	sub	eax, ecx
	jbe	et_rearm
	cmp	eax, MIX_MAX
	jbe	SHORT et_n
	mov	eax, MIX_MAX
et_n:
	cmp	[dwChannels], 2
	jne	SHORT et_mix
	and	eax, NOT 1
	jz	et_rearm
et_mix:
	mov	[mcAudible], 0
	call	Mix_Chunk
	cmp	[mcAudible], 0
	je	SHORT et_quiet
	mov	[dwQuietMs], 0
	jmp	et_rearm
et_quiet:
	add	[dwQuietMs], TICK_MS
	cmp	[dwQuietMs], QUIET_MS
	jb	et_rearm
	call	Stop_Playback			; the ring holds only silence by now
	inc	[dwSleeps]
	VMMCall Get_System_Time
	mov	[dwLastMs], eax
	jmp	et_rearm

;   Asleep: move the primary's read point on by the clock, so DSOUND sees time
;   pass, and wake as soon as it writes something that is not silence.
et_asleep:
	VMMCall Get_System_Time
	mov	ecx, eax
	sub	eax, [dwLastMs]
	mov	[dwLastMs], ecx
	mul	[dwRate]
	mul	[dwChannels]
	mov	ecx, 1000
	div	ecx				; bytes the primary would have played
	mov	ecx, [dwBufBytes]
	shr	ecx, 1
	cmp	eax, ecx
	jbe	SHORT et_scan
	mov	eax, ecx
et_scan:
	cmp	[dwChannels], 2
	jne	SHORT et_scan2
	and	eax, NOT 1
et_scan2:
	or	eax, eax
	jz	et_rearm
	mov	ecx, eax
	mov	esi, [dwStageR]
	mov	edi, [dwStageLin]
	cmp	[dwPrimPlaying], 0
	je	SHORT et_skip
et_look:
	cmp	byte ptr [edi+esi], 80h
	jne	SHORT et_wake
	inc	esi
	and	esi, ebx
	dec	ecx
	jnz	et_look
	mov	[dwStageR], esi
	jmp	et_rearm
et_skip:
	add	esi, eax
	and	esi, ebx
	mov	[dwStageR], esi
	jmp	et_rearm
et_wake:
	mov	[dwStageR], esi 		; from the first sound DSOUND wrote
	call	Engine_Wake
	inc	[dwWakes]
et_rearm:
	call	Tick_Arm
et_out:
	popad
	ret
EndProc Engine_Tick

;   The feeder: every store crosses the Inboard's bus (write-through), loads that
;   hit the cache are nearly free. So output bytes are packed four to a register
;   and stored as one dword, and a sound playing alone is mixed straight into the
;   ring with no scratch.

;   EAX = signed sample: clip to an unsigned byte, shift it into EDX; at each
;   dword boundary store EDX to the ring. EDI = ring pointer, advanced.
PUT_BYTE MACRO
	LOCAL	pb_ok, pb_zero, pb_next
	add	eax, 80h
	cmp	eax, 0FFh
	jbe	SHORT pb_ok
	test	eax, eax
	js	SHORT pb_zero
	mov	eax, 0FFh
	jmp	SHORT pb_ok
pb_zero:
	xor	eax, eax
pb_ok:
	shrd	edx, eax, 8
	inc	edi
	test	edi, 3
	jnz	SHORT pb_next
	mov	[edi-4], edx
	cmp	edi, [mcRingEnd]
	jb	SHORT pb_next
	sub	edi, [dwBufBytes]
pb_next:
	ENDM

;   EDI = ring pointer at [dwWp]. EDX = the ring's own bytes below EDI in a
;   part-filled dword, positioned so the packing completes it unchanged.
BeginProc Put_Begin
	mov	edi, [dwBufLin]
	mov	ecx, edi
	add	ecx, [dwBufBytes]
	mov	[mcRingEnd], ecx
	add	edi, [dwWp]
	mov	ecx, edi
	and	ecx, 3
	jz	SHORT pbg_done
	mov	edx, edi
	and	edx, NOT 3
	mov	edx, [edx]
	neg	ecx
	add	ecx, 4
	shl	ecx, 3				; 8 x (4 - bytes already there)
	shl	edx, cl
pbg_done:
	ret
EndProc Put_Begin

;   Store what the packing left in a part-filled dword, byte by byte, and set
;   [dwWp] from EDI.
BeginProc Put_End
	mov	ecx, edi
	and	ecx, 3
	jz	SHORT pe_wp
	push	ecx
	neg	ecx
	add	ecx, 4
	shl	ecx, 3
	shr	edx, cl
	pop	ecx
	sub	edi, ecx
pe_byte:
	mov	[edi], dl
	inc	edi
	shr	edx, 8
	dec	ecx
	jnz	pe_byte
pe_wp:
	sub	edi, [dwBufLin]
	mov	[dwWp], edi
	ret
EndProc Put_End

;   EAX = bytes to produce (whole frames, <= MIX_MAX), EBX = ring mask.
BeginProc Mix_Chunk
	mov	[mcBytes], eax

	mov	[mcPrimLoud], 0 		; does the primary hold anything but silence?
	cmp	[dwPrimPlaying], 0
	je	mc_count
	mov	ecx, eax
	mov	esi, [dwStageR]
	mov	ebp, [dwStageLin]
mc_scan:
	cmp	byte ptr [ebp+esi], 80h
	jne	SHORT mc_loud
	inc	esi
	and	esi, ebx
	dec	ecx
	jnz	mc_scan
	jmp	mc_count
mc_loud:
	mov	[mcPrimLoud], 1
	mov	[mcAudible], 1

mc_count:					; the channels to mix; the last one found
	xor	ecx, ecx
	mov	esi, OFFSET32 SecSlots
mc_cnext:
	mov	eax, [esi+S_STATE]
	and	eax, SS_PLAYING + SS_ENDED + SS_LOCKED
	cmp	eax, SS_PLAYING + SS_LOCKED
	jne	SHORT mc_cskip
	cmp	dword ptr [esi+S_VOICE], 0
	je	SHORT mc_cskip
	inc	ecx
	mov	edx, esi
mc_cskip:
	add	esi, SEC_SIZE
	cmp	esi, OFFSET32 SecSlots + SEC_N * SEC_SIZE
	jb	mc_cnext
	or	ecx, ecx
	jz	SHORT mc_nosound
	mov	[mcAudible], 1
	cmp	ecx, 1				; one mono sound and a silent primary: direct
	jne	mc_scratch
	cmp	[mcPrimLoud], 0
	jne	mc_scratch
	cmp	dword ptr [edx+S_ALIGN], 1
	jne	mc_scratch
	cmp	[dwChannels], 2
	je	SHORT mc_direct
	mov	eax, [edx+S_AMPL]		; mono out takes the left table
	cmp	eax, [edx+S_AMPR]
	jne	mc_scratch
mc_direct:
	mov	esi, edx
	push	ebx
	call	Mix_Direct
	pop	ebx
	jmp	mc_done

mc_nosound:
	cmp	[mcPrimLoud], 0
	jne	mc_scratch
	call	Put_Begin			; silence, a dword at a time
	mov	ecx, [mcBytes]
mc_sil:
	xor	eax, eax
	PUT_BYTE
	dec	ecx
	jnz	mc_sil
	call	Put_End
	jmp	mc_done

mc_scratch:
	mov	ecx, [mcBytes]
	mov	edi, [dwScratch]
	cmp	[mcPrimLoud], 0
	je	SHORT mc_zero
	mov	esi, [dwStageR]
	mov	ebp, [dwStageLin]
mc_prim:
	movzx	eax, byte ptr [ebp+esi]
	sub	eax, 80h
	mov	[edi], ax
	add	edi, 2
	inc	esi
	and	esi, ebx
	dec	ecx
	jnz	mc_prim
	jmp	mc_voices
mc_zero:
	xor	eax, eax
	cld
	rep	stosw
mc_voices:
	mov	esi, OFFSET32 SecSlots
mc_next:
	mov	eax, [esi+S_STATE]
	and	eax, SS_PLAYING + SS_ENDED + SS_LOCKED
	cmp	eax, SS_PLAYING + SS_LOCKED
	jne	SHORT mc_skip
	cmp	dword ptr [esi+S_VOICE], 0
	je	SHORT mc_skip
	push	esi
	push	ebx
	call	Mix_Voice
	pop	ebx
	pop	esi
mc_skip:
	add	esi, SEC_SIZE
	cmp	esi, OFFSET32 SecSlots + SEC_N * SEC_SIZE
	jb	mc_next

	call	Put_Begin			; scratch to the ring, clipped, packed
	mov	ecx, [mcBytes]
	mov	esi, [dwScratch]
mc_out:
	movsx	eax, word ptr [esi]
	PUT_BYTE
	add	esi, 2
	dec	ecx
	jnz	mc_out
	call	Put_End

mc_done:
	mov	eax, [mcBytes]
	add	eax, [dwStageR]
	and	eax, ebx
	mov	[dwStageR], eax
	ret
EndProc Mix_Chunk

;   ESI = the only channel playing, 8-bit mono source: straight to the ring.
;   Loop state lives in registers; the only stores are the packed dwords.
BeginProc Mix_Direct
	mov	[mvSlot], esi
	mov	eax, [esi+S_FREQ]		; step = rate / output rate, 16.16
	xor	edx, edx
	shld	edx, eax, 16
	shl	eax, 16
	div	[dwRate]
	mov	[mvStep], eax
	mov	eax, [esi+S_MEM]
	mov	[mvMem], eax
	add	eax, [esi+S_BYTES]
	mov	[mvEnd], eax
	mov	ebx, [esi+S_VOICE]
	dec	ebx
	shl	ebx, 10
	add	ebx, [dwVoiceTabs]
	mov	ebp, [esi+S_FRAC]
	mov	eax, [esi+S_POS]
	mov	esi, [esi+S_MEM]
	add	esi, eax
	call	Put_Begin
	mov	ecx, [mcBytes]
	cmp	[dwChannels], 2
	jne	md_mono
	shr	ecx, 1

md_stereo:
	movzx	eax, byte ptr [esi]
	movsx	eax, word ptr [ebx+eax*2]
	PUT_BYTE
	movzx	eax, byte ptr [esi]
	movsx	eax, word ptr [ebx+512+eax*2]
	PUT_BYTE
	add	ebp, [mvStep]
	mov	eax, ebp
	shr	eax, 16
	and	ebp, 0FFFFh
	add	esi, eax
	cmp	esi, [mvEnd]
	jae	SHORT mds_end
mds_cont:
	dec	ecx
	jnz	md_stereo
	jmp	md_save
mds_end:
	call	Md_End
	jnc	mds_cont
	dec	ecx				; one-shot over: the rest is silence
	shl	ecx, 1
	jmp	md_rest

md_mono:
	movzx	eax, byte ptr [esi]
	movsx	eax, word ptr [ebx+eax*2]
	PUT_BYTE
	add	ebp, [mvStep]
	mov	eax, ebp
	shr	eax, 16
	and	ebp, 0FFFFh
	add	esi, eax
	cmp	esi, [mvEnd]
	jae	SHORT mdm_end
mdm_cont:
	dec	ecx
	jnz	md_mono
	jmp	md_save
mdm_end:
	call	Md_End
	jnc	mdm_cont
	dec	ecx

md_rest:
	or	ecx, ecx			; bytes left after the frame just done
	jz	md_fin
md_sil:
	xor	eax, eax
	PUT_BYTE
	dec	ecx
	jnz	md_sil
	jmp	md_fin

md_save:
	mov	eax, [mvSlot]
	sub	esi, [mvMem]
	mov	[eax+S_POS], esi
	mov	[eax+S_FRAC], ebp
md_fin:
	jmp	Put_End
EndProc Mix_Direct

;   The source ran past its end. Looping: wrap ESI, CF clear. One-shot: mark the
;   channel ended, CF set.
BeginProc Md_End
	mov	eax, [mvSlot]
	test	dword ptr [eax+S_STATE], SS_LOOP
	jz	SHORT mde_once
	push	ecx
	mov	ecx, [eax+S_BYTES]
mde_wrap:
	sub	esi, ecx
	cmp	esi, [mvEnd]
	jae	mde_wrap
	pop	ecx
	clc
	ret
mde_once:
	push	ecx
	mov	ecx, [eax+S_BYTES]
	mov	[eax+S_POS], ecx
	mov	dword ptr [eax+S_FRAC], 0
	or	dword ptr [eax+S_STATE], SS_ENDED
	pop	ecx
	stc
	ret
EndProc Md_End

;   ESI = playing channel with a voice: add [mcBytes] bytes' worth of output
;   frames into the scratch, stepping the source by its own rate.
BeginProc Mix_Voice
	mov	eax, [mcBytes]
	cmp	[dwChannels], 2
	jne	SHORT mv_frames
	shr	eax, 1
mv_frames:
	mov	[mvCount], eax
	mov	[mvSlot], esi
	mov	eax, [esi+S_FREQ]		; step = rate / output rate, 16.16
	xor	edx, edx
	shld	edx, eax, 16
	shl	eax, 16
	div	[dwRate]
	mov	[mvStep], eax
	mov	eax, [esi+S_BYTES]
	cmp	dword ptr [esi+S_ALIGN], 2
	sete	byte ptr [mvSrcSt]
	jne	SHORT mv_tot
	shr	eax, 1
mv_tot:
	mov	[mvTotal], eax			; source frames
	mov	ebx, [esi+S_VOICE]
	dec	ebx
	shl	ebx, 10
	add	ebx, [dwVoiceTabs]
	mov	ebp, [esi+S_FRAC]
	mov	eax, [esi+S_POS]
	mov	ecx, eax
	cmp	byte ptr [mvSrcSt], 0
	je	SHORT mv_left
	shr	ecx, 1
mv_left:
	mov	edx, [mvTotal]
	sub	edx, ecx
	mov	[mvLeft], edx			; frames to the end
	mov	esi, [esi+S_MEM]
	mov	[mvMem], esi
	add	esi, eax
	mov	edi, [dwScratch]

mv_frame:
	movzx	eax, byte ptr [esi]
	mov	ecx, eax
	cmp	byte ptr [mvSrcSt], 0
	je	SHORT mv_mono
	movzx	ecx, byte ptr [esi+1]
mv_mono:
	mov	ax, [ebx+eax*2]
	mov	cx, [ebx+512+ecx*2]
	cmp	[dwChannels], 2
	jne	SHORT mv_outm
	add	[edi], ax
	add	[edi+2], cx
	add	edi, 4
	jmp	SHORT mv_adv
mv_outm:
	add	ax, cx
	sar	ax, 1
	add	[edi], ax
	add	edi, 2
mv_adv:
	add	ebp, [mvStep]
	mov	eax, ebp
	shr	eax, 16
	and	ebp, 0FFFFh
	sub	[mvLeft], eax
	jle	SHORT mv_end
	cmp	byte ptr [mvSrcSt], 0
	je	SHORT mv_step
	add	eax, eax
mv_step:
	add	esi, eax
mv_cont:
	dec	[mvCount]
	jnz	mv_frame
	jmp	SHORT mv_save

mv_end:
	mov	ecx, [mvSlot]
	test	dword ptr [ecx+S_STATE], SS_LOOP
	jz	SHORT mv_once
	mov	eax, [mvTotal]
mv_wrap:
	add	[mvLeft], eax
	cmp	[mvLeft], 0
	jle	mv_wrap
	sub	eax, [mvLeft]			; frame now
	cmp	byte ptr [mvSrcSt], 0
	je	SHORT mv_wpos
	add	eax, eax
mv_wpos:
	add	eax, [mvMem]
	mov	esi, eax
	jmp	SHORT mv_cont
mv_once:
	mov	eax, [ecx+S_BYTES]
	mov	[ecx+S_POS], eax
	mov	dword ptr [ecx+S_FRAC], 0
	or	dword ptr [ecx+S_STATE], SS_ENDED
	ret

mv_save:
	mov	ecx, [mvSlot]
	mov	eax, [mvTotal]
	sub	eax, [mvLeft]
	cmp	byte ptr [mvSrcSt], 0
	je	SHORT mv_spos
	add	eax, eax
mv_spos:
	mov	[ecx+S_POS], eax
	mov	[ecx+S_FRAC], ebp
	ret
EndProc Mix_Voice

IFDEF SBDS_TRACE
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
	TRACED	Sec_QueryInterface, 30, 12
	TRACED	Sec_AddRef, 31, 4
	TRACED	Sec_Release, 32, 4
	TRACED	Sec_Lock, 33, 32
	TRACED	Sec_Unlock, 34, 20
	TRACED	Sec_SetFormat, 35, 8
	TRACED	Sec_SetFrequency, 36, 8
	TRACED	Sec_SetVolumePan, 37, 8
	TRACED	Sec_SetPosition, 38, 8
	TRACED	Sec_GetPosition, 39, 12
	TRACED	Sec_Play, 40, 16
	TRACED	Sec_Stop, 41, 4

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

;   ECX = id, EBX = the wrapper's frame ([ebx+8] this, [ebx+12] the first argument
;   after it), EAX = the method's result (kept). Pointer arguments are logged as
;   what they point to: an IID's first dword, a format, a position, amp factors.
;   Channel methods (30-41) are stored as id | channel << 8 and decoded as the
;   buffer method 20 below them; their GetPosition is not logged.
BeginProc Trace_Add
	pushad
	mov	[dwTraceId], ecx
	cmp	ecx, 30
	jb	SHORT ta_plain
	cmp	ecx, 41
	ja	SHORT ta_plain
	cmp	ecx, 39
	je	ta_done
	mov	esi, [ebx+8]
	call	Slot_Of
	shl	esi, 8
	or	[dwTraceId], esi
	sub	ecx, 20
ta_plain:
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
	cmp	byte ptr [dwTraceId], 30	; a channel's Play leaves the primary's count
	jae	SHORT ta_chplay
	mov	[dwPosLogged], 0
ta_chplay:
	call	Trace_Ms
	jmp	SHORT ta_log
ta_notplay:
	cmp	ecx, 19 			; GetPosition: play cursor, time; first 32 per Play
	jne	SHORT ta_log
	cmp	[dwPosLogged], 32
	jae	ta_done
	inc	[dwPosLogged]
	call	Trace_Ms
	or	edi, edi
	jz	SHORT ta_log
	mov	edi, [edi]
ta_log:
	push	ecx
	mov	ecx, [dwTraceId]
	call	Ring_Put
	pop	ecx
	or	eax, eax			; a new channel: where its memory is (42, 43)
	jnz	SHORT ta_pub
	cmp	ecx, 8
	jne	SHORT ta_notnew
	test	dword ptr [ebx+16], DSBCAPS_PRIMARYBUFFER
	jnz	SHORT ta_pub
	mov	esi, [ebx+32]
	mov	edi, [ebx+28]
	mov	edi, [edi]			; DSOUND's pointer
	mov	ebp, [ebx+24]
	mov	ebp, [ebp]			; size
	mov	edx, 42
	jmp	SHORT ta_new
ta_notnew:
	cmp	ecx, 9
	jne	SHORT ta_pub
	mov	esi, [ebx+12]
	call	Slot_Of
	mov	ebp, esi			; source channel
	mov	esi, [ebx+16]
	mov	edi, [esi]
	mov	edi, [edi+S_MEM]
	mov	edx, 43
ta_new:
	mov	esi, [esi]			; the new channel
	call	Slot_Of
	shl	esi, 8
	or	esi, edx
	push	ecx
	mov	ecx, esi
	call	Ring_Put
	pop	ecx
ta_pub:
	cmp	byte ptr [dwTraceId], 30	; channel calls come per sound effect: the
	jae	SHORT ta_done			; primary's next call publishes them
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

;   ECX = id, EDI = a1, EBP = a2, EAX = result: one ring entry. EDX lost.
BeginProc Ring_Put
	mov	edx, [dwTraceN]
	and	edx, TRACE_N - 1
	shl	edx, 4
	add	edx, OFFSET32 TraceRing
	mov	[edx], ecx
	mov	[edx+4], edi
	mov	[edx+8], ebp
	mov	[edx+12], eax
	inc	[dwTraceN]
	ret
EndProc Ring_Put

;   ESI = channel object -> its index.
BeginProc Slot_Of
	sub	esi, OFFSET32 SecSlots
	shr	esi, 6
	ret
EndProc Slot_Of

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

;   HKLM\Software\SBDS "Trace" = dwTraceN, then the ring. Called from the
;   caller's thread through the vtable wrappers, never at interrupt time.
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
ENDIF

;   ESI = DIOCParams. EAX = 0 on success. CreateFile("\\.\SBDS") finds this
;   static device by name.
BeginProc SBDS_IoCtl
	mov	ecx, [esi.dwIoControlCode]
	cmp	ecx, DIOC_OPEN
	je	SHORT io_ok
	cmp	ecx, DIOC_CLOSEHANDLE
	je	SHORT io_ok
	cmp	ecx, IOCTL_SET_HOT
	jne	SHORT io_bad
	inc	[dwHotSets]
	cmp	[esi.cbInBuffer], 4
	jb	SHORT io_bad
	mov	eax, [esi.lpvInBuffer]
	mov	eax, [eax]
	or	eax, eax
	jz	SHORT io_bad
	cmp	eax, [dwHot]
	je	SHORT io_ok
	cmp	[dwHot], 0			; one display driver per boot: lock its page once
	jne	SHORT io_set
	push	eax
	shr	eax, 12 			; the tick reads it at interrupt time
	VMMCall _LinPageLock, <eax, 1, 0>
	or	eax, eax
	pop	eax
	jz	SHORT io_bad
io_set:
	mov	[dwHot], eax
io_ok:
	xor	eax, eax
	ret
io_bad:
	cmp	[esi.dwIoControlCode], IOCTL_SET_HOT	; ECX is gone after _LinPageLock
	jne	SHORT io_bad1
	inc	[dwHotBad]
io_bad1:
	mov	eax, 1
	ret
EndProc SBDS_IoCtl

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
	jmp	sdi_perf
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
	mov	eax, ebx
	VMMCall _PageAllocate <eax, PG_SYS, 0, 0, 0, 0, 0, PageFixed>
	or	eax, eax
	jz	sdi_perf
	mov	[dwStageLin], edx
	mov	edi, edx
	mov	ecx, [dwBufBytes]
	mov	al, 80h
	rep	stosb
	VMMCall _PageAllocate <WORK_PAGES, PG_SYS, 0, 0, 0, 0, 0, PageFixed+PageZeroInit>
	or	eax, eax
	jnz	SHORT sdi_work
	mov	[dwStageLin], 0
	jmp	sdi_perf
sdi_work:
	mov	[dwVoiceTabs], edx
	add	edx, VOICE_N * VOICE_BYTES
	mov	[dwScratch], edx

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
IFDEF SBDS_TRACE
	call	Trace_Write			; replace the last boot's trace
ENDIF
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
