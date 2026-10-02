;******************************************************************************
;   SWAPCOMP.VXD - LZ4-compress pages on their way to WIN386.SWP
;
;   Stage 3 of swap compression (docs/magnaram_and_swap_compression_2026_10_01.md).
;   On this machine paging costs bus bytes, ~10.4 ms per 4 KB page, while LZ4
;   costs 1.17 ms to decompress and 4.71 ms to compress a page on the 5160's
;   486BL3 (LZ4BENCH). Fewer bytes over the bus is the whole idea.
;
;   Layout: every page keeps its own 4 KB slot in the swap file; only the
;   compressed sectors of it are written and read. SlotTab records how many
;   sectors each slot holds (0 = a full, uncompressed page). The swap file is
;   discarded at every boot, so the table never needs to survive one.
;
;   Mechanism: PAGEFILE still does all the file I/O. For a compressed page we
;   hand it our bounce buffer, and a hook on IFSMgr_Ring0_FileIO shortens the
;   transfer when - and only when - it sees that buffer. If that hook does not
;   see a compressed write, the page is rewritten uncompressed and compression
;   is switched off for the session (SC_Disabled).
;
;   Load: SYSTEM.INI [386Enh] device=C:\SWAPCOMP.VXD
;   Build with -DSC_CHECKSUM for the bed: every compressed read is compared
;   against a checksum taken at write time.
;******************************************************************************

	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE Debug.Inc
	INCLUDE PageFile.Inc
	INCLUDE PageSwap.Inc
	INCLUDE IFSMgr.Inc
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

PAGE_BYTES	EQU	4096
HASH_LOG	EQU	12
HASH_SIZE	EQU	(1 SHL HASH_LOG)

; Slots covered by SlotTab: 8192 = a 32 MB swap file. Pages beyond it are
; stored uncompressed, so a bigger file still works, just less of it gains.
ifndef SC_SLOTS
SC_SLOTS	EQU	8192
endif

; Store a page compressed only if it saves at least two sectors.
SC_MAX_SECTORS	EQU	6

Declare_Virtual_Device SWAPCOMP, 1, 0, SwapComp_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

;******************************************************************************
; Everything below is locked: it runs inside the pager, where a page fault
; cannot be serviced.
;******************************************************************************

VxD_LOCKED_DATA_SEG

	ALIGN 4
SC_Prev 	dd	0		; original PageFile_Read_Or_Write
SC_IFS_Prev	dd	0		; original IFSMgr_Ring0_FileIO
SC_Sem		dd	0
SC_Busy 	dd	0
SC_Inflight	dd	0		; our transfer is in PAGEFILE right now
SC_Want 	dd	0		; bytes it should move
SC_Seen 	dd	0		; the IFS hook saw and shortened it
SC_Disabled	dd	0
SC_Orig 	dd	0		; the caller's descriptor
SC_Status	dd	0

SC_Desc 	PageSwapBufferDesc <>	; one page at a time goes to PAGEFILE

; Counters (System Monitor / PERFLOG: SWAPCOMP\*)
cCompWrites	dd	0
cRawWrites	dd	0
cCompReads	dd	0
cRawReads	dd	0
cSectorsSaved	dd	0
cErrors 	dd	0
cFallback	dd	0
cContention	dd	0
ifdef SC_CHECKSUM
cSumErrors	dd	0
endif

; LZ4 working state. Only touched while SC_Sem is held.
lz_src		dd	0
lz_dst0 	dd	0
lz_iend 	dd	0
lz_oend 	dd	0
lz_mflimit	dd	0
lz_matchlimit	dd	0
lz_anchor	dd	0
lz_op		dd	0
lz_token	dd	0
lz_tok		dd	0
lz_ml		dd	0
lz_off		dd	0
lz_result	dd	0

	ALIGN 4
HTab		dw	HASH_SIZE dup (0)	; position + 1 of the last 4-byte group
SlotTab 	db	SC_SLOTS dup (0)	; sectors per slot, 0 = uncompressed
ifdef SC_CHECKSUM
SumTab		dw	SC_SLOTS dup (0)
endif
	ALIGN 4
