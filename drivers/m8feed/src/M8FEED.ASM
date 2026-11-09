;******************************************************************************
;   M8FEED.VXD - keeps the Mach8's engine fed from a queue while the program runs
;
;   Copyright (c) 2026 the win95-intel-inboard-386pc project. GPL-3.0-or-later.
;
;   The Mach8 has a 16-entry command FIFO and no DMA or engine interrupt, so the
;   card draws only while the CPU writes to it. M8DD32.DLL works a Direct3D frame
;   out whole into the queue this driver allocates, sends all but its last fills,
;   and returns from Flip; a 1 ms global time-out here tops the FIFO up until the
;   queue is empty. The system timer runs at 1 ms only while there is something
;   to feed: each tick's port I/O empties the CPU's cache, the program's too. The
;   next DirectDraw call into M8DD32 stops the feed and sends what is left itself,
;   so the card is never written from two places.
;
;   Queue: one locked block in the system arena, which ring 3 can write on Windows
;   95. A header, then entries of port | value << 16. Port 0 is a marker: it reads
;   SUBSYS_STAT's blank latch and the time, for the page wait after a page show.
;
;   Loaded dynamically: CreateFile("\\.\M8FEED.VXD") from C:\WINDOWS\SYSTEM.
;******************************************************************************

	.386p

	.XLIST
	INCLUDE VMM.Inc
	INCLUDE VWIN32.Inc
	INCLUDE VTD.Inc
	.LIST

Declare_Virtual_Device M8FEED, 1, 0, M8Feed_Control, Undefined_Device_ID, \
		       Undefined_Init_Order

Q_PAGES 	EQU	8		; 32 KB
Q_HEAD		EQU	0		; next entry to send (this driver)
Q_TAIL		EQU	4		; entries queued (ring 3)
Q_STATE 	EQU	8		; 0 idle, 1 feeding, 2 stopped: the driver was switched off,
					; 3 stopped: the 16-bit driver used the engine
Q_BLANK 	EQU	12		; SUBSYS_STAT bit 0 at the last marker
Q_SHOWMS	EQU	16		; system time at the last marker, ms
Q_TICKS 	EQU	20		; time-outs run
Q_FULL		EQU	24		; status reads that found the FIFO full
Q_SENT		EQU	28		; entries this driver sent
Q_FIRST 	EQU	64		; the first entry

H_HW_OFF	EQU	11		; byte in the display driver's hot data: set while a DOS box owns the screen
H_BUSY		EQU	12		; 2 while the engine is the feed's; any 16-bit screen operation changes it
EXT_FIFO_STATUS EQU	9AEEh		; bit 15 - n set: fewer than n entries free
SUBSYS_STAT	EQU	42E8h
BUDGET		EQU	512		; writes a time-out may make, ~4 ms of bus

IOCTL_INIT	EQU	1		; in: hot data's linear address; out: the queue's
IOCTL_START	EQU	2		; feed entries Q_HEAD .. Q_TAIL - 1
IOCTL_STOP	EQU	3		; out: Q_HEAD; ring 3 sends the rest

VxD_LOCKED_DATA_SEG
	ALIGN 4
dwQueue 	dd	0		; linear address
dwQueueHandle	dd	0
dwHot		dd	0
dwTimeOut	dd	0		; pending time-out handle, 0 none
dwMinPeriod	dd	0		; VTD_Begin_Min_Int_Period in force
dwEvent 	dd	0		; pending global event that ends it, 0 none
VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

BeginProc M8Feed_Control
	Control_Dispatch Sys_Dynamic_Device_Init, M8Feed_Dyn_Init
	Control_Dispatch Sys_Dynamic_Device_Exit, M8Feed_Dyn_Exit
	Control_Dispatch W32_DEVICEIOCONTROL, M8Feed_IoCtl
	clc
	ret
EndProc M8Feed_Control

BeginProc M8Feed_Dyn_Init
	clc
	ret
EndProc M8Feed_Dyn_Init

BeginProc M8Feed_Dyn_Exit
	call	M8Feed_Stop
	call	M8Feed_Period_Off
	cmp	[dwQueueHandle], 0
	je	SHORT de_free
	VMMCall _PageFree, <[dwQueueHandle], 0>
	mov	[dwQueueHandle], 0
	mov	[dwQueue], 0
de_free:
	clc
	ret
