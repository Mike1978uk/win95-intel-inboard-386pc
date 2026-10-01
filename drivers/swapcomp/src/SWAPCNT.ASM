;******************************************************************************
;   SWAPCNT.VXD - count swap-file I/O, change nothing
;
;   Stage 1 of the swap-compression track
;   (docs/magnaram_and_swap_compression_2026_10_01.md). Hooks
;   PageFile_Read_Or_Write, counts what passes through, and calls the
;   original. VMM's own cPageIns counts every page-in, including pages
;   re-read from EXEs and DLLs; these counters count only the swap file,
;   so the two together say how much paging compression could reach.
;
;   Counters appear in System Monitor (and PERFLOG) as SWAPCNT\*.
;
;   Load: SYSTEM.INI [386Enh] device=C:\SWAPCNT.VXD
;******************************************************************************

	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE PageFile.Inc
	INCLUDE PageSwap.Inc
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

Declare_Virtual_Device SWAPCNT, 1, 0, SwapCnt_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

;******************************************************************************
; Everything the hook touches is locked: it runs inside the pager, and a page
; fault there cannot be serviced.
;******************************************************************************

VxD_LOCKED_DATA_SEG

	ALIGN 4
SwapCnt_Prev	dd	0		; the original PageFile_Read_Or_Write

dwReads 	dd	0		; read calls
dwWrites	dd	0		; write calls
dwPagesRead	dd	0
dwPagesWritten	dd	0
dwZeroWritten	dd	0		; written pages that are all zero

; PERF may keep these pointers after init, so they cannot live in discardable data.
szSrvName	db	"Swap file I/O",0
szSrvKey	db	"SWAPCNT",0
szReads 	db	"Reads",0
szWrites	db	"Writes",0
szPagesRead	db	"PagesRead",0
szPagesWritten	db	"PagesWritten",0
szZeroWritten	db	"ZeroPagesWritten",0

VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc SwapCnt_Control
	Control_Dispatch Device_Init, SwapCnt_Device_Init
	clc
	ret
EndProc SwapCnt_Control

;******************************************************************************
;   ENTRY: EBX = PageSwapBufferDesc, ints enabled (PageFile_Read_Or_Write)
;   The descriptor is only read; status is set by the original service.
;******************************************************************************

BeginProc SwapCnt_Hook, HOOK_PROC, SwapCnt_Prev

	pushad
	movzx	ecx, [ebx].PS_BD_nPages
	cmp	[ebx].PS_BD_Cmd, PF_Write_Data
	je	SHORT sch_write

	inc	[dwReads]
	add	[dwPagesRead], ecx
	jmp	SHORT sch_done

sch_write:
	inc	[dwWrites]
	add	[dwPagesWritten], ecx

; Zero test, page by page. A write buffer is resident by definition.
	jecxz	SHORT sch_done
	mov	esi, [ebx].PS_BD_Buffer_Ptr
	cld
sch_page:
	mov	edi, esi
	push	ecx
	mov	ecx, 1000h / 4
	xor	eax, eax
	repe	scasd
	pop	ecx
	jne	SHORT sch_next
	inc	[dwZeroWritten]
sch_next:
	add	esi, 1000h
	dec	ecx
	jnz	sch_page

sch_done:
	popad
	jmp	[SwapCnt_Prev]

EndProc SwapCnt_Hook

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG

;******************************************************************************
;   Register the counters, then hook. If PERF is absent the hook still goes
;   in; the counts are then only readable with a debugger.
;******************************************************************************

BeginProc SwapCnt_Device_Init

	VxDCall PERF_Get_Version
	or	eax, eax
	jz	SHORT sdi_hook

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
	jz	SHORT sdi_hook
	mov	edx, eax

	mov	esi, OFFSET32 szReads
	mov	edi, OFFSET32 dwReads
	call	SwapCnt_Add_Stat
	mov	esi, OFFSET32 szWrites
	mov	edi, OFFSET32 dwWrites
	call	SwapCnt_Add_Stat
	mov	esi, OFFSET32 szPagesRead
	mov	edi, OFFSET32 dwPagesRead
	call	SwapCnt_Add_Stat
	mov	esi, OFFSET32 szPagesWritten
	mov	edi, OFFSET32 dwPagesWritten
	call	SwapCnt_Add_Stat
	mov	esi, OFFSET32 szZeroWritten
	mov	edi, OFFSET32 dwZeroWritten
	call	SwapCnt_Add_Stat

sdi_hook:
	GetVxDServiceOrdinal eax, PageFile_Read_Or_Write
	mov	esi, OFFSET32 SwapCnt_Hook
	VMMCall Hook_Device_Service
	clc				; a failed hook only means no counts
	ret

EndProc SwapCnt_Device_Init

;******************************************************************************
;   ENTRY: EDX = server handle, ESI = name (used for UI and registry key),
;	   EDI = DWORD counter
;   PERF_Server_Add_Stat(handle, &perf_stat_0): level, flags, name, node,
;   reserved, description, data pointer, scale factor
;******************************************************************************

BeginProc SwapCnt_Add_Stat

	push	edx
	push	0			; scale factor (no PSTF_FACTOR flag)
	push	edi
	push	esi			; description
	push	0			; reserved, must be NULL
	push	esi			; registry node name
	push	esi
	push	PSTF_COUNT
	push	0
	push	esp
	push	edx
	VxDCall PERF_Server_Add_Stat
	add	esp, 10*4
	pop	edx
	ret

EndProc SwapCnt_Add_Stat

VxD_ICODE_ENDS

	END