; [0..1] compressed length, then the LZ4 block. Worst case for an
; incompressible page is 4096 + 4096/255 + 16; such a page is stored raw.
SC_Bounce	db	4352 dup (0)

; PERF may keep these pointers after init, so they cannot live in discardable data.
szSrvName	db	"Swap compression",0
szSrvKey	db	"SWAPCOMP",0
szCompWrites	db	"CompressedWrites",0
szRawWrites	db	"RawWrites",0
szCompReads	db	"CompressedReads",0
szRawReads	db	"RawReads",0
szSectorsSaved	db	"SectorsSaved",0
szErrors	db	"Errors",0
szFallback	db	"Fallback",0
szContention	db	"Contention",0
ifdef SC_CHECKSUM
szSumErrors	db	"ChecksumErrors",0
endif

VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc SwapComp_Control
	Control_Dispatch Device_Init, SwapComp_Device_Init
	clc
	ret
EndProc SwapComp_Control

;******************************************************************************
;   IFSMgr_Ring0_FileIO hook. Shortens a transfer only when it is ours: our
;   flag is up AND the buffer is our bounce buffer.
;******************************************************************************

BeginProc SC_IFS_Hook, HOOK_PROC, SC_IFS_Prev
	cmp	[SC_Inflight], 0
	je	SHORT sih_go
	cmp	esi, OFFSET32 SC_Bounce
	jne	SHORT sih_go
	mov	ecx, [SC_Want]
	mov	[SC_Seen], 1
sih_go:
	jmp	[SC_IFS_Prev]
EndProc SC_IFS_Hook

;******************************************************************************
;   PageFile_Read_Or_Write hook.
;   ENTRY: EBX = PageSwapBufferDesc, ints enabled. EXIT: status set.
;   USES: flags only, like the service it replaces.
;   Each page of the request goes to PAGEFILE separately through SC_Desc.
;******************************************************************************

BeginProc SwapComp_Hook, HOOK_PROC, SC_Prev

	pushad
	inc	[SC_Busy]
	cmp	[SC_Busy], 1
	je	SHORT sch_lock
	inc	[cContention]
sch_lock:
	mov	eax, [SC_Sem]
	mov	ecx, BLOCK_SVC_INTS OR BLOCK_ENABLE_INTS
	VMMCall Wait_Semaphore
	mov	ebx, [esp].Pushad_EBX	; the caller's descriptor, from our own frame
	mov	[SC_Orig], ebx

	mov	[SC_Status], PFS_Success
	movzx	edi, [ebx].PS_BD_nPages
	mov	al, [ebx].PS_BD_Cmd
	mov	[SC_Desc].PS_BD_Cmd, al
	mov	al, [ebx].PS_BD_Priority
	mov	[SC_Desc].PS_BD_Priority, al
	mov	[SC_Desc].PS_BD_nPages, 1
	mov	edx, [ebx].PS_BD_File_Page
	mov	esi, [ebx].PS_BD_Buffer_Ptr
	or	edi, edi
	jz	SHORT sch_done
sch_page:
	mov	[SC_Desc].PS_BD_File_Page, edx
	mov	[SC_Desc].PS_BD_Buffer_Ptr, esi
	pushad
	call	SC_One
	popad
	add	edx, 1
	add	esi, PAGE_BYTES
	dec	edi
	jnz	sch_page
sch_done:
	mov	ebx, [SC_Orig]
	mov	al, BYTE PTR [SC_Status]
	mov	[ebx].PS_BD_Status, al

	dec	[SC_Busy]
	mov	eax, [SC_Sem]
	VMMCall Signal_Semaphore
	popad
	ret

EndProc SwapComp_Hook

;******************************************************************************
;   SC_One - one page, described by SC_Desc. A failure sets SC_Status to
;   PFS_Failure.
;******************************************************************************

BeginProc SC_One

	mov	edx, [SC_Desc].PS_BD_File_Page
	cmp	[SC_Desc].PS_BD_Cmd, PF_Write_Data
	je	sco_write