EndProc M8Feed_Dyn_Exit

;   The 1 ms period off, from a normal context. Preserves ESI.
BeginProc M8Feed_Period_Off
	push	esi
	pushfd
	cli
	mov	esi, [dwEvent]
	mov	[dwEvent], 0
	or	esi, esi
	jz	SHORT po_event
	VMMCall Cancel_Global_Event
po_event:
	popfd
	cmp	[dwMinPeriod], 0
	je	SHORT po_done
	mov	[dwMinPeriod], 0
	mov	eax, 1
	VxDCall VTD_End_Min_Int_Period
po_done:
	pop	esi
	ret
EndProc M8Feed_Period_Off

;   Global event scheduled when the queue drained: the period off, unless a new
;   frame started feeding since.
BeginProc M8Feed_Event
	pushad
	mov	[dwEvent], 0
	mov	edx, [dwQueue]
	or	edx, edx
	jz	SHORT ev_off
	cmp	dword ptr [edx + Q_STATE], 1
	je	SHORT ev_out
ev_off:
	cmp	[dwMinPeriod], 0
	je	SHORT ev_out
	mov	[dwMinPeriod], 0
	mov	eax, 1
	VxDCall VTD_End_Min_Int_Period
ev_out:
	popad
	ret
EndProc M8Feed_Event

;   ESI = DIOCParams. EAX = 0 on success.
BeginProc M8Feed_IoCtl
	mov	ecx, [esi.dwIoControlCode]
	cmp	ecx, DIOC_OPEN
	je	io_ok
	cmp	ecx, DIOC_CLOSEHANDLE
	jne	SHORT io_init
	call	M8Feed_Stop
	call	M8Feed_Period_Off
	jmp	io_ok
io_init:
	cmp	ecx, IOCTL_INIT
	jne	io_start
	cmp	[esi.cbInBuffer], 4
	jb	io_bad
	cmp	[esi.cbOutBuffer], 4
	jb	io_bad
	call	M8Feed_Stop
	mov	eax, [esi.lpvInBuffer]
	mov	eax, [eax]
	mov	[dwHot], eax
	push	esi
	shr	eax, 12
	VMMCall _LinPageLock, <eax, 1, 0>
	pop	esi
	cmp	[dwQueue], 0
	jne	SHORT io_have
	push	esi
	VMMCall _PageAllocate, <Q_PAGES, PG_SYS, 0, 0, 0, 0, 0, PAGEFIXED + PAGEZEROINIT>
	pop	esi
	or	eax, eax
	jz	io_bad
	mov	[dwQueueHandle], eax
	mov	[dwQueue], edx
io_have:
	mov	eax, [esi.lpvOutBuffer]
	mov	edx, [dwQueue]
	mov	[eax], edx
	mov	eax, [esi.lpcbBytesReturned]
	or	eax, eax
	jz	io_ok
	mov	dword ptr [eax], 4
	jmp	io_ok
io_start:
	cmp	ecx, IOCTL_START
	jne	SHORT io_stop
	mov	edx, [dwQueue]
	or	edx, edx
	jz	io_bad
	call	M8Feed_Stop
	cmp	[dwMinPeriod], 0
	jne	SHORT io_on
	push	edx
	push	esi
	mov	eax, 1
	VxDCall VTD_Begin_Min_Int_Period
	pop	esi
	pop	edx
	jc	SHORT io_on
	mov	[dwMinPeriod], 1
io_on:
	mov	edx, [dwQueue]
	mov	dword ptr [edx + Q_STATE], 1
	call	M8Feed_Run
	jmp	io_ok
io_stop:
	cmp	ecx, IOCTL_STOP
	jne	io_bad
	call	M8Feed_Stop
	call	M8Feed_Period_Off
	mov	eax, [esi.lpvOutBuffer]
	or	eax, eax
	jz	io_ok
	mov	edx, [dwQueue]
	or	edx, edx
	jz	io_bad
	mov	edx, [edx + Q_HEAD]
	mov	[eax], edx
	jmp	io_ok
io_bad:
	mov	eax, 1
	ret
io_ok:
	xor	eax, eax
	ret
EndProc M8Feed_IoCtl

;   No time-out pending and the state idle when this returns. Preserves ESI.
BeginProc M8Feed_Stop
	push	esi
	pushfd
	cli
	mov	edx, [dwQueue]
	or	edx, edx
	jz	SHORT st_state
	cmp	dword ptr [edx + Q_STATE], 1
	jne	SHORT st_state
	mov	dword ptr [edx + Q_STATE], 0
