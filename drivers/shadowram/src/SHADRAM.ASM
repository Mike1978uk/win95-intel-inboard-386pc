;******************************************************************************
;   SHADRAM.VXD - give Windows the Inboard's idle 64 KB of shadow RAM
;
;   The Inboard 386/PC reserves 128 KB of its RAM as two 64 KB shadow windows:
;   0x5F0000 holds the system BIOS copy, 0x5E0000 the EGA ROM copy. With
;   EGACACHE off nothing uses the EGA half, and Windows never sees it because
;   it lies above the top of the RAM INBRDPC.SYS reports
;   (docs/inboard_memory_layout_2026_09_30.md). This driver tests each page
;   of that window and adds the ones that pass to the VMM free pool, where
;   every program and driver can use them.
;
;   _AddFreePhysPage only works during initialisation, so the work is done
;   at Sys_Critical_Init; the counters are registered with PERF at
;   Device_Init, which is the earliest PERF can take them.
;
;   A page is refused if Windows already owns it, if a pattern does not read
;   back, if writing it changes the BIOS or video ROM area (the window would
;   then be an alias of live memory), or if two pages of the window alias
;   each other.
;
;   Counters: System Monitor / PERFLOG as SHADRAM\*.
;   Load: SYSTEM.INI [386Enh] device=C:\SHADRAM.VXD
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

ifndef SR_FIRST_PAGE
SR_FIRST_PAGE	EQU	5E0h		; physical page of the EGA shadow half
endif
SR_PAGES	EQU	16		; 64 KB
PAGE_BYTES	EQU	4096

; Status codes, so one PERFLOG column says why nothing was added.
SRS_OK		EQU	0
SRS_OWNED	EQU	1		; Windows already knows these pages
SRS_NOMAP	EQU	2		; _MapPhysToLinear refused the window
SRS_NOROM	EQU	3		; could not map the ROM areas for the alias check

Declare_Virtual_Device SHADRAM, 1, 0, ShadRam_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

VxD_LOCKED_DATA_SEG

	ALIGN 4
dwTested	dd	0		; pages examined
dwAdded 	dd	0		; pages the VMM accepted
dwFailed	dd	0		; pages refused by a test
dwStatus	dd	SRS_OK

; PERF may keep these pointers after init, so they cannot live in discardable data.
szSrvName	db	"Inboard shadow RAM",0
szSrvKey	db	"SHADRAM",0
szTested	db	"PagesTested",0
szAdded 	db	"PagesAdded",0
szFailed	db	"PagesFailed",0
szStatus	db	"Status",0

VxD_LOCKED_DATA_ENDS

VxD_IDATA_SEG

	ALIGN 4
linWin		dd	0		; ring-0 alias of the window
linBios 	dd	0		; ring-0 alias of F0000-FFFFF
linVideo	dd	0		; ring-0 alias of C0000-CFFFF
PassMask	dd	0		; bit n set = page n passed
SaveBuf 	dd	PAGE_BYTES / 4 dup (0)

VxD_IDATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc ShadRam_Control
	Control_Dispatch Sys_Critical_Init, ShadRam_Sys_Init
	Control_Dispatch Device_Init, ShadRam_Device_Init
	clc
	ret
EndProc ShadRam_Control

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG

;******************************************************************************
;   Sys_Critical_Init: test the window and donate what passes. Interrupts are
;   off here, so nothing else touches the window or the ROM areas meanwhile.
;   Never fails the load: a refusal just means no pages are added.
;******************************************************************************

BeginProc ShadRam_Sys_Init

	pushad

	VMMCall _GetPhysPageInfo, <SR_FIRST_PAGE, SR_PAGES, 0>
	cmp	eax, PHYSINFO_NONE
	je	SHORT ssi_map
	mov	[dwStatus], SRS_OWNED
	jmp	ssi_done

ssi_map:
	VMMCall _MapPhysToLinear, <SR_FIRST_PAGE * PAGE_BYTES, SR_PAGES * PAGE_BYTES, 0>
	cmp	eax, 0FFFFFFFFh
	jne	SHORT ssi_map_rom
	mov	[dwStatus], SRS_NOMAP
	jmp	ssi_done
ssi_map_rom:
	mov	[linWin], eax
	VMMCall _MapPhysToLinear, <0F0000h, 10000h, 0>
	cmp	eax, 0FFFFFFFFh
	je	SHORT ssi_norom
	mov	[linBios], eax
	VMMCall _MapPhysToLinear, <0C0000h, 10000h, 0>
	cmp	eax, 0FFFFFFFFh
	jne	SHORT ssi_have_rom
ssi_norom:
	mov	[dwStatus], SRS_NOROM
	jmp	ssi_done
ssi_have_rom:
	mov	[linVideo], eax

; Pass 1, page by page: save it, write two patterns, check each reads back
; and does not appear in the ROM areas at the same offset, then restore it.
	cld
	xor	ebx, ebx			; EBX = page index