;---- read --------------------------------------------------------------------
	cmp	edx, SC_SLOTS
	jae	SHORT sco_raw_read
	movzx	ecx, SlotTab[edx]
	or	ecx, ecx
	jnz	SHORT sco_comp_read
sco_raw_read:
	inc	[cRawReads]
	jmp	sco_pass

sco_comp_read:
	shl	ecx, 9
	call	SC_Transfer		; reads ECX bytes into SC_Bounce
	jc	sco_fail
	movzx	ecx, WORD PTR SC_Bounce
	mov	eax, ecx
	add	eax, 2
	mov	edx, [SC_Desc].PS_BD_File_Page
	movzx	edx, SlotTab[edx]
	shl	edx, 9
	cmp	eax, edx
	ja	sco_error		; length field larger than what was read
	mov	esi, OFFSET32 SC_Bounce + 2
	mov	edi, [SC_Desc].PS_BD_Buffer_Ptr
	call	LZ4_Decompress
	cmp	eax, PAGE_BYTES
	jne	sco_error
	inc	[cCompReads]
ifdef SC_CHECKSUM
	mov	esi, [SC_Desc].PS_BD_Buffer_Ptr
	call	SC_PageSum
	mov	edx, [SC_Desc].PS_BD_File_Page
	cmp	ax, SumTab[edx*2]
	je	SHORT sco_sum_ok
	inc	[cSumErrors]
sco_sum_ok:
endif
	ret

;---- write -------------------------------------------------------------------
sco_write:
	cmp	edx, SC_SLOTS
	jae	sco_raw_write_nt
	mov	SlotTab[edx], 0		; until a compressed write succeeds
	cmp	[SC_Disabled], 0
	jne	sco_raw_write

	mov	esi, [SC_Desc].PS_BD_Buffer_Ptr
	mov	edi, OFFSET32 SC_Bounce + 2
	call	LZ4_Compress
	mov	WORD PTR SC_Bounce, ax
	lea	ecx, [eax + 2 + 511]
	shr	ecx, 9			; sectors
	cmp	ecx, SC_MAX_SECTORS
	ja	sco_raw_write
	push	ecx
	shl	ecx, 9
	call	SC_Transfer		; writes ECX bytes from SC_Bounce
	pop	ecx
	jc	sco_fail
	cmp	[SC_Seen], 0
	je	SHORT sco_not_seen
	mov	edx, [SC_Desc].PS_BD_File_Page
	mov	SlotTab[edx], cl
	inc	[cCompWrites]
	mov	eax, 8
	sub	eax, ecx
	add	[cSectorsSaved], eax
ifdef SC_CHECKSUM
	mov	esi, [SC_Desc].PS_BD_Buffer_Ptr
	call	SC_PageSum
	mov	edx, [SC_Desc].PS_BD_File_Page
	mov	SumTab[edx*2], ax
endif
	ret

; PAGEFILE moved our buffer without passing it through the hook we rely on,
; so whatever it wrote is not a page. Rewrite it whole and stop compressing.
sco_not_seen:
	mov	[SC_Disabled], 1
	inc	[cFallback]
sco_raw_write:
	inc	[cRawWrites]
	jmp	SHORT sco_pass
sco_raw_write_nt:
	inc	[cRawWrites]

sco_pass:
	mov	ebx, OFFSET32 SC_Desc
	call	[SC_Prev]
	cmp	[SC_Desc].PS_BD_Status, PFS_Success
	jne	SHORT sco_fail
	ret

sco_error:
	inc	[cErrors]
sco_fail:
	mov	[SC_Status], PFS_Failure
	ret

EndProc SC_One

;******************************************************************************
;   SC_Transfer - move ECX bytes between SC_Bounce and this page's slot,
;   through PAGEFILE. Carry set on failure. SC_Seen says whether the IFS hook
;   did the shortening.
;******************************************************************************

