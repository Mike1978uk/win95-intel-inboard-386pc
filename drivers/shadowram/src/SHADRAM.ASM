;******************************************************************************
;   SHADRAM.VXD - give Windows the Inboard's idle 64 KB of shadow RAM
;
;   The card's main RAM bank ends in 128 KB that only appears at 580000-5FFFFF,
;   and only while port 670h bit 0 (ROMCACHE) is clear: 5F0000 holds the BIOS
;   copy, 5E0000 is never used. With the bit set - as INBRDPC.SYS leaves it -
;   the BIOS copy serves F0000 and the 64 KB has no address at all
;   (docs/romcache_decode_2026_10_03.md).
;
;   So at Sys_Critical_Init this driver:
;   1. clears the bit the way INBRDPC's own "ROM cache off" routine (0B60h)
;      does, including its resident [02BF], which its INT 13h wrapper ORs
;      into every later write of port 670h - leave it set and the next
;      floppy call closes the window under Windows;
;   2. checks 5F0000 now holds the BIOS copy, i.e. the window is open;
;   3. writes the 64 KB at 5E0000 before reading any of it - the card checks
;      parity on every read, and this RAM is never written at power-on -
;      with NMI masked and the I/O channel check polled, as INBRDPC does;
;   4. tests each page and gives the ones that pass to the VMM;
;   5. maps V86 F0000-FFFFF onto the BIOS copy in every VM, so BIOS calls
;      still run from the card's RAM rather than the 8-bit ROM.
;
;   Nothing is changed unless INBRDPC is found, the pages are free and V86
;   pages F0-FF can be assigned. If the window does not open, the bit and
;   [02BF] are put back.
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

SR_FIRST_PAGE	EQU	5E0h		; physical page of the idle half
SR_BIOS_PAGE	EQU	5F0h		; physical page of the BIOS copy
SR_PAGES	EQU	16		; 64 KB
PAGE_BYTES	EQU	4096
V86_BIOS_PAGE	EQU	0F0h

ROMCACHE_PORT	EQU	670h
INB_NORMAL	EQU	02ACh		; INBRDPC: normal speed byte
INB_CACHE	EQU	02BFh		; INBRDPC: bit 0 value for port 670h
COPY_MAX_DIFF	EQU	100h		; dwords the BIOS copy may differ from the ROM

; Status codes, so one PERFLOG column says why nothing was added.
SRS_OK		EQU	0
SRS_OWNED	EQU	1		; Windows already knows these pages
SRS_NOMAP	EQU	2		; _MapPhysToLinear refused an area
SRS_NOINB	EQU	4		; INBRDPC.SYS not found or not the known build
SRS_V86 	EQU	5		; V86 pages F0-FF already assigned
SRS_NOWIN	EQU	6		; 5F0000 is not the BIOS copy: window did not open
SRS_PARITY	EQU	7		; I/O channel check during the page tests

Declare_Virtual_Device SHADRAM, 2, 0, ShadRam_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

VxD_LOCKED_DATA_SEG

	ALIGN 4
dwTested	dd	0		; pages examined
dwAdded 	dd	0		; pages the VMM accepted
dwFailed	dd	0		; pages refused by a test
dwStatus	dd	SRS_OK
dwBiosPage	dd	0		; physical page V86 F0000 is mapped to (0 = not ours)
dwMapped	dd	0		; VMs mapped

; PERF may keep these pointers after init, so they cannot live in discardable data.
szSrvName	db	"Inboard shadow RAM",0
szSrvKey	db	"SHADRAM",0
szTested	db	"PagesTested",0
szAdded 	db	"PagesAdded",0
szFailed	db	"PagesFailed",0
szStatus	db	"Status",0
szBiosPage	db	"BiosPage",0
szMapped	db	"VMsMapped",0

VxD_LOCKED_DATA_ENDS

VxD_IDATA_SEG

	ALIGN 4
linWin		dd	0		; ring-0 alias of 5E0000
linCopy 	dd	0		; ring-0 alias of 5F0000
linBios 	dd	0		; ring-0 alias of F0000-FFFFF
linVideo	dd	0		; ring-0 alias of C0000-CFFFF
linInb		dd	0		; ring-0 address of INBRDPC's resident image
PassMask	dd	0		; bit n set = page n passed
PgArray 	dd	9 dup (0)	; V86 page assignment bits, 110h pages

VxD_IDATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc ShadRam_Control
	Control_Dispatch Sys_Critical_Init, ShadRam_Sys_Init
	Control_Dispatch Device_Init, ShadRam_Device_Init
	Control_Dispatch Sys_VM_Init, ShadRam_Map_VM
	Control_Dispatch Create_VM, ShadRam_Map_VM
	clc
	ret
EndProc ShadRam_Control

