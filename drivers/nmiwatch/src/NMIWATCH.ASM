;******************************************************************************
;   NMIWATCH.VXD - count NMIs and the XT's parity / I/O channel check latches
;
;   On an XT the NMI sources are latched in port 62h: bit 7 = RAM parity check,
;   bit 6 = I/O channel check (the Inboard's "Parity Check 2"). With neither set,
;   an NMI is the coprocessor. This driver changes nothing: its NMI handler
;   counts and records port 62h, then passes the NMI to the previous handler,
;   and a one-second timer reads the latches without clearing them.
;
;   It loads before the drivers with no init order (SHADRAM, SWAPCOMP), so the
;   entry snapshot is the machine as the VMM found it after the switch to
;   protected mode.
;
;   Counters: System Monitor / PERFLOG as NMIWATCH\*.
;   Load: SYSTEM.INI [386Enh] device=C:\NMIWATCH.VXD
;******************************************************************************

	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	.LIST

; PERF.INC is not in this DDK; the table is from NEW95DOC\VXDS.DOC, chapter 7.
Begin_Service_Table PERF
PERF_Service PERF_Get_Version, LOCAL
PERF_Service PERF_Server_Register, LOCAL
PERF_Service PERF_Server_Deregister, LOCAL
PERF_Service PERF_Server_Add_Stat, LOCAL
PERF_Service PERF_Server_Remove_Stat, LOCAL
End_Service_Table PERF

PSTF_COUNT	EQU	0
POLL_MS 	EQU	1000
NW_CHECKS	EQU	0C0h		; port 62h: parity (80h) and I/O channel (40h)

NW_Init_Order	EQU	Undefined_Init_Order - 1000h

Declare_Virtual_Device NMIWATCH, 1, 0, NmiWatch_Control, Undefined_Device_ID, \
		       NW_Init_Order

VxD_LOCKED_DATA_SEG

	ALIGN 4
pfnPrevNmi	dd	0		; next handler in the NMI chain
dwNmi		dd	0		; NMIs received
dwNmiParity	dd	0		;   with port 62h bit 7 set
dwNmiIoChk	dd	0		;   with port 62h bit 6 set
dwNmiOther	dd	0		;   with neither (coprocessor)
dwNmiLast62	dd	0		; port 62h at the last NMI
dwNmiFirstMs	dd	0		; system time of the first NMI, ms
dwEntry 	dd	0		; port 61h << 8 | port 62h at Sys_Critical_Init
dwInitDone	dd	0		; the same at Init_Complete
dwPollSecs	dd	0		; polls that found a latch set
dwPollEvents	dd	0		; polls that found it newly set
dwPollLast62	dd	0		; port 62h at the last such poll
dwPollFirstMs	dd	0		; system time a latch was first seen, ms
bPrevLatch	db	0

; PERF keeps these pointers after init, so they cannot live in discardable data.
szSrvName	db	"NMI and check latches",0
szSrvKey	db	"NMIWATCH",0
szNmi		db	"Nmi",0
szNmiParity	db	"NmiParity",0
szNmiIoChk	db	"NmiIoChk",0
szNmiOther	db	"NmiOther",0
szNmiLast62	db	"NmiLast62",0
szNmiFirstMs	db	"NmiFirstMs",0
szEntry 	db	"PortsEntry",0
szInitDone	db	"PortsInitDone",0
szPollSecs	db	"LatchedSecs",0
szPollEvents	db	"LatchEvents",0
szPollLast62	db	"LatchLast62",0
szPollFirstMs	db	"LatchFirstMs",0

VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc NmiWatch_Control
	Control_Dispatch Sys_Critical_Init, NmiWatch_Sys_Init
	Control_Dispatch Device_Init, NmiWatch_Device_Init
	Control_Dispatch Init_Complete, NmiWatch_Init_Complete
	clc
	ret
EndProc NmiWatch_Control

;******************************************************************************
;   NMI handler. It may not call any service, may touch only locked data, and
;   must end by jumping to the previous handler, never IRET. The NMI can arrive
;   from V86 mode, where DS is not set up for us, so data goes through SS: the
;   ring-0 stack segment is flat.
;******************************************************************************

BeginProc NmiWatch_NMI
	pushfd
	push	eax
	in	al, 62h
	inc	dword ptr ss:[dwNmi]
	mov	byte ptr ss:[dwNmiLast62], al
	test	al, 80h
	jz	SHORT nn_not_parity
	inc	dword ptr ss:[dwNmiParity]
nn_not_parity:
	test	al, 40h
	jz	SHORT nn_not_iochk
	inc	dword ptr ss:[dwNmiIoChk]
nn_not_iochk:
	test	al, NW_CHECKS
	jnz	SHORT nn_chain
	inc	dword ptr ss:[dwNmiOther]
nn_chain:
	pop	eax
	popfd
	jmp	dword ptr ss:[pfnPrevNmi]
EndProc NmiWatch_NMI

;   NMI event: runs after the handler chain, where services are allowed.
BeginProc NmiWatch_NMI_Event
	cmp	[dwNmiFirstMs], 0
	jne	SHORT nne_done
	push	eax
	VMMCall Get_System_Time
	or	eax, 1				; 0 means "not yet"
	mov	[dwNmiFirstMs], eax
	pop	eax
nne_done:
	ret
EndProc NmiWatch_NMI_Event

;   One-second poll of the latches. Reads only; re-arms itself.
BeginProc NmiWatch_Poll
	pushad
	in	al, 62h
	mov	ah, al
	and	al, NW_CHECKS
	jz	SHORT np_clear
	inc	[dwPollSecs]
	mov	byte ptr [dwPollLast62], ah
	cmp	[bPrevLatch], 0
	jne	SHORT np_seen
	inc	[dwPollEvents]
	cmp	[dwPollFirstMs], 0
	jne	SHORT np_seen
	push	eax
	VMMCall Get_System_Time
	or	eax, 1
	mov	[dwPollFirstMs], eax
	pop	eax
np_seen:
np_clear:
	mov	[bPrevLatch], al
	mov	eax, POLL_MS
	xor	edx, edx
	mov	esi, OFFSET32 NmiWatch_Poll
	VMMCall Set_Global_Time_Out
	popad
	ret
EndProc NmiWatch_Poll

;   EAX <- port 61h << 8 | port 62h
BeginProc NmiWatch_Ports
	in	al, 61h
	mov	ah, al
	in	al, 62h
	movzx	eax, ax
	ret
EndProc NmiWatch_Ports

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG

;   Sys_Critical_Init: snapshot the ports and join the NMI chain.
BeginProc NmiWatch_Sys_Init
	call	NmiWatch_Ports
	mov	[dwEntry], eax
	VMMCall Get_NMI_Handler_Addr
	mov	[pfnPrevNmi], esi
	mov	esi, OFFSET32 NmiWatch_NMI
	VMMCall Set_NMI_Handler_Addr
	mov	esi, OFFSET32 NmiWatch_NMI_Event
	VMMCall Hook_NMI_Event
	clc
	ret
EndProc NmiWatch_Sys_Init

;   Device_Init: publish the counters and start the poll.
BeginProc NmiWatch_Device_Init

	mov	eax, POLL_MS
	xor	edx, edx
	mov	esi, OFFSET32 NmiWatch_Poll
	VMMCall Set_Global_Time_Out

	VxDCall PERF_Get_Version
	or	eax, eax
	jz	ndi_done

; PERF_Server_Register(&perf_server_0): level, flags, name, node, control
	push	0
	push	OFFSET32 szSrvKey
	push	OFFSET32 szSrvName
	push	0
	push	0
	push	esp
	VxDCall PERF_Server_Register
	add	esp, 6*4
	or	eax, eax
	jz	ndi_done
	mov	edx, eax

	mov	esi, OFFSET32 szNmi
	mov	edi, OFFSET32 dwNmi
	call	NW_Add_Stat
	mov	esi, OFFSET32 szNmiParity
	mov	edi, OFFSET32 dwNmiParity
	call	NW_Add_Stat
	mov	esi, OFFSET32 szNmiIoChk
	mov	edi, OFFSET32 dwNmiIoChk
	call	NW_Add_Stat
	mov	esi, OFFSET32 szNmiOther
	mov	edi, OFFSET32 dwNmiOther
	call	NW_Add_Stat
	mov	esi, OFFSET32 szNmiLast62
	mov	edi, OFFSET32 dwNmiLast62
	call	NW_Add_Stat
	mov	esi, OFFSET32 szNmiFirstMs
	mov	edi, OFFSET32 dwNmiFirstMs
	call	NW_Add_Stat
	mov	esi, OFFSET32 szEntry
	mov	edi, OFFSET32 dwEntry
	call	NW_Add_Stat
	mov	esi, OFFSET32 szInitDone
	mov	edi, OFFSET32 dwInitDone
	call	NW_Add_Stat
	mov	esi, OFFSET32 szPollSecs
	mov	edi, OFFSET32 dwPollSecs
	call	NW_Add_Stat
	mov	esi, OFFSET32 szPollEvents
	mov	edi, OFFSET32 dwPollEvents
	call	NW_Add_Stat
	mov	esi, OFFSET32 szPollLast62
	mov	edi, OFFSET32 dwPollLast62
	call	NW_Add_Stat
	mov	esi, OFFSET32 szPollFirstMs
	mov	edi, OFFSET32 dwPollFirstMs
	call	NW_Add_Stat

ndi_done:
	clc
	ret

EndProc NmiWatch_Device_Init

;   Init_Complete: snapshot the ports again, after every driver's init.
BeginProc NmiWatch_Init_Complete
	call	NmiWatch_Ports
	mov	[dwInitDone], eax
	clc
	ret
EndProc NmiWatch_Init_Complete

;   EDX = server handle, ESI = name, EDI = DWORD counter (see SWAPCNT.ASM)
BeginProc NW_Add_Stat
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
EndProc NW_Add_Stat

VxD_ICODE_ENDS

	END