BeginProc SC_Transfer
	mov	[SC_Want], ecx
	mov	[SC_Seen], 0
	mov	eax, [SC_Desc].PS_BD_Buffer_Ptr
	push	eax
	mov	[SC_Desc].PS_BD_Buffer_Ptr, OFFSET32 SC_Bounce
	mov	[SC_Inflight], 1
	mov	ebx, OFFSET32 SC_Desc
	call	[SC_Prev]
	mov	[SC_Inflight], 0
	pop	eax
	mov	[SC_Desc].PS_BD_Buffer_Ptr, eax
	cmp	[SC_Desc].PS_BD_Status, PFS_Success
	jne	SHORT sct_fail
	cmp	[SC_Desc].PS_BD_Cmd, PF_Write_Data
	je	SHORT sct_ok
	cmp	[SC_Seen], 0		; a read that was not shortened read 4 KB
	je	SHORT sct_fail		;   into a 4.25 KB buffer: usable, but wrong
sct_ok:
	clc
	ret
sct_fail:
	stc
	ret
EndProc SC_Transfer

ifdef SC_CHECKSUM
;   ESI = page -> AX = 16-bit checksum
BeginProc SC_PageSum
	push	ecx
	push	esi
	xor	eax, eax
	mov	ecx, PAGE_BYTES / 4
scs_loop:
	add	eax, [esi]
	rol	eax, 1
	add	esi, 4
	dec	ecx
	jnz	scs_loop
	mov	ecx, eax
	shr	ecx, 16
	xor	eax, ecx
	pop	esi
	pop	ecx
	ret
EndProc SC_PageSum
endif

;******************************************************************************
;   LZ4 block format, one 4 KB page. Same algorithm as LZ4BENCH.C, which
;   round-tripped real swap pages on the 5160.
;******************************************************************************

;   EAX = 4 bytes -> EAX = hash index. Other registers preserved.
BeginProc LZ_Hash
	imul	eax, eax, 9E3779B1h
	shr	eax, 32 - HASH_LOG
	ret
EndProc LZ_Hash

;   Copy ECX bytes ESI -> EDI forward, advancing both. Correct for an
;   overlapping match only when the source is at least 4 bytes behind.
BeginProc LZ_Copy
	push	ecx
	shr	ecx, 2
	rep	movsd
	pop	ecx
	and	ecx, 3
	rep	movsb
	ret
EndProc LZ_Copy

;   ECX = length remainder -> 255-run encoding at EDI, EDI advanced
BeginProc LZ_Put_Len
lpl_loop:
	cmp	ecx, 255
	jb	SHORT lpl_last
	mov	BYTE PTR [edi], 255
	inc	edi
	sub	ecx, 255
	jmp	lpl_loop
lpl_last:
	mov	[edi], cl
	inc	edi
	ret
EndProc LZ_Put_Len

;   EDX = literal count, EDI = output, source at [lz_anchor].
;   Writes the token (literal nibble only) and the literals; [lz_token] = token.
BeginProc LZ_Emit_Literals
	mov	[lz_token], edi
	inc	edi
	cmp	edx, 15
	jb	SHORT lel_small
	mov	BYTE PTR [edi-1], 0F0h
	mov	ecx, edx
	sub	ecx, 15
	call	LZ_Put_Len
	jmp	SHORT lel_copy
lel_small:
	mov	eax, edx
	shl	eax, 4
	mov	[edi-1], al
lel_copy:
	push	esi
	mov	esi, [lz_anchor]
	mov	ecx, edx
	call	LZ_Copy
	pop	esi
	ret
EndProc LZ_Emit_Literals

;   ESI = page, EDI = output -> EAX = compressed length
BeginProc LZ4_Compress
	pushad
	cld
	mov	[lz_src], esi
	mov	[lz_dst0], edi
	mov	[lz_op], edi
	mov	[lz_anchor], esi
	lea	eax, [esi + PAGE_BYTES]
	mov	[lz_iend], eax
	lea	ecx, [eax - 12]
	mov	[lz_mflimit], ecx
	lea	ecx, [eax - 5]
	mov	[lz_matchlimit], ecx

	mov	edi, OFFSET32 HTab
	mov	ecx, HASH_SIZE * 2 / 4
	xor	eax, eax
	rep	stosd

	mov	eax, [esi]
	call	LZ_Hash
	mov	HTab[eax*2], 1
	lea	ebx, [esi + 1]			; EBX = ip