st_state:
	mov	esi, [dwTimeOut]
	mov	[dwTimeOut], 0
	or	esi, esi
	jz	SHORT st_done
	VMMCall Cancel_Time_Out
st_done:
	popfd
	pop	esi
	ret
EndProc M8Feed_Stop

;   The time-out: EDX = the queue. Must not page-fault; touches locked data only.
BeginProc M8Feed_Tick
	pushad
	mov	[dwTimeOut], 0
	inc	dword ptr [edx + Q_TICKS]
	call	M8Feed_Run
	popad
	ret
EndProc M8Feed_Tick

;   Tops the FIFO up while there is room, up to BUDGET writes, then re-arms the
;   time-out if entries are left. Interrupts may be on. EDX = the queue.
BeginProc M8Feed_Run
	pushad
	mov	esi, edx
	cmp	dword ptr [esi + Q_STATE], 1
	jne	rn_out
	mov	eax, [dwHot]
	cmp	byte ptr [eax + H_HW_OFF], 0
	je	SHORT rn_on
	mov	dword ptr [esi + Q_STATE], 2
	jmp	rn_out
rn_on:
	; The 16-bit driver may be part-way through its own register sequence.
	cmp	byte ptr [eax + H_BUSY], 2
	je	SHORT rn_ours
	mov	dword ptr [esi + Q_STATE], 3
	jmp	rn_out
rn_ours:
	mov	ebp, BUDGET
	mov	edi, [esi + Q_HEAD]
rn_status:
	cmp	edi, [esi + Q_TAIL]
	jae	SHORT rn_empty
	mov	dx, EXT_FIFO_STATUS
	in	ax, dx
	; Free entries: count clear bits from bit 14 down, as m8d3d_room does.
	xor	ecx, ecx
	mov	bx, 4000h
rn_free:
	test	ax, bx
	jnz	SHORT rn_counted
	inc	ecx
	shr	bx, 1
	cmp	ecx, 15
	jb	SHORT rn_free
rn_counted:
	or	ecx, ecx
	jnz	SHORT rn_send
	inc	dword ptr [esi + Q_FULL]
	jmp	SHORT rn_again
rn_send:
	cmp	edi, [esi + Q_TAIL]
	jae	SHORT rn_empty
	mov	edx, [esi + Q_FIRST + edi * 4]
	inc	edi
	or	dx, dx
	jz	SHORT rn_marker
	mov	eax, edx
	shr	eax, 16
	out	dx, ax
	inc	dword ptr [esi + Q_SENT]
	dec	ebp
	jz	SHORT rn_spent
	loop	rn_send
	jmp	SHORT rn_status
rn_marker:
	mov	dx, SUBSYS_STAT
	in	ax, dx
	and	eax, 1
	mov	[esi + Q_BLANK], eax
	VMMCall Get_System_Time
	mov	[esi + Q_SHOWMS], eax
	jmp	SHORT rn_send
rn_spent:
	mov	[esi + Q_HEAD], edi
	jmp	SHORT rn_again
rn_empty:
	mov	[esi + Q_HEAD], edi
	mov	dword ptr [esi + Q_STATE], 0
	pushfd
	cli
	cmp	[dwMinPeriod], 0
	je	SHORT rn_noevent
	cmp	[dwEvent], 0
	jne	SHORT rn_noevent
	mov	esi, OFFSET32 M8Feed_Event
	xor	edx, edx
	VMMCall Schedule_Global_Event
	mov	[dwEvent], esi
rn_noevent:
	popfd
	jmp	SHORT rn_out
rn_again:
	mov	[esi + Q_HEAD], edi
	pushfd
	cli
	cmp	dword ptr [esi + Q_STATE], 1
	jne	SHORT rn_armed
	cmp	[dwTimeOut], 0
	jne	SHORT rn_armed
	mov	eax, 1
	mov	edx, esi
	mov	esi, OFFSET32 M8Feed_Tick
	VMMCall Set_Global_Time_Out
	mov	[dwTimeOut], esi
rn_armed:
	popfd
rn_out:
	popad
	ret
EndProc M8Feed_Run

VxD_LOCKED_CODE_ENDS

	END