ssi_page:
	inc	[dwTested]
	mov	esi, ebx
	shl	esi, 12
	add	esi, [linWin]			; ESI = this page
	push	esi
	mov	edi, OFFSET32 SaveBuf
	mov	ecx, PAGE_BYTES / 4
	rep	movsd
	pop	esi

	mov	eax, 55AA55AAh
	call	SR_Pattern
	jc	SHORT ssi_bad
	mov	eax, 0AA55AA55h
	call	SR_Pattern
	jc	SHORT ssi_bad
	bts	[PassMask], ebx
	jmp	SHORT ssi_restore
ssi_bad:
	inc	[dwFailed]
ssi_restore:
	mov	edi, esi
	push	esi
	mov	esi, OFFSET32 SaveBuf
	mov	ecx, PAGE_BYTES / 4
	rep	movsd
	pop	esi
	inc	ebx
	cmp	ebx, SR_PAGES
	jb	ssi_page

; Pass 2: every passing page holds its own physical address in each dword.
; If two pages of the window decode to the same RAM, one overwrites the other
; and the check below catches it.
	xor	ebx, ebx
ssi_fill:
	bt	[PassMask], ebx
	jnc	SHORT ssi_fill_next
	call	SR_Page_Address
	mov	ecx, PAGE_BYTES / 4
ssi_fill_dw:
	mov	[edi], eax
	add	edi, 4
	add	eax, 4
	dec	ecx
	jnz	ssi_fill_dw
ssi_fill_next:
	inc	ebx
	cmp	ebx, SR_PAGES
	jb	ssi_fill

	xor	ebx, ebx
ssi_check:
	bt	[PassMask], ebx
	jnc	SHORT ssi_check_next
	call	SR_Page_Address
	mov	ecx, PAGE_BYTES / 4
ssi_check_dw:
	cmp	[edi], eax
	jne	SHORT ssi_alias
	add	edi, 4
	add	eax, 4
	dec	ecx
	jnz	ssi_check_dw
	jmp	SHORT ssi_check_next
ssi_alias:
	btr	[PassMask], ebx
	inc	[dwFailed]
ssi_check_next:
	inc	ebx
	cmp	ebx, SR_PAGES
	jb	ssi_check

; Donate the survivors one page at a time, so one refusal costs one page.
	xor	ebx, ebx
ssi_add:
	bt	[PassMask], ebx
	jnc	SHORT ssi_add_next
	lea	eax, [ebx + SR_FIRST_PAGE]
	VMMCall _AddFreePhysPage, <eax, 1, 0>
	cmp	eax, 2				; all of the requested pages added
	jne	SHORT ssi_add_next
	inc	[dwAdded]
ssi_add_next:
	inc	ebx
	cmp	ebx, SR_PAGES
	jb	ssi_add

ssi_done:
	popad
	clc
	ret

EndProc ShadRam_Sys_Init

;******************************************************************************
;   SR_Pattern - fill the page at ESI with EAX, verify it, and check that the
;   BIOS and video ROM areas did not change to the pattern at the same offset.
;   ENTRY: ESI = page, EBX = page index, EAX = pattern
;   EXIT:  CF set on failure. ESI, EBX, EAX preserved.
;******************************************************************************

BeginProc SR_Pattern

	mov	edi, esi
	mov	ecx, PAGE_BYTES / 4
	rep	stosd
	mov	edi, esi
	mov	ecx, PAGE_BYTES / 4
	repe	scasd
	jne	SHORT srp_fail

	mov	edx, ebx
	shl	edx, 12
	mov	edi, [linBios]
	cmp	[edi + edx], eax
	je	SHORT srp_fail
	mov	edi, [linVideo]
	cmp	[edi + edx], eax
	je	SHORT srp_fail
	clc
	ret
srp_fail:
	stc
	ret

EndProc SR_Pattern

;   EBX = page index -> EDI = linear address of the page, EAX = its physical address
BeginProc SR_Page_Address
	mov	edi, ebx
	shl	edi, 12
	lea	eax, [edi + SR_FIRST_PAGE * PAGE_BYTES]
	add	edi, [linWin]
	ret
EndProc SR_Page_Address

;******************************************************************************
;   Device_Init: publish the counters. Without PERF they are still set, just
;   not visible.
;******************************************************************************

BeginProc ShadRam_Device_Init

	VxDCall PERF_Get_Version
	or	eax, eax
	jz	SHORT sdi_done

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
	jz	SHORT sdi_done
	mov	edx, eax

	mov	esi, OFFSET32 szTested
	mov	edi, OFFSET32 dwTested
	call	SR_Add_Stat
	mov	esi, OFFSET32 szAdded
	mov	edi, OFFSET32 dwAdded
	call	SR_Add_Stat
	mov	esi, OFFSET32 szFailed
	mov	edi, OFFSET32 dwFailed
	call	SR_Add_Stat
	mov	esi, OFFSET32 szStatus
	mov	edi, OFFSET32 dwStatus
	call	SR_Add_Stat

sdi_done:
	clc
	ret

EndProc ShadRam_Device_Init

;   EDX = server handle, ESI = name, EDI = DWORD counter (see SWAPCNT.ASM)
BeginProc SR_Add_Stat
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
EndProc SR_Add_Stat

VxD_ICODE_ENDS

	END