lzc_loop:
	cmp	ebx, [lz_mflimit]
	jae	lzc_last
	mov	eax, [ebx]
	call	LZ_Hash
	movzx	edx, HTab[eax*2]		; candidate, position + 1
	mov	ecx, ebx
	sub	ecx, [lz_src]
	inc	ecx
	mov	HTab[eax*2], cx
	or	edx, edx
	jz	lzc_next
	mov	esi, [lz_src]
	lea	esi, [esi + edx - 1]		; ESI = candidate match
	mov	eax, [esi]
	cmp	eax, [ebx]
	jne	lzc_next

	mov	ecx, 4				; ECX = match length
lzc_fwd:
	lea	eax, [ebx + ecx]
	cmp	eax, [lz_matchlimit]
	jae	SHORT lzc_back
	mov	al, [esi + ecx]
	cmp	al, [ebx + ecx]
	jne	SHORT lzc_back
	inc	ecx
	jmp	lzc_fwd
lzc_back:
	cmp	ebx, [lz_anchor]
	jbe	SHORT lzc_emit
	cmp	esi, [lz_src]
	jbe	SHORT lzc_emit
	mov	al, [ebx - 1]
	cmp	al, [esi - 1]
	jne	SHORT lzc_emit
	dec	ebx
	dec	esi
	inc	ecx
	jmp	lzc_back

lzc_emit:
	mov	[lz_ml], ecx
	mov	eax, ebx
	sub	eax, esi
	mov	[lz_off], eax
	mov	edx, ebx
	sub	edx, [lz_anchor]		; literals before the match
	mov	edi, [lz_op]
	call	LZ_Emit_Literals
	mov	eax, [lz_off]
	mov	[edi], ax
	add	edi, 2
	mov	ecx, [lz_ml]
	sub	ecx, 4
	mov	eax, [lz_token]
	cmp	ecx, 15
	jb	SHORT lzc_ml_small
	or	BYTE PTR [eax], 15
	sub	ecx, 15
	call	LZ_Put_Len
	jmp	SHORT lzc_ml_done
lzc_ml_small:
	or	[eax], cl
lzc_ml_done:
	mov	[lz_op], edi
	add	ebx, [lz_ml]
	mov	[lz_anchor], ebx
	jmp	lzc_loop

lzc_next:
	inc	ebx
	jmp	lzc_loop

lzc_last:
	mov	edx, [lz_iend]
	sub	edx, [lz_anchor]
	mov	edi, [lz_op]
	call	LZ_Emit_Literals
	sub	edi, [lz_dst0]
	mov	[lz_result], edi
	popad
	mov	eax, [lz_result]
	ret
EndProc LZ4_Compress

;   EAX = running length; adds 255-run extension bytes from ESI.
;   Carry set if the input runs out.
BeginProc LZD_Ext
lze_loop:
	cmp	esi, [lz_iend]
	jae	SHORT lze_bad
	movzx	edx, BYTE PTR [esi]
	inc	esi
	add	eax, edx
	cmp	edx, 255
	je	lze_loop
	clc
	ret
lze_bad:
	stc
	ret
EndProc LZD_Ext

;   ESI = block, ECX = its length, EDI = 4 KB output
;   -> EAX = bytes produced, 0 if the block is malformed. Never writes outside
;   the output page, never reads outside the block.
BeginProc LZ4_Decompress
	pushad
	cld
	mov	[lz_dst0], edi
	lea	eax, [esi + ecx]
	mov	[lz_iend], eax
	lea	eax, [edi + PAGE_BYTES]
	mov	[lz_oend], eax

lzd_loop:
	cmp	esi, [lz_iend]
	jae	lzd_bad
	movzx	eax, BYTE PTR [esi]
	inc	esi
	mov	[lz_tok], eax
	shr	eax, 4
	cmp	eax, 15
	jne	SHORT lzd_lit
	call	LZD_Ext
	jc	lzd_bad