;   EBX = VM. Map its V86 F0000-FFFFF onto dwBiosPage, if the pages are ours.
BeginProc ShadRam_Map_VM
	mov	eax, [dwBiosPage]
	or	eax, eax
	jz	SHORT smv_done
	VMMCall _PhysIntoV86, <eax, ebx, V86_BIOS_PAGE, SR_PAGES, 0>
	or	eax, eax
	jz	SHORT smv_done
	inc	[dwMapped]
smv_done:
	clc
	ret
EndProc ShadRam_Map_VM

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG

;******************************************************************************
;   Sys_Critical_Init. Interrupts are off. EBX = system VM, EDX = INBRDPC's
;   segment from the real-mode init (0 if not found). Never fails the load.
;******************************************************************************

BeginProc ShadRam_Sys_Init

	pushad

	or	edx, edx
	jnz	SHORT ssi_have_inb
	mov	[dwStatus], SRS_NOINB
	jmp	ssi_done
ssi_have_inb:
	shl	edx, 4
	add	edx, [ebx.CB_High_Linear]
	mov	[linInb], edx

	VMMCall _GetPhysPageInfo, <SR_FIRST_PAGE, SR_PAGES, 0>
	cmp	eax, PHYSINFO_NONE
	je	SHORT ssi_v86
	mov	[dwStatus], SRS_OWNED
	jmp	ssi_done

; V86 F0-FF must be free to take over; assign them now, before changing anything.
ssi_v86:
	VMMCall _Get_Device_V86_Pages_Array, <0, <OFFSET32 PgArray>, 0>
	or	eax, eax
	jz	SHORT ssi_v86_bad
	test	[PgArray + 7*4], 0FFFF0000h	; bits F0h-FFh
	jnz	SHORT ssi_v86_bad
	VMMCall _Assign_Device_V86_Pages, <V86_BIOS_PAGE, SR_PAGES, 0, 0>
	or	eax, eax
	jnz	SHORT ssi_v86_ok
ssi_v86_bad:
	mov	[dwStatus], SRS_V86
	jmp	ssi_done
ssi_v86_ok:
	mov	[dwBiosPage], V86_BIOS_PAGE	; ROM, until the copy is proven

	VMMCall _MapPhysToLinear, <SR_FIRST_PAGE * PAGE_BYTES, SR_PAGES * PAGE_BYTES, 0>
	cmp	eax, 0FFFFFFFFh
	je	SHORT ssi_nomap
	mov	[linWin], eax
	VMMCall _MapPhysToLinear, <SR_BIOS_PAGE * PAGE_BYTES, SR_PAGES * PAGE_BYTES, 0>
	cmp	eax, 0FFFFFFFFh
	je	SHORT ssi_nomap
	mov	[linCopy], eax
	VMMCall _MapPhysToLinear, <0F0000h, 10000h, 0>
	cmp	eax, 0FFFFFFFFh
	je	SHORT ssi_nomap
	mov	[linBios], eax
	VMMCall _MapPhysToLinear, <0C0000h, 10000h, 0>
	cmp	eax, 0FFFFFFFFh
	jne	SHORT ssi_have_maps
ssi_nomap:
	mov	[dwStatus], SRS_NOMAP
	jmp	ssi_done
ssi_have_maps:
	mov	[linVideo], eax

; 1. Clear ROMCACHE as INBRDPC's routine at 0B60h does.
	mov	esi, [linInb]
	mov	byte ptr [esi + INB_CACHE], 0
	call	SR_Write_670

; 2. The window is open if 5F0000 now reads as the BIOS (INBRDPC patches a few dwords).
	mov	esi, [linCopy]
	mov	edi, [linBios]
	mov	ecx, 10000h / 4
	xor	edx, edx
ssi_cmp:
	mov	eax, [esi]
	cmp	eax, [edi]
	je	SHORT ssi_cmp_next
	inc	edx
ssi_cmp_next:
	add	esi, 4
	add	edi, 4
	dec	ecx
	jnz	ssi_cmp
	cmp	edx, COPY_MAX_DIFF
	jb	SHORT ssi_open
	mov	esi, [linInb]			; put INBRDPC's state back
	mov	byte ptr [esi + INB_CACHE], 1
	call	SR_Write_670
	mov	[dwStatus], SRS_NOWIN
	jmp	ssi_done
ssi_open:
	mov	[dwBiosPage], SR_BIOS_PAGE

; 3. NMI masked and the latch cleared, as INBRDPC does around its memory tests.
	mov	al, 0
	out	0A0h, al
	call	SR_Clear_Check

; Write every dword before any read, so parity is valid.
	cld
	mov	edi, [linWin]
	mov	ecx, SR_PAGES * PAGE_BYTES / 4
	xor	eax, eax
	rep	stosd

; 4a. Page by page: two patterns, each read back and checked absent from the ROM areas.
	xor	ebx, ebx			; EBX = page index
ssi_page:
	inc	[dwTested]
	mov	esi, ebx
	shl	esi, 12
	add	esi, [linWin]			; ESI = this page
	mov	eax, 55AA55AAh
	call	SR_Pattern
	jc	SHORT ssi_bad
	mov	eax, 0AA55AA55h
	call	SR_Pattern
	jc	SHORT ssi_bad
	bts	[PassMask], ebx
	jmp	SHORT ssi_page_next
ssi_bad:
	inc	[dwFailed]
ssi_page_next:
	inc	ebx
	cmp	ebx, SR_PAGES
	jb	ssi_page

; 4b. Every passing page holds its own physical address in each dword; if two
; pages decode to the same RAM, one overwrites the other and the check catches it.
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

; Leave the pages zeroed, then read the I/O channel check: any parity error
; during the tests refuses every page.
	mov	edi, [linWin]
	mov	ecx, SR_PAGES * PAGE_BYTES / 4
	xor	eax, eax
	rep	stosd
	in	al, 62h
	test	al, 40h
	jz	SHORT ssi_nmi_on
	mov	[dwStatus], SRS_PARITY
	mov	[PassMask], 0
ssi_nmi_on:
	call	SR_Clear_Check
	mov	al, 80h
	out	0A0h, al

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

;   Write port 670h as INBRDPC does: wait states from its normal byte, bit 0 from [02BF].
BeginProc SR_Write_670
	mov	esi, [linInb]
	mov	al, [esi + INB_NORMAL]
	not	al
	and	al, 1Eh
	or	al, [esi + INB_CACHE]
	mov	dx, ROMCACHE_PORT
	out	dx, al
	ret
EndProc SR_Write_670

;   Pulse port 61h bit 5 to clear the I/O channel check latch (INBRDPC 9C0Bh).
BeginProc SR_Clear_Check
	in	al, 61h
	or	al, 20h
	out	61h, al
	and	al, 0DFh
	out	61h, al
	ret
EndProc SR_Clear_Check

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
	mov	esi, OFFSET32 szBiosPage
	mov	edi, OFFSET32 dwBiosPage
	call	SR_Add_Stat
	mov	esi, OFFSET32 szMapped
	mov	edi, OFFSET32 dwMapped
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

;******************************************************************************
;   Real-mode init: find INBRDPC.SYS in the DOS device chain and check its
;   resident code is the build whose offsets this driver uses. EDX = its
;   segment, or 0. Always loads, so the counters can say why nothing happened.
;******************************************************************************

VxD_REAL_INIT_SEG

BeginProc ShadRam_Real_Init

	push	cs
	pop	ds
	xor	edx, edx
	mov	ah, 52h 			; ES:BX = DOS list of lists
	int	21h
	add	bx, 22h 			; NUL device header
sri_dev:
	cmp	bx, 0FFFFh
	je	SHORT sri_exit
	push	es
	push	bx
	lea	di, [bx + 0Ah]			; device name
	mov	si, OFFSET szInbName
	mov	cx, 8
	cld
	repe	cmpsb
	pop	bx
	pop	es
	je	SHORT sri_found
	les	bx, es:[bx]			; next header
	jmp	sri_dev

sri_found:
	or	bx, bx				; a device driver loads at offset 0
	jnz	SHORT sri_exit
	mov	di, 0B60h			; "ROM cache off" routine
	mov	si, OFFSET SigB60
	mov	cx, SIG_LEN
	repe	cmpsb
	jne	SHORT sri_exit
	mov	di, 0B78h			; INT 13h wrapper
	mov	si, OFFSET SigB78
	mov	cx, SIG_LEN
	repe	cmpsb
	jne	SHORT sri_exit
	mov	dx, es

sri_exit:
	xor	bx, bx
	xor	si, si
	mov	ax, Device_Load_Ok
	ret

szInbName	db	"INBRDPC%"
SIG_LEN 	EQU	24
SigB60	db	83h,3Eh,0B7h,02h,0FFh,74h,10h,0BAh,70h,06h,0A0h,0ACh
	db	02h,0F6h,0D0h,24h,1Eh,0EEh,0C6h,06h,0BFh,02h,00h,0C3h
SigB78	db	0FAh,9Ch,50h,2Eh,0A1h,0B1h,02h,80h,0FAh,80h,73h,04h
	db	2Eh,0A1h,0AFh,02h,2Eh,3Ah,06h,0ACh,02h,76h,0Fh,0F6h

EndProc ShadRam_Real_Init

VxD_REAL_INIT_ENDS

	END ShadRam_Real_Init