lzd_lit:
	mov	edx, [lz_oend]
	sub	edx, edi
	cmp	eax, edx
	ja	lzd_bad
	mov	edx, [lz_iend]
	sub	edx, esi
	cmp	eax, edx
	ja	lzd_bad
	mov	ecx, eax
	call	LZ_Copy
	cmp	esi, [lz_iend]
	jae	SHORT lzd_end

	mov	edx, [lz_iend]
	sub	edx, esi
	cmp	edx, 2
	jb	SHORT lzd_bad
	movzx	edx, WORD PTR [esi]		; match offset
	add	esi, 2
	or	edx, edx
	jz	SHORT lzd_bad
	mov	eax, edi
	sub	eax, [lz_dst0]
	cmp	edx, eax
	ja	SHORT lzd_bad
	mov	eax, [lz_tok]
	and	eax, 15
	cmp	eax, 15
	jne	SHORT lzd_ml
	push	edx
	call	LZD_Ext
	pop	edx
	jc	SHORT lzd_bad
lzd_ml:
	add	eax, 4
	mov	ecx, [lz_oend]
	sub	ecx, edi
	cmp	eax, ecx
	ja	SHORT lzd_bad
	push	esi
	mov	esi, edi
	sub	esi, edx
	mov	ecx, eax
	cmp	edx, 4
	jb	SHORT lzd_bytes
	call	LZ_Copy
	jmp	SHORT lzd_mdone
lzd_bytes:
	rep	movsb				; byte by byte replicates a short period
lzd_mdone:
	pop	esi
	jmp	lzd_loop

lzd_end:
	sub	edi, [lz_dst0]
	mov	[lz_result], edi
	jmp	SHORT lzd_ret
lzd_bad:
	mov	[lz_result], 0
lzd_ret:
	popad
	mov	eax, [lz_result]
	ret
EndProc LZ4_Decompress

VxD_LOCKED_CODE_ENDS

VxD_ICODE_SEG

BeginProc SwapComp_Device_Init

	mov	ecx, 1
	VMMCall Create_Semaphore
	jc	sdi_fail
	mov	[SC_Sem], eax

	VxDCall PERF_Get_Version
	or	eax, eax
	jz	sdi_hook

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
	jz	sdi_hook
	mov	edx, eax

	mov	esi, OFFSET32 szCompWrites
	mov	edi, OFFSET32 cCompWrites
	call	SC_Add_Stat
	mov	esi, OFFSET32 szRawWrites
	mov	edi, OFFSET32 cRawWrites
	call	SC_Add_Stat
	mov	esi, OFFSET32 szCompReads
	mov	edi, OFFSET32 cCompReads
	call	SC_Add_Stat
	mov	esi, OFFSET32 szRawReads
	mov	edi, OFFSET32 cRawReads
	call	SC_Add_Stat
	mov	esi, OFFSET32 szSectorsSaved
	mov	edi, OFFSET32 cSectorsSaved
	call	SC_Add_Stat
	mov	esi, OFFSET32 szErrors
	mov	edi, OFFSET32 cErrors
	call	SC_Add_Stat
	mov	esi, OFFSET32 szFallback
	mov	edi, OFFSET32 cFallback
	call	SC_Add_Stat
	mov	esi, OFFSET32 szContention
	mov	edi, OFFSET32 cContention
	call	SC_Add_Stat
ifdef SC_CHECKSUM
	mov	esi, OFFSET32 szSumErrors
	mov	edi, OFFSET32 cSumErrors
	call	SC_Add_Stat
endif

; IFS hook first: the pager hook must never run without it.
sdi_hook:
	GetVxDServiceOrdinal eax, IFSMgr_Ring0_FileIO
	mov	esi, OFFSET32 SC_IFS_Hook
	VMMCall Hook_Device_Service
	jc	SHORT sdi_fail
	GetVxDServiceOrdinal eax, PageFile_Read_Or_Write
	mov	esi, OFFSET32 SwapComp_Hook
	VMMCall Hook_Device_Service
sdi_fail:
	clc				; without the hooks Windows pages as usual
	ret

EndProc SwapComp_Device_Init

;   EDX = server handle, ESI = name, EDI = DWORD counter (see SWAPCNT.ASM)
BeginProc SC_Add_Stat
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
EndProc SC_Add_Stat

VxD_ICODE_ENDS

	END
