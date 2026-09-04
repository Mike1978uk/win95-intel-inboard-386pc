;****************************************************************************
; CFU1.ASM - RATOC REX-CFU1 (SL811HS USB 1.1 host) PC Card enabler VxD
;            for Windows 95/98.
;
; The REX-CFU1's CIS CONFIG tuple points at attribute 0xF8, but the card's
; real (and only) configuration register is at attribute 0xFC - so the OS
; PCMCIA stack can never enable it.  This VxD enables the card by hand:
; socket power via the 82365-compatible PCIC, a temporary attribute-memory
; window for the COR-at-0xFC poke, then an I/O window for the SL811HS.
;
; Dynamically loadable: CFUDIAG.EXE loads it with
;   CreateFile("\\\\.\\<path>\\CFU1.VXD", ..., FILE_FLAG_DELETE_ON_CLOSE)
; and drives it through DeviceIoControl.  See CFU1.H for the ioctl ABI.
;
; Hardware findings this encodes are documented in PROBE-NOTES.md (repo
; root); verified on real hardware (IBM PC110) 2026-07-11.
;
; Build (Open Watcom wlink + JWasm):
;   jwasm -q -Iddkinc win/vxd/CFU1.ASM
;   wlink system win_vxd name CFU1.VXD file CFU1.obj export CFU1_DDB.1
;****************************************************************************
        .386p
        include vmm.inc
        include vwin32.inc
        include vpicd.inc
        include configmg.inc
        include vmd.inc
        include vkd.inc
        include shell.inc
        include drp.inc
        include ilb.inc
        include aep.inc
        include ios.inc
        include blockdev.inc
        include ior.inc
        include ddb.inc
BDD_BD_Major_Ver equ 4                  ; block-device version (struct defaults)
BDD_BD_Minor_Ver equ 0
        include dcb.inc
        include iop.inc

;----------------------------------------------------------------------------
; ioctl codes (keep in sync with win/diag/CFU1.H)
;----------------------------------------------------------------------------
CFU_GETVER      equ 1000h       ; out: DWORD version
CFU_ENABLE      equ 1001h       ; in: CFU_SETUP, out: CFU_RESULT
CFU_DISABLE     equ 1002h       ; power socket down
CFU_RDREG       equ 1003h       ; in: BYTE reg          out: BYTE val
CFU_WRREG       equ 1004h       ; in: BYTE reg,BYTE val
CFU_RDATTR      equ 1005h       ; in: WORD offset       out: BYTE val
CFU_WRATTR      equ 1006h       ; in: WORD offset,BYTE val
CFU_IRQHOOK     equ 1007h       ; in: BYTE irq
CFU_IRQCOUNT    equ 1008h       ; out: DWORD count (does not clear)
CFU_IRQUNHOOK   equ 1009h
CFU_PCICRD      equ 100Ah       ; in: BYTE reg (socket bank added) out: DWORD
CFU_PCICWR      equ 100Bh       ; in: BYTE reg, BYTE val
CFU_GETCONF     equ 100Ch       ; out: CFU_CONF (PnP-assigned resources)
CFU_USBINIT     equ 100Dh       ; in: BYTE lowspeed   out: DWORD INTSTAT
CFU_XACT        equ 100Eh       ; in: CFU_XREQ        out: CFU_XRSP
CFU_CTRL        equ 100Fh       ; in: CFU_CREQ        out: CFU_CRSP
CFU_HIDSTART    equ 1010h       ; in: BYTE ep,devaddr,mps,interval_ms
                                ;     [,BYTE mode: bit0 = post to VMOUSE]
CFU_HIDSTOP     equ 1011h
CFU_HIDREAD     equ 1012h       ; out: DWORD count + count*9 (len+8 data)
CFU_AUTOMOUSE   equ 1013h       ; in: BYTE 1=on/0=off  (hot-plug supervisor)
CFU_AUTOSTAT    equ 1014h       ; out: DWORD (b0 run, b1 armed, b8-15 state)
CFU_FORCEKEY    equ 1015h       ; in: raw scan bytes -> VKD_Force_Keys (test)
CFU_KBDSTART    equ 1016h       ; in: BYTE ep,devaddr,mps,interval_ms
CFU_KBDSTAT     equ 1017h       ; out: 6 DWORDs (pipeline counters, debug)
CFU_MSCSET      equ 1018h       ; in: BYTE addr, ep_in, ep_out (reset toggles)
CFU_MSCCMD      equ 1019h       ; in: BYTE cdblen,dir; WORD datalen; 16 cdb;
                                ;     [OUT data]  out: DWORD csw; [IN data]
CFU_IOSREG      equ 101Ah       ; register with IOS; out: DWORD reg_result
CFU_IOSSTAT     equ 101Bh       ; out: total, lastfunc, regresult, counts[16]
CFU_MSCMOUNT    equ 101Ch       ; ring-0 enumerate + READ CAPACITY;
                                ; out: DWORD blocks, DWORD block_size
CFU_IORSTAT     equ 101Dh       ; out: ior_total, ior_last, isp_cd_result,
                                ;      cfg_dcb, inq_dcb, ior_counts[48]
CFU_PEEK        equ 101Eh       ; in: DWORD linear addr, DWORD len (<=256)
                                ; out: len raw bytes (ring-0 read)
CFU_DCBENUM     equ 101Fh       ; out: DWORD count + up to 15 DCB addresses
CFU_IOSCFG      equ 1020h       ; broadcast AEP_CONFIG_DCB for our DCB to
                                ; all layer drivers; out: DWORD results
CFU_IOSVOL      equ 1021h       ; act as our own TSD: parse the MBR, create
                                ; a logical volume DCB, ISP_ASSOCIATE_DCB.
                                ; out: DWORD (create_res<<16)|assoc_res
CFU_VOLUP       equ 1022h       ; mount + bring the volume up now (manual
                                ; replug); out: DWORD (mounted<<16)|has_vol
CFU_VOLDOWN     equ 1023h       ; polite teardown; in: BYTE force (skip the
                                ; query-remove veto); out: DWORD 0 ok/1 veto
CFU_VOLSTAT     equ 1024h       ; out: mounted, log_dcb, part_start,
                                ;      part_len, drive_index
CFU_IORTRACE    equ 1025h       ; out: DWORD total + 32*(func,flags,lba,cnt)
CFU_IRQTEST     equ 1026h       ; in: BYTE irq; hook+count SOF ints ~1s with
                                ; exclusive chip ownership, park after;
                                ; out: DWORD count
CFU_HUBSTAT     equ 1027h       ; out: 18 DWORDs: nports, port, naddr, conn,
                                ; rescan, pwrfail, p2pg, estage, pstat[7],
                                ; hid_port, hd2_port, parked mask
CFU_ASYNCA      equ 1028h       ; in: BYTE 1/0 = stage-A IRQ completion
                                ; on/off; out: DWORD irqcount
CFU_ASYNCST     equ 1029h       ; out: 4 DWORDs: irqcount, windows completed
                                ; by IRQ, windows timed out, amode

VXD_VERSION     equ 00020037h   ; 2.37 - detach fix: supervisor resumes after
                                ;        mount + xbusy watchdog
                                ;        auto-arm, unplug, eject, replug

HIDRING_SLOTS   equ 32
HIDSLOT_SIZE    equ 9

; CFU_XREQ offsets (one raw transaction on EP A)
XQ_PIDEP        equ 0           ; BYTE  PID<<4 | endpoint
XQ_DEVADDR      equ 1           ; BYTE
XQ_CTL          equ 2           ; BYTE  EP A host control bits
XQ_LEN          equ 3           ; BYTE  transfer length (<= 120)
XQ_DATA         equ 4           ; BYTE[len] for OUT/SETUP payloads
; CFU_XRSP offsets
XR_STATUS       equ 0           ; DWORD SL811 packet status (FFFFFFFF timeout)
XR_GOT          equ 4           ; DWORD bytes received (IN)
XR_DATA         equ 8           ; BYTE[got]

; CFU_CREQ offsets (complete control transfer)
CQ_DEVADDR      equ 0           ; BYTE
CQ_MPS          equ 1           ; BYTE  max packet size of ep0 (8..64)
CQ_WLEN         equ 2           ; WORD  data stage length
CQ_SETUP        equ 4           ; BYTE[8] the SETUP packet
CFU_CREQ_SIZE   equ 12
; CFU_CRSP offsets
CR2_ERR         equ 0           ; DWORD 0 ok; low byte stage 1/2/3,
                                ;       byte1 = SL811 status (FF timeout)
CR2_GOT         equ 4           ; DWORD data-stage bytes transferred
CR2_DATA        equ 8           ; BYTE[wlen] for IN transfers
CFU_CRSP_HDR    equ 8

; CFU_CONF output structure offsets
CC_IOBASE       equ 0           ; WORD  Windows-assigned I/O base (0 = none)
CC_IRQ          equ 2           ; BYTE  Windows-assigned IRQ (FF = none)
CC_SOCKET       equ 3           ; BYTE  socket the card was found in (FF=?)
CC_FLAGS        equ 4           ; BYTE  bit0 devnode seen, bit1 started,
                                ;       bit2 COR fix applied
CC_CORRB        equ 5           ; BYTE  COR readback at CONFIG_START
CC_RSVD         equ 6           ; WORD
CFU_CONF_SIZE   equ 8

; CFU_SETUP input structure offsets
CS_IOBASE       equ 0           ; WORD  I/O base (0 = default 0x260)
CS_PCIC         equ 2           ; WORD  PCIC index port (0 = default 0x3E0)
CS_SOCKET       equ 4           ; BYTE  socket number 0..3
CS_COR          equ 5           ; BYTE  config index (0 = default 0x02)
CS_ATTRSEG      equ 6           ; WORD  attr window real-mode segment
                                ;       (0 = default 0xD000)
CFU_SETUP_SIZE  equ 8

; CFU_RESULT output structure offsets
CR_ERR          equ 0           ; DWORD 0=ok else CFUERR_*
CR_CORRB        equ 4           ; BYTE  COR readback (expect index|0x40)
CR_HWREV        equ 5           ; BYTE  SL811 reg 0x0E (0x20 = HS rev 1.5)
CR_PCICSTAT     equ 6           ; BYTE  PCIC interface status
CR_RSVD         equ 7           ; BYTE
CFU_RESULT_SIZE equ 8

CFUERR_NOCARD   equ 1           ; card-detect bits absent
CFUERR_NOREADY  equ 2           ; card never signalled READY
CFUERR_NOMAP    equ 3           ; _MapPhysToLinear failed
CFUERR_BUFFER   equ 4           ; ioctl buffer missing/too small
CFUERR_NOTENAB  equ 5           ; operation needs a prior CFU_ENABLE
CFUERR_BADIOCTL equ 6

;----------------------------------------------------------------------------
DECLARE_VIRTUAL_DEVICE CFU1, 1, 0, CFU1_Control, UNDEFINED_DEVICE_ID, \
                       UNDEFINED_INIT_ORDER

VxD_LOCKED_DATA_SEG
iobase          dw      260h            ; SL811 addr port; data = +1
pcicidx         dw      3E0h
sockoff         db      0               ; socket * 40h
enabled         db      0
attrseg         dw      0D000h
                dw      0
attrlin         dd      0               ; mapped linear addr of attr window
irqhandle       dd      0
irqcount        dd      0
vid             VPICD_IRQ_Descriptor    <>

; PnP state (filled by the CONFIGMG path)
pnp_devnode     dd      0
pnp_iobase      dw      0
pnp_irq         db      0FFh
pnp_socket      db      0FFh
pnp_flags       db      0               ; bit0 devnode, bit1 started, bit2 COR
pnp_corrb       db      0
                dw      0
savregs         db      8 dup (0)       ; cor_fix PCIC reg save area
xa_pidep        db      0               ; do_xact arguments
xa_addr         db      0
xa_ctl          db      0
xa_len          db      0
ct_toggle       db      0               ; control-transfer data toggle
xbusy           db      0               ; EP A ownership guard (timer skips)
                db      2 dup (0)
xa_budget       dd      50000           ; do_xact completion poll budget
xr_tries        dd      250             ; do_xact_retry NAK budget

; do_control (internal control transfer) parameters/results
ct_addr         db      0
ct_mps          db      8
                dw      0
ct_wlen         dd      0               ; wanted IN data length (<= mps here)
ct_got          dd      0
ct_setup        db      8 dup (0)
ct_buf          db      72 dup (0)      ; control-transfer IN buffer

; hot-plug supervisor state
sup_handle      dd      0
sup_run         db      0               ; auto-mouse enabled
sup_armed       db      0               ; poll currently armed by supervisor
sup_absent      db      0               ; consecutive "no device" ticks
sup_state       db      0               ; enumeration state machine
sup_mps         db      8
                dw      0

; HID poll loop state
hid_handle      dd      0               ; Set_Global_Time_Out handle (in ESI!)
hid_ep          db      0
hid_addr        db      0
hid_mps         db      0
hid_ivl         db      10
hid_tog         db      0
hid_run         db      0               ; 1 = keep polling (gate reschedule)
hid_mode        db      0               ; bit0 = post reports to VMOUSE
                db      1 dup (0)
hid_head        dd      0
hid_tail        dd      0
hidring         db      HIDRING_SLOTS*HIDSLOT_SIZE dup (0)
; HID boot buttons (b0=L b1=R b2=M) -> VMOUSE serial format (20h/10h/08h)
btntab          db      000h, 020h, 010h, 030h, 008h, 028h, 018h, 038h

;----------------------------------------------------------------------------
; keyboard state (mode bit1)
;----------------------------------------------------------------------------
kb_prev         db      8 dup (0)       ; previous boot-keyboard report
kb_held         db      0               ; HID usage of the repeating key
kb_ticks        dd      0               ; ticks the key has been held
; scan-code queue: tick (interrupt time) enqueues, a Schedule_Global_Event
; callback drains it at task time (VKD_Force_Keys is not IRQ-time safe).
kb_sbuf         db      128 dup (0)
kb_sh           dd      0               ; producer index (free-running)
kb_st           dd      0               ; consumer index
kb_sched        db      0               ; a drain event is pending
                db      3 dup (0)
kb_batch        db      128 dup (0)     ; contiguous drain buffer for VKD
; debug counters (CFU_KBDSTAT)
kb_dbg_reports  dd      0               ; keyboard reports processed
kb_dbg_enq      dd      0               ; scan bytes enqueued
kb_dbg_sched    dd      0               ; drain events scheduled
kb_dbg_drains   dd      0               ; drain callbacks fired
kb_dbg_forced   dd      0               ; scan bytes handed to VKD_Force_Keys

KB_DELAY        equ     60              ; typematic delay in ticks (~480ms)
KB_RATE         equ     4               ; repeat interval in ticks (~32ms)

; HID usage (keyboard page 07) -> PC scan code set 1.  Word entries:
; low byte = make code, bit 15 set = E0-prefixed, 0 = unmapped.
E0K             equ     8000h
kscan   dw      0, 0, 0, 0                              ; 00-03
        dw      1Eh,30h,2Eh,20h,12h,21h,22h,23h         ; 04-0B a-h
        dw      17h,24h,25h,26h,32h,31h,18h,19h         ; 0C-13 i-p
        dw      10h,13h,1Fh,14h,16h,2Fh,11h,2Dh         ; 14-1B q-x
        dw      15h,2Ch                                 ; 1C-1D y,z
        dw      02h,03h,04h,05h,06h,07h,08h,09h,0Ah,0Bh ; 1E-27 1-0
        dw      1Ch,01h,0Eh,0Fh,39h                     ; 28-2C enter esc bs tab spc
        dw      0Ch,0Dh,1Ah,1Bh,2Bh,2Bh                 ; 2D-32 - = [ ] \ #
        dw      27h,28h,29h,33h,34h,35h,3Ah             ; 33-39 ; ' ` , . / caps
        dw      3Bh,3Ch,3Dh,3Eh,3Fh,40h,41h,42h         ; 3A-41 F1-F8
        dw      43h,44h,57h,58h                         ; 42-45 F9-F12
        dw      37h+E0K,46h,0                           ; 46-48 prtsc scrlk pause(skip)
        dw      52h+E0K,47h+E0K,49h+E0K                 ; 49-4B ins home pgup
        dw      53h+E0K,4Fh+E0K,51h+E0K                 ; 4C-4E del end pgdn
        dw      4Dh+E0K,4Bh+E0K,50h+E0K,48h+E0K         ; 4F-52 right left down up
        dw      45h,35h+E0K,37h,4Ah,4Eh,1Ch+E0K         ; 53-58 numlk kp/ kp* kp- kp+ kpent
        dw      4Fh,50h,51h,4Bh,4Ch,4Dh,47h,48h,49h     ; 59-61 kp1-kp9
        dw      52h,53h,56h                             ; 62-64 kp0 kp. intl\
KSCAN_MAX equ   ($ - kscan) / 2

; HID modifier bits 0-7 -> scan codes (LCtrl LShift LAlt LGui RCtrl RShift RAlt RGui)
kmod    dw      1Dh, 2Ah, 38h, 5Bh+E0K, 1Dh+E0K, 36h, 38h+E0K, 5Ch+E0K
cmconfig        Config_Buff_s <>
VxD_LOCKED_DATA_ENDS

VxD_LOCKED_CODE_SEG

;----------------------------------------------------------------------------
; helpers: PCIC index/data access.  in: AH=reg (bank offset added), AL=value
;----------------------------------------------------------------------------
BeginProc pcic_wr
        push    edx
        push    eax
        movzx   edx, [pcicidx]
        mov     al, ah
        add     al, [sockoff]
        out     dx, al
        pop     eax
        inc     edx
        out     dx, al                  ; data
        pop     edx
        ret
EndProc pcic_wr

BeginProc pcic_rd                       ; in: AH=reg  out: AL=value
        push    edx
        movzx   edx, [pcicidx]
        mov     al, ah
        add     al, [sockoff]
        out     dx, al
        inc     edx
        in      al, dx
        pop     edx
        ret
EndProc pcic_rd

BeginProc iodelay                       ; in: ECX = ~microseconds
@@:     in      al, 80h
        loop    @B
        ret
EndProc iodelay

;----------------------------------------------------------------------------
; control dispatch
;----------------------------------------------------------------------------
BeginProc CFU1_Control
        Control_Dispatch SYS_DYNAMIC_DEVICE_INIT, CFU1_Init
        Control_Dispatch SYS_DYNAMIC_DEVICE_EXIT, CFU1_Exit
        Control_Dispatch W32_DEVICEIOCONTROL,     CFU1_Ioctl
        Control_Dispatch PNP_NEW_DEVNODE,         CFU1_NewDevnode
        clc
        ret
EndProc CFU1_Control

;----------------------------------------------------------------------------
; PNP_NEW_DEVNODE: EBX = devnode, EDX = load type.  Register our config
; handler so CONFIGMG tells us when the card's resources are assigned.
;----------------------------------------------------------------------------
BeginProc CFU1_NewDevnode
        cmp     edx, DLVXD_LOAD_DEVLOADER
        je      @F
        cmp     edx, DLVXD_LOAD_DRIVER
        jne     nd_ok                   ; not for us; report success anyway
@@:
        mov     [pnp_devnode], ebx
        or      [pnp_flags], 1
        VxDCall _CONFIGMG_Register_Device_Driver, <ebx, <OFFSET32 CFU1_ConfigHandler>, 0, <CM_REGISTER_DEVICE_DRIVER_DISABLEABLE OR CM_REGISTER_DEVICE_DRIVER_REMOVABLE>>
        ; EAX = CR_* result; pass it back
        clc
        ret
nd_ok:
        mov     eax, CR_SUCCESS
        clc
        ret
EndProc CFU1_NewDevnode

;----------------------------------------------------------------------------
; CONFIGMG config handler (C convention):
;   CONFIGRET handler(func, subfunc, devnode, refdata, flags)
;   [ebp+8]=func [ebp+12]=subfunc [ebp+16]=devnode [ebp+20]=ref [ebp+24]=flags
;----------------------------------------------------------------------------
BeginProc CFU1_ConfigHandler
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        push    edi

        mov     eax, [ebp+8]
        cmp     eax, CONFIG_START
        je      ch_start
        cmp     eax, CONFIG_STOP
        je      ch_stop
        cmp     eax, CONFIG_REMOVE
        je      ch_stop
        mov     eax, CR_DEFAULT
        jmp     ch_done

ch_start:
        ; fetch the resources Windows assigned to the card
        mov     ebx, [ebp+16]           ; devnode
        VxDCall _CONFIGMG_Get_Alloc_Log_Conf, <<OFFSET32 cmconfig>, ebx, CM_GET_ALLOC_LOG_CONF_ALLOC>
        test    eax, eax                ; CR_SUCCESS = 0
        jnz     ch_start_noconf
        cmp     [cmconfig.wNumIOPorts], 0
        je      @F
        mov     ax, word ptr [cmconfig.wIOPortBase]
        mov     [pnp_iobase], ax
        mov     [iobase], ax            ; adopt as the active I/O base
@@:     cmp     [cmconfig.wNumIRQs], 0
        je      @F
        mov     al, byte ptr [cmconfig.bIRQRegisters]
        mov     [pnp_irq], al
@@:
ch_start_noconf:
        or      [pnp_flags], 2          ; started
        call    cor_fix                 ; the reason this driver exists
        cmp     [enabled], 0            ; if the card came up, watch for a mouse
        jz      @F
        call    sup_start
        ; park the card's IRQ at the PCIC (steer to none).  The SL811's
        ; write-only address latch cannot be shared between a hardware IRQ
        ; handler and the polled engine (2.0D's boot hook triple-faulted the
        ; machine at the first mount); interrupts return only when the async
        ; engine owns the chip exclusively - see win/ASYNC-ENGINE.md.
        mov     al, 60h                 ; I/O card, reset off, IRQ = none
        mov     ah, 03h
        call    pcic_wr
        mov     eax, CR_SUCCESS
        jmp     ch_done

ch_stop:
        and     [pnp_flags], NOT 2
        call    sup_stop
        mov     byte ptr [msc_mounted], 0       ; card going away: fail I/O
        call    sched_voldown                   ; and drop the drive letter
        mov     eax, CR_SUCCESS

ch_done:
        pop     edi
        pop     esi
        pop     ebx
        pop     ebp
        ret
EndProc CFU1_ConfigHandler

;----------------------------------------------------------------------------
; cor_fix: find which socket carries an enabled I/O window covering our
; assigned iobase, then briefly borrow memory window 0 on that socket to
; write the REAL configuration register at attribute 0xFC (the CIS lies
; and says 0xF8, so the OS-written COR never landed).  PCIC window regs
; are saved and restored around the poke.
;----------------------------------------------------------------------------
BeginProc cor_fix
        push    ebx
        push    ecx
        push    edx
        push    esi

        ; --- find the socket: banks 0..3 ---
        xor     ecx, ecx
cf_scan:
        mov     al, cl
        shl     al, 6
        mov     [sockoff], al
        mov     ah, 01h
        call    pcic_rd
        and     al, 0Ch
        cmp     al, 0Ch                 ; card present?
        jne     cf_next
        mov     ah, 06h
        call    pcic_rd
        test    al, 40h                 ; I/O window 0 enabled?
        jz      cf_next
        mov     ah, 08h
        call    pcic_rd
        mov     bl, al
        mov     ah, 09h
        call    pcic_rd
        mov     bh, al                  ; BX = window start
        mov     dx, [pnp_iobase]
        test    dx, dx
        jnz     @F
        mov     dx, [iobase]
@@:     cmp     bx, dx
        jne     cf_next
        jmp     cf_found
cf_next:
        inc     ecx
        cmp     ecx, 4
        jb      cf_scan
        jmp     cf_out                  ; no socket found; leave flags unset

cf_found:
        mov     al, cl
        mov     [pnp_socket], al

        ; --- save PCIC regs 06,10-15 into savregs ---
        mov     ah, 06h
        call    pcic_rd
        mov     [savregs+0], al
        mov     ah, 10h
cf_save:
        call    pcic_rd
        movzx   ebx, ah
        mov     [savregs+ebx-0Fh], al   ; 10h -> +1 ... 15h -> +6
        inc     ah
        cmp     ah, 16h
        jb      cf_save

        ; --- map attribute window at attrseg, poke COR, restore ---
        movzx   ebx, [attrseg]
        shr     ebx, 8
        mov     ah, 10h
        mov     al, bl
        call    pcic_wr
        mov     ah, 11h
        mov     al, bh
        and     al, 3Fh
        call    pcic_wr
        lea     ecx, [ebx+3]
        mov     ah, 12h
        mov     al, cl
        call    pcic_wr
        mov     ah, 13h
        mov     al, ch
        and     al, 3Fh
        call    pcic_wr
        mov     eax, 0
        sub     eax, ebx
        and     eax, 3FFFh
        or      eax, 4000h
        push    eax
        mov     ah, 14h
        call    pcic_wr
        pop     eax
        mov     al, ah
        mov     ah, 15h
        call    pcic_wr
        mov     ah, 06h
        call    pcic_rd
        or      al, 01h
        mov     ah, 06h
        call    pcic_wr
        mov     ecx, 2000
        call    iodelay

        movzx   eax, [attrseg]
        shl     eax, 4
        VMMCall _MapPhysToLinear, <eax, 1000h, 0>
        cmp     eax, 0FFFFFFFFh
        je      cf_restore
        mov     [attrlin], eax
        mov     edx, eax
        mov     byte ptr [edx+1F8h], 42h    ; REAL COR: attr 0xFC
        mov     ecx, 2000
        call    iodelay
        mov     al, [edx+1F8h]
        mov     [pnp_corrb], al
        cmp     al, 42h
        jne     cf_restore
        or      [pnp_flags], 4          ; COR fix confirmed
        mov     [enabled], 1

cf_restore:
        ; --- restore PCIC regs 10-15 then 06 ---
        mov     ah, 10h
cf_rest:
        movzx   ebx, ah
        mov     al, [savregs+ebx-0Fh]
        call    pcic_wr
        inc     ah
        cmp     ah, 16h
        jb      cf_rest
        mov     ah, 06h
        mov     al, [savregs+0]
        call    pcic_wr

cf_out:
        pop     esi
        pop     edx
        pop     ecx
        pop     ebx
        ret
EndProc cor_fix

BeginProc CFU1_Init
        clc
        ret
EndProc CFU1_Init

BeginProc CFU1_Exit
        call    sup_stop
        call    hid_stop
        cmp     [irqhandle], 0
        jz      @F
        mov     eax, [irqhandle]
        VxDCall VPICD_Force_Default_Behavior
        mov     [irqhandle], 0
@@:     clc
        ret
EndProc CFU1_Exit

;----------------------------------------------------------------------------
; W32_DEVICEIOCONTROL:  ESI -> DIOCParams.  Return status in EAX (0 = ok).
;----------------------------------------------------------------------------
BeginProc CFU1_Ioctl
        mov     eax, [esi.dwIoControlCode]
        cmp     eax, DIOC_OPEN                  ; 0: CreateFile handshake
        jne     @F
        xor     eax, eax
        ret
@@:     cmp     eax, DIOC_CLOSEHANDLE           ; -1: cleanup on CloseHandle
        jne     @F
        cmp     [amode], 0                     ; async engine owns a resident
        jne     cioc_keephook                  ; hook — must outlive the tool
        call    ioc_unhook
cioc_keephook:
        cmp     [hid_mode], 0           ; any persistent feed (mouse OR
        jne     close_keep              ; keyboard) survives app exit
        call    hid_stop
close_keep:
        xor     eax, eax
        ret
@@:
        cmp     eax, CFU_GETVER
        je      ioc_getver
        cmp     eax, CFU_ENABLE
        je      ioc_enable
        cmp     eax, CFU_DISABLE
        je      ioc_disable
        cmp     eax, CFU_RDREG
        je      ioc_rdreg
        cmp     eax, CFU_WRREG
        je      ioc_wrreg
        cmp     eax, CFU_RDATTR
        je      ioc_rdattr
        cmp     eax, CFU_WRATTR
        je      ioc_wrattr
        cmp     eax, CFU_IRQHOOK
        je      ioc_irqhook
        cmp     eax, CFU_IRQCOUNT
        je      ioc_irqcount
        cmp     eax, CFU_IRQUNHOOK
        je      ioc_irqunhook2
        cmp     eax, CFU_PCICRD
        je      ioc_pcicrd
        cmp     eax, CFU_PCICWR
        je      ioc_pcicwr
        cmp     eax, CFU_GETCONF
        je      ioc_getconf
        cmp     eax, CFU_USBINIT
        je      ioc_usbinit
        cmp     eax, CFU_XACT
        je      ioc_xact
        cmp     eax, CFU_CTRL
        je      ioc_ctrl
        cmp     eax, CFU_HIDSTART
        je      ioc_hidstart
        cmp     eax, CFU_HIDSTOP
        je      ioc_hidstop
        cmp     eax, CFU_HIDREAD
        je      ioc_hidread
        cmp     eax, CFU_AUTOMOUSE
        je      ioc_automouse
        cmp     eax, CFU_AUTOSTAT
        je      ioc_autostat
        cmp     eax, CFU_FORCEKEY
        je      ioc_forcekey
        cmp     eax, CFU_KBDSTART
        je      ioc_kbdstart
        cmp     eax, CFU_KBDSTAT
        je      ioc_kbdstat
        cmp     eax, CFU_MSCSET
        je      ioc_mscset
        cmp     eax, CFU_MSCCMD
        je      ioc_msccmd
        cmp     eax, CFU_IOSREG
        je      ioc_iosreg
        cmp     eax, CFU_IOSSTAT
        je      ioc_iosstat
        cmp     eax, CFU_MSCMOUNT
        je      ioc_mscmount
        cmp     eax, CFU_IORSTAT
        je      ioc_iorstat
        cmp     eax, CFU_PEEK
        je      ioc_peek
        cmp     eax, CFU_DCBENUM
        je      ioc_dcbenum
        cmp     eax, CFU_IOSCFG
        je      ioc_ioscfg
        cmp     eax, CFU_IOSVOL
        je      ioc_iosvol
        cmp     eax, CFU_VOLUP
        je      ioc_volup
        cmp     eax, CFU_VOLDOWN
        je      ioc_voldown
        cmp     eax, CFU_VOLSTAT
        je      ioc_volstat
        cmp     eax, CFU_IORTRACE
        je      ioc_iortrace
        cmp     eax, CFU_IRQTEST
        je      ioc_irqtest
        cmp     eax, CFU_HUBSTAT
        je      ioc_hubstat
        cmp     eax, CFU_ASYNCA
        je      ioc_asynca
        cmp     eax, CFU_ASYNCST
        je      ioc_asyncst
        mov     eax, CFUERR_BADIOCTL
        ret
EndProc CFU1_Ioctl

; --- return dword in EAX through lpvOutBuffer, set bytes-returned = 4 -----
BeginProc ret_dword                     ; in: EAX=value  uses EDX
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      @F
        cmp     [esi.cbOutBuffer], 4
        jb      @F
        mov     [edx], eax
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      @F
        mov     dword ptr [edx], 4
@@:     xor     eax, eax
        ret
EndProc ret_dword

ioc_getver:
        mov     eax, VXD_VERSION
        jmp     ret_dword

;----------------------------------------------------------------------------
; CFU_ENABLE
;----------------------------------------------------------------------------
ioc_enable:
        ; defaults already in globals; override from input buffer if present
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ena_defaults
        cmp     [esi.cbInBuffer], CFU_SETUP_SIZE
        jb      ena_defaults
        mov     ax, [edx+CS_IOBASE]
        test    ax, ax
        jz      @F
        mov     [iobase], ax
@@:     mov     ax, [edx+CS_PCIC]
        test    ax, ax
        jz      @F
        mov     [pcicidx], ax
@@:     mov     al, [edx+CS_SOCKET]
        shl     al, 6                   ; * 40h
        mov     [sockoff], al
        mov     ax, [edx+CS_ATTRSEG]
        test    ax, ax
        jz      ena_defaults
        mov     [attrseg], ax
ena_defaults:

        ; card present?
        mov     ah, 01h
        call    pcic_rd
        and     al, 0Ch
        cmp     al, 0Ch
        je      @F
        mov     eax, CFUERR_NOCARD
        jmp     ena_fail
@@:
        ; power on 5V, deassert reset, wait READY
        mov     ah, 02h
        mov     al, 95h
        call    pcic_wr
        mov     ecx, 50000
        call    iodelay
        mov     ah, 03h
        mov     al, 40h
        call    pcic_wr
        mov     ecx, 20000
        call    iodelay
        mov     ecx, 100                ; up to ~100ms more
ena_rdywait:
        push    ecx
        mov     ah, 01h
        call    pcic_rd
        test    al, 20h
        pop     ecx
        jnz     ena_ready
        push    ecx
        mov     ecx, 1000
        call    iodelay
        pop     ecx
        loop    ena_rdywait
        mov     eax, CFUERR_NOREADY
        jmp     ena_fail
ena_ready:

        ; map the attribute window physical page so we can poke the COR
        movzx   eax, [attrseg]
        shl     eax, 4                  ; phys = seg << 4
        VMMCall _MapPhysToLinear, <eax, 1000h, 0>
        mov     [attrlin], eax
        cmp     eax, 0FFFFFFFFh
        jne     @F
        mov     eax, CFUERR_NOMAP
        jmp     ena_fail
@@:
        ; program attribute memory window 0 -> attrseg, card offset 0, REG
        movzx   ebx, [attrseg]
        shr     ebx, 8                  ; window start in 4K pages (e.g. 0D0h)
        mov     ah, 10h
        mov     al, bl
        call    pcic_wr                 ; start low
        mov     ah, 11h
        mov     al, bh
        and     al, 3Fh
        call    pcic_wr                 ; start high
        lea     ecx, [ebx+3]
        mov     ah, 12h
        mov     al, cl
        call    pcic_wr                 ; stop low
        mov     ah, 13h
        mov     al, ch
        and     al, 3Fh
        call    pcic_wr                 ; stop high
        mov     eax, 0
        sub     eax, ebx
        and     eax, 3FFFh
        or      eax, 4000h              ; REG = attribute space
        push    eax
        mov     ah, 14h
        call    pcic_wr                 ; offset low (AL already low byte)
        pop     eax
        mov     al, ah
        push    eax
        mov     ah, 15h
        call    pcic_wr                 ; offset high
        pop     eax
        mov     ah, 06h
        call    pcic_rd
        or      al, 01h
        mov     ah, 06h
        call    pcic_wr                 ; enable memory window 0
        mov     ecx, 2000
        call    iodelay

        ; *** the quirk: write the config index to attribute 0xFC ***
        ; (window offset 0x1F8; the CIS says 0xF8 and the CIS is wrong)
        mov     edx, [attrlin]
        mov     al, 02h                 ; default config index
        mov     [edx+1F8h], al
        mov     ecx, 2000
        call    iodelay
        mov     al, [edx+1F8h]          ; readback (expect index | 40h)
        push    eax                     ; save COR readback

        ; socket to I/O mode, map I/O window 0 over the SL811
        mov     ah, 03h
        mov     al, 60h
        call    pcic_wr
        mov     bx, [iobase]
        mov     ah, 08h
        mov     al, bl
        call    pcic_wr
        mov     ah, 09h
        mov     al, bh
        call    pcic_wr
        add     bx, 7
        mov     ah, 0Ah
        mov     al, bl
        call    pcic_wr
        mov     ah, 0Bh
        mov     al, bh
        call    pcic_wr
        mov     ah, 07h
        mov     al, 00h                 ; 8-bit timing
        call    pcic_wr
        mov     ah, 06h
        mov     al, 41h                 ; attr window + I/O window 0
        call    pcic_wr
        mov     ecx, 1000
        call    iodelay
        mov     [enabled], 1

        ; read SL811 hwrev (reg 0x0E)
        movzx   edx, [iobase]
        mov     al, 0Eh
        out     dx, al
        inc     edx
        in      al, dx
        mov     bl, al                  ; BL = hwrev
        mov     ah, 01h
        call    pcic_rd
        mov     bh, al                  ; BH = PCIC status
        pop     ecx                     ; CL = COR readback

        ; fill CFU_RESULT
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ena_oknobuf
        cmp     [esi.cbOutBuffer], CFU_RESULT_SIZE
        jb      ena_oknobuf
        mov     dword ptr [edx+CR_ERR], 0
        mov     [edx+CR_CORRB], cl
        mov     [edx+CR_HWREV], bl
        mov     [edx+CR_PCICSTAT], bh
        mov     byte ptr [edx+CR_RSVD], 0
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      ena_oknobuf
        mov     dword ptr [edx], CFU_RESULT_SIZE
ena_oknobuf:
        xor     eax, eax
        ret

ena_fail:                               ; EAX = CFUERR_*
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      @F
        cmp     [esi.cbOutBuffer], CFU_RESULT_SIZE
        jb      @F
        mov     [edx+CR_ERR], eax
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      @F
        mov     dword ptr [edx], CFU_RESULT_SIZE
@@:     xor     eax, eax                ; ioctl itself succeeded; err in struct
        ret

;----------------------------------------------------------------------------
ioc_disable:
        call    ioc_unhook
        mov     ah, 06h
        mov     al, 00h
        call    pcic_wr
        mov     ah, 03h
        mov     al, 00h
        call    pcic_wr
        mov     ah, 02h
        mov     al, 00h
        call    pcic_wr
        mov     [enabled], 0
        xor     eax, eax
        ret

;----------------------------------------------------------------------------
ioc_rdreg:
        cmp     [enabled], 0
        je      ioc_notena
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        mov     al, [edx]               ; reg
        movzx   edx, [iobase]
        out     dx, al
        inc     edx
        in      al, dx
        movzx   eax, al
        jmp     ret_dword

ioc_wrreg:
        cmp     [enabled], 0
        je      ioc_notena
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 2
        jb      ioc_badbuf
        mov     ax, [edx]               ; AL=reg AH=val
        movzx   edx, [iobase]
        out     dx, al
        inc     edx
        mov     al, ah
        out     dx, al
        xor     eax, eax
        ret

ioc_rdattr:
        cmp     [enabled], 0
        je      ioc_notena
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        movzx   eax, word ptr [edx]     ; offset within window
        and     eax, 0FFFh
        mov     edx, [attrlin]
        movzx   eax, byte ptr [edx+eax]
        jmp     ret_dword

ioc_wrattr:
        cmp     [enabled], 0
        je      ioc_notena
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 3
        jb      ioc_badbuf
        movzx   eax, word ptr [edx]
        and     eax, 0FFFh
        mov     cl, [edx+2]
        mov     edx, [attrlin]
        mov     [edx+eax], cl
        xor     eax, eax
        ret

ioc_getconf:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], CFU_CONF_SIZE
        jb      ioc_badbuf
        mov     ax, [pnp_iobase]
        mov     [edx+CC_IOBASE], ax
        mov     al, [pnp_irq]
        mov     [edx+CC_IRQ], al
        mov     al, [pnp_socket]
        mov     [edx+CC_SOCKET], al
        mov     al, [pnp_flags]
        mov     [edx+CC_FLAGS], al
        mov     al, [pnp_corrb]
        mov     [edx+CC_CORRB], al
        mov     word ptr [edx+CC_RSVD], 0
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      @F
        mov     dword ptr [edx], CFU_CONF_SIZE
@@:     xor     eax, eax
        ret

ioc_pcicrd:
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        mov     ah, [edx]               ; PCIC register
        call    pcic_rd
        movzx   eax, al
        jmp     ret_dword

ioc_pcicwr:
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 2
        jb      ioc_badbuf
        mov     ax, [edx]               ; AL=reg AH=val
        xchg    al, ah
        call    pcic_wr
        xor     eax, eax
        ret

;----------------------------------------------------------------------------
; CFU_USBINIT: bus reset (SE0 ~30ms) -> host mode (DSWAP+LSPD if lowspeed)
; -> SOF counters + kickstart -> INTENA SOF-timer gate.  Returns INTSTAT.
;----------------------------------------------------------------------------
ioc_usbinit:
        cmp     [enabled], 0
        je      ioc_notena
        call    sup_stop                ; manual USB test takes over from auto
        call    hid_stop                ; a bus reset invalidates the poll
        mov     edx, [esi.lpvInBuffer]
        xor     ecx, ecx                ; CL = lowspeed flag
        test    edx, edx
        jz      @F
        mov     cl, [edx]
@@:
        mov     al, 06h                 ; INTENA off
        mov     ah, 00h
        call    sl_wr
        mov     al, 05h                 ; CTL1: force SE0 (USB reset)
        mov     ah, 08h
        call    sl_wr
        push    ecx
        mov     ecx, 30000              ; ~30ms
        call    iodelay
        pop     ecx
        mov     al, 05h
        mov     ah, 00h
        call    sl_wr
        push    ecx
        mov     ecx, 3000
        call    iodelay
        pop     ecx
        mov     al, 0Eh                 ; SOF counter low
        mov     ah, 0E0h
        call    sl_wr
        mov     ah, 0AEh                ; CTL2: HOST | SOF-high
        test    cl, cl
        jz      @F
        or      ah, 40h                 ; + DSWAP for low speed
@@:     mov     al, 0Fh
        call    sl_wr
        mov     al, 02h                 ; SOF kickstart: EP A len 0
        mov     ah, 00h
        call    sl_wr
        mov     al, 03h                 ; PID=SOF ep0
        mov     ah, 50h
        call    sl_wr
        mov     al, 04h
        mov     ah, 00h
        call    sl_wr
        mov     al, 00h                 ; ARM
        mov     ah, 01h
        call    sl_wr
        mov     ah, 01h                 ; CTL1: SOF_ENA (+LSPD if LS)
        test    cl, cl
        jz      @F
        or      ah, 20h
@@:     mov     al, 05h
        call    sl_wr
        mov     al, 0Dh
        mov     ah, 0FFh
        call    sl_wr
        mov     al, 06h                 ; INTENA bit4 gates the SOF timer
        mov     ah, 10h
        call    sl_wr
        mov     ecx, 100000             ; ~100ms post-reset recovery
        call    iodelay
        mov     al, 0Dh
        call    sl_rd
        movzx   eax, al
        jmp     ret_dword

;----------------------------------------------------------------------------
; CFU_XACT: one raw transaction.  Payload (OUT/SETUP) copied to chip RAM
; 0x10 first; IN data copied back to the response.
;----------------------------------------------------------------------------
ioc_xact:
        cmp     [enabled], 0
        je      ioc_notena
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 4
        jb      ioc_badbuf
        mov     al, [edx+XQ_PIDEP]
        mov     [xa_pidep], al
        mov     al, [edx+XQ_DEVADDR]
        mov     [xa_addr], al
        mov     al, [edx+XQ_CTL]
        mov     [xa_ctl], al
        mov     al, [edx+XQ_LEN]
        mov     [xa_len], al
        mov     [xbusy], 1              ; keep the HID timer off EP A
        ; copy any outbound payload into chip RAM
        push    esi                     ; preserve DIOCParams ptr
        test    byte ptr [xa_ctl], 04h
        jz      @F
        movzx   ecx, [xa_len]
        jecxz   @F
        lea     esi, [edx+XQ_DATA]
        mov     al, 10h
        call    buf_to_chip
@@:
        call    do_xact
        mov     [xbusy], 0
        pop     esi
        jnc     @F
        mov     eax, 0FFFFFFFFh         ; timeout marker
        jmp     xact_status
@@:     movzx   eax, al
xact_status:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], XR_DATA + 120
        jb      ioc_badbuf
        mov     [edx+XR_STATUS], eax
        mov     dword ptr [edx+XR_GOT], 0
        ; IN transaction that ACKed: pull the data back
        cmp     eax, 0FFFFFFFFh
        je      xact_ret
        test    al, 01h                 ; ACK?
        jz      xact_ret
        test    byte ptr [xa_ctl], 04h  ; only for IN
        jnz     xact_ret
        push    eax
        mov     al, 04h                 ; remaining count
        call    sl_rd
        mov     cl, [xa_len]
        sub     cl, al                  ; got = len - remaining
        movzx   ecx, cl
        mov     [edx+XR_GOT], ecx
        jecxz   xact_pop
        push    edi
        lea     edi, [edx+XR_DATA]
        mov     al, 10h
        call    buf_from_chip
        pop     edi
xact_pop:
        pop     eax
xact_ret:
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      @F
        mov     dword ptr [edx], XR_DATA + 120
@@:     xor     eax, eax
        ret

;----------------------------------------------------------------------------
; CFU_CTRL: complete control transfer with NAK retry and data toggles.
;----------------------------------------------------------------------------
ioc_ctrl:
        cmp     [enabled], 0
        je      ioc_notena
        mov     [xr_tries], 250         ; generous NAK budget for ring-3 path
        push    ebx
        push    edi
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ctrl_badbuf
        cmp     [esi.cbInBuffer], CFU_CREQ_SIZE
        jb      ctrl_badbuf
        mov     edi, [esi.lpvOutBuffer]
        test    edi, edi
        jz      ctrl_badbuf
        mov     eax, [esi.cbOutBuffer]
        sub     eax, CFU_CRSP_HDR
        jb      ctrl_badbuf
        movzx   ecx, word ptr [edx+CQ_WLEN]
        cmp     eax, ecx                ; room for the whole data stage?
        jb      ctrl_badbuf
        mov     dword ptr [edi+CR2_ERR], 0
        mov     dword ptr [edi+CR2_GOT], 0

        mov     al, [edx+CQ_DEVADDR]
        mov     [xa_addr], al
        mov     [xbusy], 1              ; keep the HID timer off EP A

        ; ---- SETUP stage: 8 bytes, DATA0, OUT ----
        push    esi
        push    edx
        lea     esi, [edx+CQ_SETUP]
        mov     al, 10h
        mov     ecx, 8
        call    buf_to_chip
        pop     edx
        pop     esi
        mov     [xa_pidep], 0D0h
        mov     [xa_len], 8
        mov     al, 07h
        or      al, [ct_pre]
        mov     [xa_ctl], al           ; ARM|ENABLE|OUT, DATA0
        call    do_xact_retry
        jc      ctrl_setup_tmo
        test    al, 01h
        jnz     ctrl_data
ctrl_setup_tmo:
        mov     ah, al
        jnc     @F
        mov     ah, 0FFh
@@:     mov     al, 1                   ; stage 1 failed
        jmp     ctrl_fail

        ; ---- DATA stage (IN only; OUT data stage not needed yet) ----
ctrl_data:
        movzx   ebx, word ptr [edx+CQ_WLEN]
        test    ebx, ebx
        jz      ctrl_status
        test    byte ptr [edx+CQ_SETUP], 80h
        jz      ctrl_status
        mov     [ct_toggle], 40h        ; DATA1 first
ctrl_data_loop:
        mov     al, [edx+CQ_MPS]        ; always request a full packet;
        mov     [xa_len], al            ; the device short-terminates
        mov     [xa_pidep], 90h         ; IN, ep 0
        mov     al, [ct_toggle]
        or      al, 03h                 ; ARM|ENABLE
        mov     [xa_ctl], al
        call    do_xact_retry
        jc      ctrl_data_tmo
        test    al, 01h                 ; ACK?
        jz      ctrl_data_bad
        mov     al, 04h                 ; remaining count
        call    sl_rd
        mov     ah, [xa_len]
        sub     ah, al
        movzx   ecx, ah                 ; ECX = got this chunk
        push    esi
        push    edi
        push    ecx
        mov     eax, [edi+CR2_GOT]
        lea     edi, [edi+CR2_DATA+eax]
        mov     al, 10h
        call    buf_from_chip
        pop     ecx
        pop     edi
        pop     esi
        add     [edi+CR2_GOT], ecx
        xor     [ct_toggle], 40h
        cmp     ecx, ebx
        jae     ctrl_data_done          ; got everything requested
        sub     ebx, ecx
        movzx   eax, byte ptr [edx+CQ_MPS]
        cmp     ecx, eax
        jb      ctrl_data_done          ; short packet = device done early
        jmp     ctrl_data_loop
ctrl_data_tmo:
        mov     ah, 0FFh
        mov     al, 2
        jmp     ctrl_fail
ctrl_data_bad:
        mov     ah, al
        mov     al, 2
        jmp     ctrl_fail
ctrl_data_done:

        ; ---- STATUS stage: opposite direction, DATA1, zero length ----
ctrl_status:
        mov     [xa_len], 0
        test    byte ptr [edx+CQ_SETUP], 80h
        jz      ctrl_status_in
        mov     [xa_pidep], 10h         ; OUT status after IN data
        mov     al, 47h
        or      al, [ct_pre]
        mov     [xa_ctl], al           ; ARM|ENABLE|OUT|DATA1
        jmp     @F
ctrl_status_in:
        mov     [xa_pidep], 90h         ; IN status after OUT/no data
        mov     [xa_ctl], 43h           ; ARM|ENABLE|DATA1
@@:     call    do_xact_retry
        jc      ctrl_status_tmo
        test    al, 01h
        jnz     ctrl_ok
        mov     ah, al
        mov     al, 3
        jmp     ctrl_fail
ctrl_status_tmo:
        mov     ah, 0FFh
        mov     al, 3
        jmp     ctrl_fail

ctrl_ok:
        xor     eax, eax
        jmp     ctrl_out
ctrl_fail:
        movzx   eax, ax                 ; AL=stage AH=status
        mov     [edi+CR2_ERR], eax
        xor     eax, eax
ctrl_out:
        mov     [xbusy], 0
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      @F
        mov     ecx, [edi+CR2_GOT]
        add     ecx, CFU_CRSP_HDR
        mov     [edx], ecx
@@:     pop     edi
        pop     ebx
        xor     eax, eax
        ret
ctrl_badbuf:
        pop     edi
        pop     ebx
        mov     eax, CFUERR_BUFFER
        ret

;----------------------------------------------------------------------------
; HID poll loop control
;----------------------------------------------------------------------------
ioc_hidstart:
        cmp     [enabled], 0
        je      ioc_notena
        call    sup_stop                ; manual poll replaces the supervisor
        cmp     [hid_handle], 0
        jnz     ioc_badbuf              ; already running
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 4
        jb      ioc_badbuf
        mov     al, [edx]
        mov     [hid_ep], al
        mov     al, [edx+1]
        mov     [hid_addr], al
        mov     al, [edx+2]
        mov     [hid_mps], al
        mov     al, [edx+3]
        test    al, al
        jnz     @F
        mov     al, 10                  ; default 10ms
@@:     mov     [hid_ivl], al
        mov     [hid_mode], 0
        cmp     [esi.cbInBuffer], 5     ; optional 5th byte: mode
        jb      @F
        mov     al, [edx+4]
        mov     [hid_mode], al
@@:     call    hid_arm
        xor     eax, eax
        ret

; hid_arm: start the poll loop from the hid_* globals (ep/addr/mps/ivl/mode)
BeginProc hid_arm
        mov     [hid_tog], 0
        mov     [hid_head], 0
        mov     [hid_tail], 0
        mov     [hid_run], 1
        push    esi
        movzx   eax, [hid_ivl]
        xor     edx, edx
        mov     esi, OFFSET32 CFU1_HidTick
        VMMCall Set_Global_Time_Out
        mov     [hid_handle], esi       ; handle returned in ESI
        pop     esi
        ret
EndProc hid_arm

ioc_hidstop:
        call    hid_stop
        xor     eax, eax
        ret

BeginProc hid_stop
        mov     [hid_run], 0            ; stray in-flight tick won't re-arm
        mov     [hid_mode], 0
        cmp     [hid_handle], 0
        jz      @F
        push    esi
        mov     esi, [hid_handle]       ; Cancel_Time_Out takes ESI
        mov     [hid_handle], 0
        VMMCall Cancel_Time_Out
        pop     esi
@@:     ret
EndProc hid_stop

ioc_hidread:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 4 + HIDRING_SLOTS*HIDSLOT_SIZE
        jb      ioc_badbuf
        push    ebx
        push    edi
        mov     edi, edx
        add     edi, 4                  ; reports start after the count
        xor     ebx, ebx                ; EBX = report count
        mov     ecx, [hid_head]         ; snapshot: timer only advances head
hr_loop:
        mov     eax, [hid_tail]
        cmp     eax, ecx
        je      hr_done
        push    ecx
        push    esi
        lea     esi, [eax+eax*8]
        add     esi, OFFSET32 hidring
        mov     ecx, HIDSLOT_SIZE
        rep     movsb                   ; copy len byte + 8 data bytes
        pop     esi
        pop     ecx
        inc     eax
        and     eax, HIDRING_SLOTS-1
        mov     [hid_tail], eax
        inc     ebx
        jmp     hr_loop
hr_done:
        mov     [edx], ebx              ; count
        mov     eax, [esi.lpcbBytesReturned]
        test    eax, eax
        jz      @F
        lea     ecx, [ebx+ebx*8]
        add     ecx, 4
        mov     [eax], ecx
@@:     pop     edi
        pop     ebx
        xor     eax, eax
        ret

ioc_automouse:
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     byte ptr [edx], 0
        jz      am_off
        cmp     [enabled], 0
        je      ioc_notena
        call    sup_start
        xor     eax, eax
        ret
am_off:
        call    sup_stop
        xor     eax, eax
        ret

ioc_autostat:
        movzx   eax, [hub_naddr]        ; b24..: next downstream addr
        shl     eax, 8
        mov     al, [hub_port]          ; b16..: port cursor
        shl     eax, 8
        mov     al, [sup_state]         ; b8..: state
        shl     eax, 8
        mov     al, 0
        cmp     [sup_run], 0
        jz      @F
        or      al, 1
@@:     cmp     [sup_armed], 0
        jz      @F
        or      al, 2
@@:     jmp     ret_dword

; CFU_HUBSTAT: hub-descent diagnostics, 18 DWORDs
ioc_hubstat:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 72
        jb      ioc_badbuf
        movzx   eax, byte ptr [hub_nports]
        mov     [edx+0], eax
        movzx   eax, byte ptr [hub_port]
        mov     [edx+4], eax
        movzx   eax, byte ptr [hub_naddr]
        mov     [edx+8], eax
        movzx   eax, byte ptr [hub_conn]
        mov     [edx+12], eax
        movzx   eax, byte ptr [hub_rescan]
        mov     [edx+16], eax
        movzx   eax, byte ptr [hub_pwrfail]
        mov     [edx+20], eax
        movzx   eax, byte ptr [hub_p2pg]
        mov     [edx+24], eax
        movzx   eax, byte ptr [hub_estage]
        mov     [edx+28], eax
        xor     ecx, ecx
@@:     mov     eax, [hub_pstat + ecx*4]
        mov     [edx+32+ecx*4], eax
        inc     ecx
        cmp     ecx, 7
        jb      @B
        movzx   eax, byte ptr [hid_port]
        mov     [edx+60], eax
        movzx   eax, byte ptr [hd2_port]
        mov     [edx+64], eax
        movzx   eax, byte ptr [hub_parked]
        mov     [edx+68], eax
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 72
@@:     xor     eax, eax
        ret

; CFU_FORCEKEY: bulk-inject the raw scan-code buffer via VKD_Force_Keys
; (VKD self-test: ESI = buffer, ECX = count).
ioc_forcekey:
        call    sup_stop                ; keep the cursor still during the test
        mov     ecx, [esi.lpvInBuffer]
        test    ecx, ecx
        jz      ioc_badbuf
        mov     eax, [esi.cbInBuffer]
        test    eax, eax
        jz      fk_done
        push    esi
        push    edi
        mov     esi, ecx                ; ESI = scan-code buffer
        mov     ecx, eax                ; ECX = count
        VxDCall  VKD_Force_Keys
        pop     edi
        pop     esi
fk_done:
        xor     eax, eax
        ret

; CFU_KBDSTART: arm the poll loop in keyboard mode (hid_mode bit1)
ioc_kbdstart:
        cmp     [enabled], 0
        je      ioc_notena
        call    sup_stop
        cmp     [hid_handle], 0
        jnz     ioc_badbuf
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 4
        jb      ioc_badbuf
        mov     al, [edx]
        mov     [hid_ep], al
        mov     al, [edx+1]
        mov     [hid_addr], al
        mov     al, [edx+2]
        mov     [hid_mps], al
        mov     al, [edx+3]
        test    al, al
        jnz     @F
        mov     al, 8
@@:     mov     [hid_ivl], al
        mov     [hid_mode], 2           ; keyboard
        ; reset keyboard diff + scan-code queue state
        xor     eax, eax
        mov     dword ptr [kb_prev], eax
        mov     dword ptr [kb_prev+4], eax
        mov     [kb_held], al
        mov     [kb_ticks], eax
        mov     [kb_sh], eax
        mov     [kb_st], eax
        mov     [kb_sched], al
        call    hid_arm
        xor     eax, eax
        ret

; CFU_KBDSTAT: dump the keyboard pipeline counters for diagnosis
ioc_kbdstat:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 24
        jb      ioc_badbuf
        mov     eax, [kb_dbg_reports]
        mov     [edx+0], eax
        mov     eax, [kb_dbg_enq]
        mov     [edx+4], eax
        mov     eax, [kb_dbg_sched]
        mov     [edx+8], eax
        mov     eax, [kb_dbg_drains]
        mov     [edx+12], eax
        mov     eax, [kb_dbg_forced]
        mov     [edx+16], eax
        movzx   eax, [hid_mode]
        mov     [edx+20], eax
        mov     eax, [esi.lpcbBytesReturned]
        test    eax, eax
        jz      @F
        mov     dword ptr [eax], 24
@@:     xor     eax, eax
        ret

; CFU_MSCSET: set the mass-storage device address + bulk endpoints,
; reset data toggles (call after the ring-3 tool does SET_CONFIGURATION)
ioc_mscset:
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 3
        jb      ioc_badbuf
        mov     al, [edx+0]
        mov     [msc_addr], al
        mov     al, [edx+1]
        mov     [msc_epin], al
        mov     al, [edx+2]
        mov     [msc_epout], al
        mov     byte ptr [msc_togin], 0
        mov     byte ptr [msc_togout], 0
        mov     dword ptr [msc_tag], 1
        xor     eax, eax
        ret

; CFU_MSCCMD: run one BOT command.  in: [0]=cdblen [1]=dir [2:2]=datalen
; [4:16]=cdb, then OUT data.  out: [0:4]=CSW status (FFFFFFFF=fail), IN data.
ioc_msccmd:
        cmp     [enabled], 0
        je      ioc_notena
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 20
        jb      ioc_badbuf
        mov     [xbusy], 1
        mov     al, [edx+0]
        mov     [msc_cdblen], al
        mov     al, [edx+1]
        mov     [msc_dir], al
        movzx   eax, word ptr [edx+2]
        mov     [msc_datalen], eax
        push    esi
        push    edi
        cld
        lea     esi, [edx+4]
        mov     edi, OFFSET32 msc_cdb
        mov     ecx, 16
        rep     movsb
        pop     edi
        pop     esi
        cmp     byte ptr [msc_dir], 0
        jne     mcc_in
        mov     edx, [esi.lpvInBuffer]  ; OUT data follows the 20-byte header
        add     edx, 20
        mov     [msc_databuf], edx
        jmp     mcc_go
mcc_in:
        mov     edx, [esi.lpvOutBuffer] ; IN data lands after the 4-byte CSW
        test    edx, edx
        jz      mcc_bad
        add     edx, 4
        mov     [msc_databuf], edx
mcc_go:
        push    esi                     ; msc_bot trashes ESI (DIOCParams ptr)
        call    msc_bot
        mov     [xbusy], 0
        pop     esi
        jc      mcc_failw
        movzx   eax, al                 ; CSW status
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      mcc_len
        mov     [edx], eax
        jmp     mcc_len
mcc_failw:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      mcc_len
        mov     dword ptr [edx], 0FFFFFFFFh
mcc_len:
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      mcc_done
        mov     eax, [msc_datalen]
        add     eax, 4
        mov     [ecx], eax
mcc_done:
        xor     eax, eax
        ret
mcc_bad:
        mov     [xbusy], 0
        mov     eax, CFUERR_BUFFER
        ret

; CFU_IOSREG: register with IOS (dynamic).  IOS calls our AER during this.
ioc_iosreg:
        push    esi
        call    do_iosreg
        pop     esi
        mov     eax, [ios_regresult]
        jmp     ret_dword

; CFU_IOSSTAT: dump the AEP observation counters
ioc_iosstat:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 80    ; 4 dwords + 16 counts
        jb      ioc_badbuf
        mov     eax, [aep_total]
        mov     [edx+0], eax
        mov     eax, [aep_lastfunc]
        mov     [edx+4], eax
        mov     eax, [ios_regresult]
        mov     [edx+8], eax
        mov     eax, [cfu_ddb_ptr]
        mov     [edx+12], eax
        push    esi
        push    edi
        cld
        mov     esi, OFFSET32 aep_counts
        lea     edi, [edx+16]
        mov     ecx, 16
        rep     movsd
        pop     edi
        pop     esi
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 80
@@:     xor     eax, eax
        ret

; CFU_IORSTAT: dump the IOR observation counters
ioc_iorstat:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 212   ; 5 dwords + 48 counts
        jb      ioc_badbuf
        mov     eax, [ior_total]
        mov     [edx+0], eax
        mov     eax, [ior_last]
        mov     [edx+4], eax
        mov     eax, [isp_cd_result]
        mov     [edx+8], eax
        mov     eax, [cfg_dcb_ptr]
        mov     [edx+12], eax
        mov     eax, [inq_dcb_ptr]
        mov     [edx+16], eax
        push    esi
        push    edi
        cld
        mov     esi, OFFSET32 ior_counts
        lea     edi, [edx+20]
        mov     ecx, 48
        rep     movsd
        pop     edi
        pop     esi
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 212
@@:     xor     eax, eax
        ret

; CFU_PEEK: ring-0 read of linear memory (debug).  in: addr, len (<=256)
ioc_peek:
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 8
        jb      ioc_badbuf
        mov     eax, [edx+0]            ; source linear address
        mov     ecx, [edx+4]            ; length
        cmp     ecx, 256
        ja      ioc_badbuf
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], ecx
        jb      ioc_badbuf
        push    esi
        push    edi
        cld
        mov     esi, eax
        mov     edi, edx
        push    ecx
        rep     movsb
        pop     ecx
        pop     edi
        pop     esi
        mov     edx, [esi.lpcbBytesReturned]
        test    edx, edx
        jz      @F
        mov     [edx], ecx
@@:     xor     eax, eax
        ret

; CFU_DCBENUM: walk all DCBs via ISP_GET_FIRST_NEXT_DCB (needs cfu_ilb from
; a prior IOS_Register).  out: DWORD count + up to 15 DCB linear addresses.
ioc_dcbenum:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 64
        jb      ioc_badbuf
        cmp     dword ptr [cfu_ilb], 0  ; ILB filled in by IOS_Register?
        je      ioc_badbuf
        push    ebx
        push    edi
        mov     ebx, edx                ; EBX = out buffer (callee-saved)
        mov     dword ptr [ebx], 0      ; count = 0
        sub     esp, 16                 ; ISP_GET_FRST_NXT_DCB on the stack
        mov     dword ptr [esp+4], 0    ; dcb_offset = 0 -> first
denum_loop:
        mov     word ptr [esp+0], 8     ; ISP_func = ISP_GET_FIRST_NEXT_DCB
        mov     word ptr [esp+2], 0     ; ISP_result
        mov     dword ptr [esp+8], 0    ; found_dcb (out)
        mov     byte ptr [esp+12], 0FFh ; any device type
        push    esp                     ; -> the ISP (value before push)
        call    dword ptr [cfu_ilb]
        add     esp, 4
        cmp     word ptr [esp+2], 0
        jne     denum_done
        mov     eax, [esp+8]
        test    eax, eax
        jz      denum_done
        mov     ecx, [ebx]
        cmp     ecx, 15
        jae     denum_done
        mov     [ebx+4+ecx*4], eax
        inc     dword ptr [ebx]
        mov     [esp+4], eax            ; continue from this DCB
        jmp     denum_loop
denum_done:
        add     esp, 16
        pop     edi
        pop     ebx
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 64
@@:     xor     eax, eax
        ret

; CFU_IOSCFG: broadcast AEP_CONFIG_DCB for our claimed DCB so already-
; registered layer drivers (disk TSD, voltrack) get offered it.  IOS only
; ran OUR config pass during dynamic registration; the TSD layers never saw
; the DCB (DCB_DEV_MUST_CONFIGURE still set).  Appy time only - we are in a
; Win32 DeviceIoControl, which qualifies.
; out: DWORD = (broadcast ISP_result << 16) | AEP_result after broadcast
ioc_ioscfg:
        cmp     dword ptr [cfu_ilb], 0
        je      ioc_badbuf
        mov     eax, [cfg_dcb_ptr]
        test    eax, eax
        jz      ioc_badbuf
        push    ebx
        sub     esp, 16                 ; AEP_dcb_config
        mov     ebx, esp
        mov     word ptr [ebx+0], 3     ; AEP_func = AEP_CONFIG_DCB
        mov     word ptr [ebx+2], 0     ; AEP_result
        mov     eax, [cfu_ddb_ptr]
        mov     [ebx+4], eax            ; AEP_ddb
        mov     byte ptr [ebx+8], 0     ; AEP_lgn (IOS walks the layers)
        mov     byte ptr [ebx+9], 0
        mov     word ptr [ebx+10], 0
        mov     eax, [cfg_dcb_ptr]
        mov     [ebx+12], eax           ; AEP_d_c_dcb
        sub     esp, 8                  ; ISP_AEP_BROADCAST
        mov     word ptr [esp+0], 20    ; ISP_func = ISP_BROADCAST_AEP
        mov     word ptr [esp+2], 0     ; ISP_result
        mov     [esp+4], ebx            ; ISP_baep_paep
        push    esp
        call    dword ptr [cfu_ilb]
        add     esp, 4
        movzx   eax, word ptr [esp+2]   ; broadcast ISP_result
        shl     eax, 16
        mov     ax, word ptr [ebx+2]    ; AEP_result after the broadcast
        add     esp, 8 + 16
        pop     ebx
        jmp     ret_dword

; CFU_IOSVOL: act as our own TSD.  DISKTSD never engages a dynamically-
; registered port driver's DCB (and broadcasting CONFIG_DCB hangs a layer
; driver), so do the TSD's job ourselves: read the MBR in ring 0, create a
; logical volume DCB for partition 1, and ISP_ASSOCIATE_DCB - which per the
; DDK notifies IFSMGR and broadcasts DBT_DEVICEARRIVAL.
ioc_iosvol:
        call    vol_create
        jmp     ret_dword

; CFU_VOLUP: bring the whole volume up now (manual replug / diagnostics).
; out: DWORD (msc_mounted<<16) | (log_dcb_ptr != 0)
ioc_volup:
        push    esi
        call    CFU1_VolUp
        pop     esi
        movzx   eax, byte ptr [msc_mounted]
        shl     eax, 16
        cmp     dword ptr [log_dcb_ptr], 0
        je      @F
        or      eax, 1
@@:     jmp     ret_dword

; CFU_VOLDOWN: polite teardown.  in (optional): BYTE force.  out: DWORD
; 0 = volume down, 1 = vetoed (something has the volume open), 2 = no volume.
ioc_voldown:
        cmp     dword ptr [log_dcb_ptr], 0
        jne     @F
        mov     eax, 2
        jmp     ret_dword
@@:     xor     ecx, ecx                ; force flag
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      @F
        cmp     [esi.cbInBuffer], 1
        jb      @F
        mov     cl, [edx]
@@:     test    cl, cl
        jnz     ivd_force
        ; ISP_QUERY_REMOVE_DCB: let IFSMGR veto (open files)
        sub     esp, 12                 ; hdr + pdcb + flags
        mov     word ptr [esp+0], 12    ; ISP_QUERY_REMOVE_DCB
        mov     word ptr [esp+2], 0
        mov     eax, [log_dcb_ptr]
        mov     [esp+4], eax
        mov     dword ptr [esp+8], 0
        push    esp
        call    dword ptr [cfu_ilb]
        add     esp, 4
        movzx   eax, word ptr [esp+2]
        add     esp, 12
        test    eax, eax
        jz      ivd_force
        mov     eax, 1                  ; vetoed
        jmp     ret_dword
ivd_force:
        push    esi
        call    CFU1_VolDown
        pop     esi
        xor     eax, eax
        jmp     ret_dword

; CFU_VOLSTAT: out: mounted, log_dcb, part_start, part_len, drive index
ioc_volstat:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 20
        jb      ioc_badbuf
        movzx   eax, byte ptr [msc_mounted]
        mov     [edx+0], eax
        mov     eax, [log_dcb_ptr]
        mov     [edx+4], eax
        mov     eax, [part_start]
        mov     [edx+8], eax
        mov     eax, [part_len]
        mov     [edx+12], eax
        movzx   eax, byte ptr [vol_drive]
        mov     [edx+16], eax
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 20
@@:     xor     eax, eax
        ret

;----------------------------------------------------------------------------
; phys_create: build + register our own PHYSICAL disk DCB (ISP_CREATE_DCB +
; fill + ISP_INSERT_CALLDOWN).  Removes the dependence on IOS's one-shot
; AEP_DEVICE_INQUIRY: whether the disk was present at registration no longer
; matters.  Sets cfg_dcb_ptr.  CF on failure.  Preserves EBX/ESI/EDI.
;----------------------------------------------------------------------------
BeginProc phys_create
        cmp     dword ptr [cfu_ilb], 0
        je      pc_fail
        cmp     dword ptr [cfu_ddb_ptr], 0
        je      pc_fail
        push    ebx
        push    edi
        sub     esp, 12                 ; ISP_dcb_create
        mov     edi, esp
        mov     word ptr [edi+0], 1     ; ISP_CREATE_DCB
        mov     word ptr [edi+2], 0
        mov     word ptr [edi+4], SIZE DCB
        mov     dword ptr [edi+6], 0
        push    edi
        call    dword ptr [cfu_ilb]
        mov     ebx, [esp+4+6]          ; new DCB
        add     esp, 12 + 4
        test    ebx, ebx
        jz      pc_fail2
        ; fill: physical, removable, writeable disk with real geometry
        mov     [ebx+0], ebx            ; DCB_physical_dcb -> itself
        mov     dword ptr [ebx+1Ch], 10h        ; dmd_small_memory
        mov     dword ptr [ebx+20h], 8006h      ; PHYSICAL|WRITEABLE|REMOVABLE
        mov     byte ptr [ebx+3Ch], 9   ; apparent_blk_shift (512)
        mov     word ptr [ebx+3Eh], 4342h       ; DCB_sig
        mov     byte ptr [ebx+40h], 0   ; DCB_type_disk
        mov     byte ptr [ebx + DCB_unit_on_ctl], 0
        mov     dword ptr [ebx + DCB_max_xfer_len], 1000h
        mov     eax, [msc_blocks]
        mov     [ebx + DCB_actual_sector_cnt], eax
        mov     dword ptr [ebx + DCB_actual_sector_cnt + 4], 0
        mov     dword ptr [ebx + DCB_actual_blk_size], 200h
        mov     dword ptr [ebx + DCB_actual_head_cnt], 255
        mov     dword ptr [ebx + DCB_actual_spt], 63
        push    edx
        mov     eax, [msc_blocks]
        xor     edx, edx
        mov     ecx, 255*63
        div     ecx
        pop     edx
        mov     [ebx + DCB_actual_cyl_cnt], eax
        ; our request routine into its calldown
        sub     esp, 24                 ; ISP_calldown_insert
        mov     word ptr [esp+0], 5     ; ISP_INSERT_CALLDOWN
        mov     word ptr [esp+2], 0
        mov     [esp+4], ebx
        mov     dword ptr [esp+8], OFFSET32 CFU1_IosReq
        mov     eax, [cfu_ddb_ptr]
        mov     [esp+12], eax
        mov     word ptr [esp+16], 0
        mov     dword ptr [esp+18], 10h ; dmd_small_memory
        mov     al, [aep_lgn_save]
        mov     [esp+22], al
        push    esp
        call    dword ptr [cfu_ilb]
        add     esp, 4
        movzx   eax, word ptr [esp+2]   ; insert result
        add     esp, 24
        test    eax, eax
        jnz     pc_fail2
        mov     [cfg_dcb_ptr], ebx      ; adopted as our physical DCB
        pop     edi
        pop     ebx
        clc
        ret
pc_fail2:
        pop     edi
        pop     ebx
pc_fail:
        stc
        ret
EndProc phys_create

;----------------------------------------------------------------------------
; vol_create: read the MBR, create + fill the logical volume DCB, associate
; it with the first free drive letter from E: up.  EAX = (create ISP_result
; << 16) | associate ISP_result; 0FFFFFFFFh if preconditions fail.
; Preserves EBX/ESI/EDI.
;----------------------------------------------------------------------------
BeginProc vol_create
        cmp     dword ptr [cfu_ilb], 0
        je      vc_fail
        cmp     byte ptr [msc_mounted], 0
        je      vc_fail
        cmp     dword ptr [cfg_dcb_ptr], 0
        jne     vc_have_phys
        call    phys_create             ; IOS's one-shot inquiry was missed:
        jc      vc_fail                 ; make the physical DCB ourselves
vc_have_phys:
        cmp     dword ptr [log_dcb_ptr], 0
        jne     vc_fail                 ; volume already up
        cmp     dword ptr [msc_bsize], 512
        jne     vc_fail                 ; only 512-byte-sector media
        ; 1) MBR -> partition 1 start/length
        mov     [xbusy], 1
        push    esi
        push    edi
        xor     eax, eax                ; LBA 0
        mov     ecx, 1                  ; one sector
        mov     edi, OFFSET32 vol_secbuf
        call    msc_do_read
        pop     edi
        pop     esi
        mov     [xbusy], 0
        jc      vc_fail
        cmp     word ptr [vol_secbuf+510], 0AA55h
        jne     vc_fail
        mov     eax, dword ptr [vol_secbuf+1BEh+8]
        test    eax, eax
        jz      vc_fail                 ; no partition 1
        mov     [part_start], eax
        mov     eax, dword ptr [vol_secbuf+1BEh+12]
        test    eax, eax
        jz      vc_fail
        mov     [part_len], eax
        push    ebx
        push    edi
        ; refresh the physical DCB's actual geometry (stick may have changed)
        mov     ebx, [cfg_dcb_ptr]
        mov     eax, [msc_blocks]
        mov     [ebx + DCB_actual_sector_cnt], eax
        mov     dword ptr [ebx + DCB_actual_sector_cnt + 4], 0
        ; 2) create the logical DCB
        sub     esp, 12                 ; ISP_dcb_create
        mov     edi, esp
        mov     word ptr [edi+0], 1     ; ISP_func = ISP_CREATE_DCB
        mov     word ptr [edi+2], 0
        mov     word ptr [edi+4], SIZE DCB
        mov     dword ptr [edi+6], 0
        push    edi
        call    dword ptr [cfu_ilb]
        mov     ebx, [esp+4+6]          ; ISP_dcb_ptr
        movzx   edi, word ptr [esp+4+2] ; create ISP_result
        add     esp, 12 + 4
        shl     edi, 16                 ; result accumulator (create<<16)
        test    ebx, ebx
        jz      vc_out
        test    edi, edi                ; create failed? don't keep the ptr
        jnz     vc_out
        mov     [log_dcb_ptr], ebx
        ; 3) fill the logical DCB
        mov     eax, [cfg_dcb_ptr]
        mov     [ebx+0], eax            ; DCB_physical_dcb -> our physical
        mov     [eax+10h], ebx          ; phys DCB_next_logical_dcb -> us
        ; give the logical DCB its OWN calldown node (never alias the
        ; physical's: ISP_DCB_DESTROY frees the DCB's calldown nodes, and a
        ; shared chain leaves the physical pointing at freed memory)
        sub     esp, 24                 ; ISP_calldown_insert
        mov     word ptr [esp+0], 5     ; ISP_INSERT_CALLDOWN
        mov     word ptr [esp+2], 0
        mov     [esp+4], ebx            ; on the logical DCB
        mov     dword ptr [esp+8], OFFSET32 CFU1_IosReq
        mov     eax, [cfu_ddb_ptr]
        mov     [esp+12], eax
        mov     word ptr [esp+16], 0    ; expansion len
        mov     dword ptr [esp+18], 10h ; dmd_small_memory
        mov     al, [aep_lgn_save]
        mov     [esp+22], al
        push    esp
        call    dword ptr [cfu_ilb]
        add     esp, 4
        movzx   eax, word ptr [esp+2]
        add     esp, 24
        test    eax, eax
        jnz     vc_out                  ; no calldown -> don't associate
        mov     dword ptr [ebx+1Ch], 10h        ; dmd_small_memory
        mov     dword ptr [ebx+20h], 4006h      ; LOGICAL|WRITEABLE|REMOVABLE
        mov     eax, [part_start]
        mov     [ebx+28h], eax          ; DCB_Partition_Start
        mov     byte ptr [ebx+3Ch], 9   ; apparent_blk_shift
        mov     word ptr [ebx+3Eh], 4342h       ; DCB_sig
        mov     byte ptr [ebx+40h], 0   ; DCB_type_disk
        mov     dword ptr [ebx + DCB_max_xfer_len], 1000h  ; 4KB: keep PIO bursts short
        mov     eax, [part_len]
        mov     [ebx + DCB_actual_sector_cnt], eax
        mov     dword ptr [ebx + DCB_actual_sector_cnt + 4], 0
        mov     dword ptr [ebx + DCB_actual_blk_size], 200h
        mov     dword ptr [ebx + DCB_actual_head_cnt], 255
        mov     dword ptr [ebx + DCB_actual_spt], 63
        push    edx
        mov     eax, [part_len]
        xor     edx, edx
        mov     ecx, 255*63
        div     ecx
        pop     edx
        mov     [ebx + DCB_actual_cyl_cnt], eax
        ; 4) associate with the first free drive number from E: (4) up
        mov     byte ptr [vol_drive], 4
vc_assoc:
        sub     esp, 12                 ; ISP_dcb_associate
        mov     word ptr [esp+0], 6     ; ISP_func = ISP_ASSOCIATE_DCB
        mov     word ptr [esp+2], 0
        mov     [esp+4], ebx            ; ISP_d_a_dcb = logical DCB
        mov     al, [vol_drive]
        mov     [esp+8], al             ; ISP_d_a_drive
        mov     byte ptr [esp+9], 0     ; ISP_d_a_flags
        mov     word ptr [esp+10], 0
        push    esp
        call    dword ptr [cfu_ilb]
        add     esp, 4
        movzx   eax, word ptr [esp+2]   ; associate ISP_result
        add     esp, 12
        test    eax, eax
        jz      vc_assoc_ok
        inc     byte ptr [vol_drive]    ; letter taken? try the next one
        cmp     byte ptr [vol_drive], 10        ; give up past K:
        jb      vc_assoc
vc_assoc_ok:
        or      edi, eax
vc_out:
        mov     eax, edi                ; (create<<16) | associate
        pop     edi
        pop     ebx
        ret
vc_fail:
        mov     eax, 0FFFFFFFFh
        ret
EndProc vol_create

;----------------------------------------------------------------------------
; do_iosreg: one-shot IOS_Register.  Safe to call repeatedly.
;----------------------------------------------------------------------------
BeginProc do_iosreg
        cmp     dword ptr [ios_regresult], 0
        jne     dir_done                ; already registered
        push    esi
        push    OFFSET32 cfu_drp
        VxDCall IOS_Register
        add     esp, 4
        pop     esi
        movzx   eax, word ptr [cfu_drp.DRP_reg_result]
        mov     [ios_regresult], eax
dir_done:
        ret
EndProc do_iosreg

;----------------------------------------------------------------------------
; CFU1_VolUp (appy-time callback, C convention): make the drive letter
; exist.  Registers with IOS on first use (which mounts + claims + configs
; via the AER), or just re-mounts after a replug, then creates + associates
; the logical volume.  All guards are idempotent.
;----------------------------------------------------------------------------
BeginProc CFU1_VolUp
        push    ebx
        push    esi
        push    edi
        cmp     [enabled], 0
        je      vup_done
        cmp     dword ptr [ios_regresult], 0
        jne     vup_registered
        call    do_iosreg               ; AER mounts + claims during this
        jmp     vup_mounted
vup_registered:
        cmp     byte ptr [msc_mounted], 0
        jne     vup_mounted
        mov     [xbusy], 1
        call    msc_mount               ; replug: re-enumerate the stick
        mov     [xbusy], 0
vup_mounted:
        cmp     byte ptr [msc_mounted], 0
        je      vup_done
        cmp     dword ptr [log_dcb_ptr], 0
        jne     vup_done
        call    vol_create
vup_done:
        ; THE TEARDOWN BUG'S ROOT CAUSE: msc_mount stops the supervisor to
        ; own the chip, and nothing ever restarted it — so after every
        ; successful mount the detach detector existed but never ticked
        ; (E: lingered after a pull, a later mouse was never seen).
        ; Resume in disk-run state, NO bus reset: the mount must survive.
        cmp     [enabled], 0
        je      @F
        call    sup_resume_disk
@@:     mov     [vol_busy], 0
        pop     edi
        pop     esi
        pop     ebx
        ret
EndProc CFU1_VolUp

;----------------------------------------------------------------------------
; CFU1_VolDown (appy-time callback, C convention): destroy the logical
; volume DCB.  ISP_DCB_DESTROY blocks until pending I/O completes, notifies
; IFSMGR, broadcasts DBT_DEVICEREMOVECOMPLETE and frees the DCB.
;----------------------------------------------------------------------------
BeginProc CFU1_VolDown
        cmp     dword ptr [log_dcb_ptr], 0
        je      vdn_done
        cmp     dword ptr [cfu_ilb], 0
        je      vdn_done
        sub     esp, 8                  ; ISP_DCB_DESTROY: hdr + dcb
        mov     word ptr [esp+0], 10    ; ISP_DESTROY_DCB
        mov     word ptr [esp+2], 0
        mov     eax, [log_dcb_ptr]
        mov     [esp+4], eax
        push    esp
        call    dword ptr [cfu_ilb]
        add     esp, 4 + 8
        mov     dword ptr [log_dcb_ptr], 0
        mov     eax, [cfg_dcb_ptr]
        test    eax, eax
        jz      vdn_done
        mov     dword ptr [eax+10h], 0  ; unlink phys DCB_next_logical_dcb
vdn_done:
        mov     [vol_busy], 0
        ret
EndProc CFU1_VolDown

;----------------------------------------------------------------------------
; sched_volup / sched_voldown: queue the volume work at appy time (the ISP
; volume services are appy-time only; the supervisor tick is not).
; CAAFL_RING0 lets it run before the GUI is fully up during boot.
;----------------------------------------------------------------------------
BeginProc sched_volup
        cmp     [vol_busy], 0
        jnz     sv_done                 ; one op in flight is enough
        mov     [vol_busy], 1
        push    esi
        push    0                       ; ulTimeout
        push    1                       ; flags = CAAFL_RING0
        push    0                       ; dwRefData
        push    OFFSET32 CFU1_VolUp
        VxDCall _SHELL_CallAtAppyTime
        add     esp, 16
        pop     esi
        test    eax, eax
        jnz     sv_done
        mov     [vol_busy], 0           ; couldn't queue: retry next tick
sv_done:
        ret
EndProc sched_volup

BeginProc sched_voldown
        cmp     [vol_busy], 0
        jnz     sd_done
        mov     [vol_busy], 1
        push    esi
        push    0
        push    1                       ; CAAFL_RING0
        push    0
        push    OFFSET32 CFU1_VolDown
        VxDCall _SHELL_CallAtAppyTime
        add     esp, 16
        pop     esi
        test    eax, eax
        jnz     sd_done
        mov     [vol_busy], 0
sd_done:
        ret
EndProc sched_voldown

; CFU_IORTRACE: dump the request trace ring
ioc_iortrace:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 516  ; total + 32*16
        jb      ioc_badbuf
        mov     eax, [iortr_idx]
        mov     [edx+0], eax
        push    esi
        push    edi
        cld
        mov     esi, OFFSET32 iortr_ring
        lea     edi, [edx+4]
        mov     ecx, 128
        rep     movsd
        pop     edi
        pop     esi
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 516
@@:     xor     eax, eax
        ret

; req_wait_hold: spin (task time, bounded ~2s) while the IRQ test owns the chip
BeginProc req_wait_hold
        push    ecx
        mov     ecx, 2000000
rwh_l:  cmp     [irq_hold], 0
        jz      rwh_ok
        push    ecx
        mov     ecx, 1                  ; ~1us
        call    iodelay
        pop     ecx
        dec     ecx
        jnz     rwh_l
rwh_ok: pop     ecx
        ret
EndProc req_wait_hold

; CFU_IRQTEST: one-shot delivery test with exclusive chip ownership.
; The polled engine is held off (irq_hold + xbusy), the IRQ is hooked and
; steered, SOF interrupts (already running when a device is mounted) are
; counted for ~1s, then everything is parked and unhooked again.
ioc_irqtest:
        cmp     [enabled], 0
        je      ioc_notena
        cmp     [irqhandle], 0
        jnz     ioc_badbuf              ; already hooked (shouldn't happen)
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 1
        jb      ioc_badbuf
        mov     [irq_hold], 1           ; freeze new IORs out
        push    ecx
        mov     ecx, 50000              ; let any in-flight polled work drain
        call    iodelay
        pop     ecx
        cmp     [xbusy], 0
        jne     itq_busy
        mov     [xbusy], 1              ; chip is ours alone now
        movzx   ecx, byte ptr [edx]     ; IRQ number
        push    esi
        call    do_irqhook              ; hook + steer PCIC + unmask
        pop     esi
        jc      itq_fail
        push    ecx
        mov     ecx, 1000000            ; ~1s counting window
        call    iodelay
        pop     ecx
        push    esi
        call    ioc_unhook              ; VPICD default behaviour again
        pop     esi
        mov     al, 60h                 ; re-park the card IRQ at the PCIC
        mov     ah, 03h
        call    pcic_wr
        mov     [xbusy], 0
        mov     [irq_hold], 0
        mov     eax, [irqcount]
        jmp     ret_dword
itq_fail:
        mov     [xbusy], 0
itq_busy:
        mov     [irq_hold], 0
        mov     eax, 0FFFFFFFFh
        jmp     ret_dword

; CFU_ASYNCA: stage-A IRQ-completion mode on/off.  On: hook + steer the
; Windows-assigned IRQ but keep the PIC masked — do_xact unmasks only
; inside its wait windows.  Off: unhook and re-park the PCIC.
ioc_asynca:
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbInBuffer], 1
        jb      ioc_badbuf
        cmp     byte ptr [edx], 0
        jz      asa_off
        cmp     [enabled], 0
        je      ioc_notena
        cmp     [pnp_irq], 0FFh
        je      ioc_badbuf              ; no Windows-assigned IRQ
        cmp     [irqhandle], 0
        jnz     asa_on2                 ; already hooked (test tool etc.)
        movzx   ecx, [pnp_irq]
        push    esi
        call    do_irqhook              ; hook + steer PCIC + unmask
        pop     esi
        jc      ioc_badbuf
asa_on2:
        mov     eax, [irqhandle]        ; masked until a window opens
        VxDCall VPICD_Physically_Mask
        mov     dword ptr [awin_ok], 0
        mov     dword ptr [awin_tmo], 0
        mov     [amode], 1
        mov     eax, [irqcount]
        jmp     ret_dword
asa_off:
        mov     [amode], 0
        mov     [await], 0
        push    esi
        call    ioc_unhook
        pop     esi
        mov     al, 60h                 ; re-park the card IRQ at the PCIC
        mov     ah, 03h
        call    pcic_wr
        mov     eax, [irqcount]
        jmp     ret_dword

; CFU_ASYNCST: 7 DWORDs: irqcount, IRQ-completed windows, timeouts, amode,
; segment-end canary (BEEFCAFE = intact), failing BOT stage, SL811 status
ioc_asyncst:
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      ioc_badbuf
        cmp     [esi.cbOutBuffer], 28
        jb      ioc_badbuf
        mov     eax, [irqcount]
        mov     [edx+0], eax
        mov     eax, [awin_ok]
        mov     [edx+4], eax
        mov     eax, [awin_tmo]
        mov     [edx+8], eax
        movzx   eax, byte ptr [amode]
        mov     [edx+12], eax
        mov     eax, [acanary]
        mov     [edx+16], eax
        movzx   eax, byte ptr [bot_stage]
        mov     [edx+20], eax
        movzx   eax, byte ptr [bot_st]
        mov     [edx+24], eax
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      @F
        mov     dword ptr [ecx], 28
@@:     xor     eax, eax
        ret

; CFU_MSCMOUNT: enumerate + read capacity in ring 0; out: blocks, block_size
ioc_mscmount:
        cmp     [enabled], 0
        je      ioc_notena
        mov     [xbusy], 1
        push    esi                     ; msc_mount trashes ESI (DIOCParams)
        call    msc_mount
        pop     esi
        mov     [xbusy], 0
        mov     edx, [esi.lpvOutBuffer]
        test    edx, edx
        jz      mm2_done
        cmp     [esi.cbOutBuffer], 8
        jb      mm2_done
        mov     eax, [msc_blocks]
        mov     [edx], eax
        mov     eax, [msc_bsize]
        mov     [edx+4], eax
        mov     ecx, [esi.lpcbBytesReturned]
        test    ecx, ecx
        jz      mm2_done
        mov     dword ptr [ecx], 8
mm2_done:
        xor     eax, eax
        ret

ioc_notena:
        mov     eax, CFUERR_NOTENAB
        ret
ioc_badbuf:
        mov     eax, CFUERR_BUFFER
        ret

;----------------------------------------------------------------------------
; IRQ: virtualize, steer the PCIC, count SOF/any SL811 interrupts
;----------------------------------------------------------------------------
ioc_irqhook:
        cmp     [enabled], 0
        je      ioc_notena
        cmp     [irqhandle], 0
        jnz     ioc_badbuf              ; already hooked
        mov     edx, [esi.lpvInBuffer]
        test    edx, edx
        jz      ioc_badbuf
        movzx   ecx, byte ptr [edx]     ; irq number
        call    do_irqhook
        jc      ioc_badbuf
        xor     eax, eax
        ret

; do_irqhook: CL = IRQ.  Virtualize via VPICD, steer the PCIC, unmask.
; CF set on failure.  Idempotent (no-op if already hooked).
BeginProc do_irqhook
        cmp     [irqhandle], 0
        jnz     dih_ok
        mov     [vid.VID_IRQ_Number], cx
        mov     [vid.VID_Options], 0
        mov     [vid.VID_Hw_Int_Proc], OFFSET32 CFU1_HwInt
        mov     [vid.VID_Virt_Int_Proc], 0
        mov     [vid.VID_EOI_Proc], 0
        mov     [vid.VID_Mask_Change_Proc], 0
        mov     [vid.VID_IRET_Proc], 0
        mov     [vid.VID_IRET_Time_Out], 500
        push    ecx
        push    edi
        mov     edi, OFFSET32 vid
        VxDCall VPICD_Virtualize_IRQ
        pop     edi
        pop     ecx
        jc      dih_fail
        mov     [irqhandle], eax
        mov     [irqcount], 0

        ; steer the card IRQ at the PCIC (keep I/O mode + reset deasserted)
        mov     al, cl
        and     al, 0Fh
        or      al, 60h
        mov     ah, 03h
        call    pcic_wr

        mov     eax, [irqhandle]
        VxDCall VPICD_Physically_Unmask
dih_ok: clc
        ret
dih_fail:
        stc
        ret
EndProc do_irqhook

ioc_irqcount:
        mov     eax, [irqcount]
        jmp     ret_dword

ioc_irqunhook2:
        call    ioc_unhook
        xor     eax, eax
        ret

BeginProc ioc_unhook
        cmp     [irqhandle], 0
        jz      @F
        mov     eax, [irqhandle]
        VxDCall VPICD_Force_Default_Behavior
        mov     [irqhandle], 0
@@:     ret
EndProc ioc_unhook

;----------------------------------------------------------------------------
; ring-0 USB transfer engine.
; SL811 access at native port speed: address port = iobase, data = iobase+1.
;----------------------------------------------------------------------------
BeginProc sl_rd                         ; in: AL=reg  out: AL=val
        push    edx
        movzx   edx, [iobase]
        out     dx, al
        inc     edx
        in      al, dx
        pop     edx
        ret
EndProc sl_rd

BeginProc sl_wr                         ; in: AL=reg AH=val
        push    edx
        movzx   edx, [iobase]
        out     dx, al
        inc     edx
        mov     al, ah
        out     dx, al
        pop     edx
        ret
EndProc sl_wr

BeginProc buf_to_chip                   ; ESI=src, AL=chip addr, ECX=count
        push    edx
        movzx   edx, [iobase]
        out     dx, al                  ; set address (autoincrement follows)
        inc     edx
        jecxz   btc_done
btc_loop:
        lodsb
        out     dx, al
        loop    btc_loop
btc_done:
        pop     edx
        ret
EndProc buf_to_chip

BeginProc buf_from_chip                 ; EDI=dst, AL=chip addr, ECX=count
        push    edx
        movzx   edx, [iobase]
        out     dx, al
        inc     edx
        jecxz   bfc_done
bfc_loop:
        in      al, dx
        stosb
        loop    bfc_loop
bfc_done:
        pop     edx
        ret
EndProc buf_from_chip

;----------------------------------------------------------------------------
; do_xact: run one transaction on EP A using xa_* globals; the payload must
; already be in chip RAM at 0x10.  Returns AL = SL811 packet status, or
; carry set on completion timeout (~50ms).
;----------------------------------------------------------------------------
BeginProc do_xact
        push    ecx
        mov     al, 01h                 ; EP A buffer address
        mov     ah, 10h
        call    sl_wr
        mov     al, 02h                 ; EP A buffer length
        mov     ah, [xa_len]
        call    sl_wr
        mov     al, 03h                 ; PID | endpoint
        mov     ah, [xa_pidep]
        call    sl_wr
        mov     al, 04h                 ; device address
        mov     ah, [xa_addr]
        call    sl_wr
        mov     al, 0Dh                 ; clear interrupt status
        mov     ah, 0FFh
        call    sl_wr
        cmp     [amode], 0
        jnz     dx_async
dx_go:
        mov     al, 00h                 ; EP A host control: go
        mov     ah, [xa_ctl]
        call    sl_wr
        mov     ecx, [xa_budget]        ; completion budget (~us)
dx_poll:
        mov     al, 0Dh
        call    sl_rd
        test    al, 01h                 ; DONE_A
        jnz     dx_done
        in      al, 80h                 ; ~1us
        loop    dx_poll
        pop     ecx
        stc
        ret
dx_done:
        mov     al, 03h                 ; packet status
        call    sl_rd
        pop     ecx
        clc
        ret

        ; stage A: same synchronous shape, but the chip is untouched
        ; between GO and completion — the IRQ handler reads INTSTAT,
        ; captures packet status and acks; we spin on a memory flag.
        ; The PIC is only unmasked inside this window (one-master rule).
dx_async:
        cmp     [irqhandle], 0          ; hook gone (teardown): stay polled
        jz      dx_go
        mov     al, 06h                 ; INTENA: USB-A done (b0) + SOF gate
        mov     ah, 11h                 ; (b4) — b0 makes completion assert
        call    sl_wr                   ; the IRQ pin, else no interrupt
        mov     [aflag], 0
        mov     al, 00h                 ; EP A host control: go
        mov     ah, [xa_ctl]
        call    sl_wr
        mov     [await], 1              ; window open: HwInt owns the chip
        mov     eax, [irqhandle]
        VxDCall VPICD_Physically_Unmask
        mov     ecx, [xa_budget]
dxa_spin:
        cmp     [aflag], 0
        jnz     dxa_done
        in      al, 80h                 ; ~1us
        loop    dxa_spin
        mov     eax, [irqhandle]        ; timeout: close the window
        VxDCall VPICD_Physically_Mask
        mov     [await], 0
        cmp     [aflag], 0              ; IRQ landed while we closed?
        jnz     dxa_late
        inc     [awin_tmo]
        mov     al, 0Dh                 ; the chip is ours again: peek —
        call    sl_rd                   ; a lost edge leaves DONE latched
        test    al, 01h
        jz      dxa_hard
        mov     al, 03h                 ; rescued: status + ack + succeed
        call    sl_rd
        mov     [astat], al
        push    eax
        mov     al, 0Dh
        mov     ah, 0FFh
        call    sl_wr
        pop     eax
        pop     ecx
        clc
        ret
dxa_hard:
        pop     ecx
        stc
        ret
dxa_done:
        mov     eax, [irqhandle]
        VxDCall VPICD_Physically_Mask
        mov     [await], 0
dxa_late:
        inc     [awin_ok]
        mov     al, [astat]             ; packet status, via the handler
        pop     ecx
        clc
        ret
EndProc do_xact

;----------------------------------------------------------------------------
; do_xact_retry: do_xact, retrying while the device NAKs (busy), up to
; ~250 attempts with ~1ms spacing.  Same returns as do_xact.
;----------------------------------------------------------------------------
BeginProc do_xact_retry
        push    ebx
        push    ecx
        mov     ebx, [xr_tries]
dxr_try:
        call    do_xact
        jc      dxr_out                 ; timeout
        test    al, 40h                 ; NAK?
        jz      dxr_out
        mov     ecx, 1000               ; ~1ms then retry
dxr_dly:
        push    eax
        in      al, 80h
        pop     eax
        loop    dxr_dly
        dec     ebx
        jnz     dxr_try
        clc                             ; give up: return the NAK status
dxr_out:
        pop     ecx
        pop     ebx
        ret
EndProc do_xact_retry

;----------------------------------------------------------------------------
; do_control: internal control transfer used by the auto-mouse supervisor.
; Params in globals: ct_setup[8], ct_addr, ct_mps, ct_wlen (single IN packet,
; <= mps; enumeration never needs more).  IN data -> ct_buf, count -> ct_got.
; Returns CF clear on success, CF set on any stage failure.  Caller sets
; xr_tries to bound the event-time stall.
;----------------------------------------------------------------------------
BeginProc do_control
        mov     [xbusy], 1
        mov     al, [ct_addr]
        mov     [xa_addr], al
        mov     dword ptr [ct_got], 0

        ; SETUP: 8 bytes, DATA0, OUT
        push    esi
        mov     esi, OFFSET32 ct_setup
        mov     al, 10h
        mov     ecx, 8
        call    buf_to_chip
        pop     esi
        mov     [xa_pidep], 0D0h
        mov     [xa_len], 8
        mov     al, 07h
        or      al, [ct_pre]
        mov     [xa_ctl], al
        call    do_xact_retry
        jc      dc_fail
        test    al, 01h
        jz      dc_fail

        ; DATA IN stage: accumulate multi-packet into ct_buf (toggle
        ; DATA1/DATA0...), stop on wlen satisfied or a short packet.  The
        ; old single-packet read couldn't assemble descriptors bigger than
        ; bMaxPacketSize0, so 18-byte config reads failed on 8-mps devices.
        mov     ecx, [ct_wlen]
        test    ecx, ecx
        jnz     dc_din_chk
        jmp     dc_status
dc_din_chk:
        test    byte ptr [ct_setup], 80h
        jnz     dc_din_start
        jmp     dc_status               ; OUT data-stage: not needed here
dc_din_start:
        push    ebx
        push    edi
        mov     edi, OFFSET32 ct_buf
        mov     ebx, ecx                ; EBX = bytes still wanted
        mov     byte ptr [dc_tog], 43h  ; ARM|ENABLE|DATA1
dc_din:
        test    ebx, ebx
        jz      dc_din_ok
        movzx   eax, byte ptr [ct_mps]
        cmp     eax, ebx
        jbe     @F
        mov     eax, ebx
@@:     mov     [xa_len], al
        mov     [xa_pidep], 90h         ; IN ep0
        mov     al, [dc_tog]
        mov     [xa_ctl], al
        call    do_xact_retry
        jc      dc_din_err
        test    al, 01h
        jz      dc_din_err
        mov     al, 04h                 ; SL811 remaining-count
        call    sl_rd
        mov     ah, [xa_len]
        sub     ah, al                  ; AH = bytes got this packet
        movzx   ecx, ah
        add     [ct_got], ecx
        mov     al, 10h
        push    ecx
        call    buf_from_chip           ; EDI += got
        pop     ecx
        sub     ebx, ecx
        movzx   eax, byte ptr [ct_mps]
        cmp     ecx, eax
        jb      dc_din_ok               ; short packet -> transfer done
        xor     byte ptr [dc_tog], 40h  ; flip DATA toggle
        jmp     dc_din
dc_din_ok:
        pop     edi
        pop     ebx
        jmp     dc_status
dc_din_err:
        pop     edi
        pop     ebx
        jmp     dc_fail

dc_status:
        mov     [xa_len], 0
        test    byte ptr [ct_setup], 80h
        jz      dc_status_in
        mov     [xa_pidep], 10h         ; OUT status after IN data
        mov     al, 47h
        or      al, [ct_pre]
        mov     [xa_ctl], al           ; ARM|ENABLE|OUT|DATA1
        jmp     dc_st_go
dc_status_in:
        mov     [xa_pidep], 90h         ; IN status after no data
        mov     al, 43h
        or      al, [ct_pre]
        mov     [xa_ctl], al           ; ARM|ENABLE|DATA1
dc_st_go:
        call    do_xact_retry
        jc      dc_fail
        test    al, 01h
        jz      dc_fail
        mov     [xbusy], 0
        clc
        ret
dc_fail:
        mov     [xbusy], 0
        stc
        ret
EndProc do_control

;----------------------------------------------------------------------------
; ring-0 mass storage: Bulk-Only Transport + SCSI (Stage 1 of drive-letter).
; msc_bot runs one BOT command using the msc_* globals; the caller fills in
; msc_cdb/cdblen/dir/datalen/databuf and sets msc_addr/epin/epout first.
;----------------------------------------------------------------------------
VxD_LOCKED_DATA_SEG
msc_addr        db      1
msc_epin        db      81h
msc_epout       db      2
msc_togin       db      0
msc_togout      db      0
msc_cdblen      db      0
msc_dir         db      0               ; 1 = data IN, 0 = OUT/none
                db      1 dup (0)
msc_tag         dd      1
msc_datalen     dd      0
msc_databuf     dd      0               ; linear address of data buffer
msc_cdb         db      16 dup (0)
msc_cbw         db      31 dup (0)
msc_csw         db      16 dup (0)
msc_cap         db      8 dup (0)       ; READ CAPACITY result
msc_blocks      dd      0               ; total block count
msc_bsize       dd      512             ; bytes per block
msc_mounted     db      0
                db      3 dup (0)

; --- IOS registration (Stage 2 of drive-letter): DRP + ILB + AEP log ---
cfu_drp         DRP     <EyeCatcher, DRP_MISC_PD, OFFSET32 CFU1_IosAer, \
                         OFFSET32 cfu_ilb, <'CFU1 USB Stg    '>, 0, \
                         DRP_FC_DYNALOAD, 0>
cfu_ilb         ILB     <>
ios_regresult   dd      0
aep_total       dd      0
aep_lastfunc    dd      0
cfu_ddb_ptr     dd      0               ; DDB IOS allocated for us
inq_claimed     db      0               ; have we claimed our one device?
                db      3 dup (0)
aep_counts      dd      32 dup (0)
ior_total       dd      0               ; requests seen by CFU1_IosReq
ior_last        dd      0               ; last IOR_func seen
isp_cd_result   dd      0FFFFFFFFh      ; ISP_result of the calldown insert
cfg_dcb_ptr     dd      0               ; DCB from the last AEP_CONFIG_DCB
inq_dcb_ptr     dd      0               ; DCB from the last AEP_DEVICE_INQUIRY
cd_inserted_dcb dd      0               ; DCB our calldown is already on
log_dcb_ptr     dd      0               ; logical volume DCB we created
part_start      dd      0               ; first partition start LBA (from MBR)
part_len        dd      0               ; first partition length in sectors
vol_drive       db      4               ; drive index the volume landed on (E:)
aep_lgn_save    db      0               ; our load group number (from CONFIG)
vol_busy        db      0               ; an appy-time volume op is queued
irq_hold        db      0               ; IRQ test owns the chip: req paths wait
sup_class       db      0               ; classified device class (03/08/09)
sup_retry       db      0               ; ticks until a mount retry
hd2_addr        db      0               ; poll slot B (second HID device)
hd2_ep          db      0
hd2_mps         db      0
hd2_mode        db      0               ; 1 = mouse (VMD post only)
hd2_tog         db      0
hd2_act         db      0               ; slot B armed
hub_nports      db      0               ; hub descent state
hub_port        db      0               ; port being scanned (1-based)
hub_naddr       db      2               ; next downstream USB address
hd2_buf         db      8 dup (0)
ct_pre          db      0               ; 80h = send PRE (LS behind FS hub)
hid_pre         db      0               ; per-slot preamble bits
hd2_pre         db      0
dev_ls          db      0               ; last-enumerated device is LS
sup_armmode     db      1               ; hid_mode to arm (1=mouse 2=kbd)
dc_tog          db      0               ; do_control DATA-IN toggle scratch
                db      1 dup (0)
hub_mps         db      8               ; the HUB's own ep0 mps (sup_mps gets
                                        ; overwritten by downstream enum)
hub_conn        db      0               ; ports showing CONNECTION this pass
hub_rescan      db      0               ; rescan passes taken (diag)
hub_pwrfail     db      0               ; failed PORT_POWER transfers (diag)
hub_p2pg        db      0               ; bPwrOn2PwrGood, 2ms units (diag)
hub_wait        db      0               ; ticks in state 13 until a rescan
hub_estage      db      0               ; how far the last port enum got (diag)
hid_port        db      0               ; hub port bound to the primary slot
hd2_port        db      0               ; hub port bound to slot B
hub_parked      db      0               ; bitmask: enumerated but unslottable
hub_dead        db      0               ; consecutive failed port GET_STATUSes
hub_slow        db      0               ; tick divider for the port scan
hub_pstat       dd      7 dup (0)       ; last GET_STATUS(port), 1 dword/port
                                        ; (FFFFFFFF = xfer fail, FFFFFFFE short)
vol_secbuf      db      512 dup (0)     ; MBR parse buffer
ior_counts      dd      48 dup (0)      ; per-IOR_func counters (0..2Fh)
amode           db      0               ; stage A: xacts complete via IRQ
await           db      0               ; window open: HwInt owns the chip
aflag           db      0               ; HwInt signals completion here
astat           db      0               ; packet status captured by HwInt
awin_ok         dd      0               ; windows completed by IRQ (diag)
awin_tmo        dd      0               ; windows that timed out (diag)
bot_stage       db      0               ; failing BOT phase: 1=CBW 2=data-out
                                        ; 3=data-in 4=CSW (0 = no failure)
bot_st          db      0               ; SL811 status there (FF = timeout)
sup_busyct      db      0               ; consecutive xbusy-blocked ticks
                db      1 dup (0)
iortr_idx       dd      0               ; trace ring write index
iortr_ring      dd      128 dup (0)     ; 32 x (func, flags, lba, count)
acanary         dd      0BEEFCAFEh      ; guards the segment end: something
                                        ; zeroed vars placed here once
CFU_DDB_SIZE    equ     32              ; = size DDB (header only)
VxD_LOCKED_DATA_ENDS

;----------------------------------------------------------------------------
; CFU1_IosAer: IOS asynchronous event routine.  Called by IOS as aer(AEP*).
; Increment 2b: record which events arrive and reject them (AEP_FAILURE),
; so we observe the linkage without claiming any device yet.
;----------------------------------------------------------------------------
BeginProc CFU1_IosAer
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        push    edi
        mov     ebx, [ebp+8]            ; AEP pointer
        movzx   eax, word ptr [ebx]     ; AEP_func (offset 0)
        inc     dword ptr [aep_total]
        mov     [aep_lastfunc], eax
        cmp     eax, 32
        jae     @F
        inc     dword ptr [aep_counts + eax*4]
@@:
        movzx   eax, word ptr [ebx]
        cmp     eax, 0                  ; AEP_INITIALIZE
        je      aer_init
        cmp     eax, 6                  ; AEP_DEVICE_INQUIRY
        je      aer_inq
        cmp     eax, 2                  ; AEP_BOOT_COMPLETE
        je      aer_ok
        cmp     eax, 3                  ; AEP_CONFIG_DCB
        je      aer_cfg
        cmp     eax, 16                 ; AEP_DCB_LOCK / MOUNT_NOTIFY /
        jb      @F                      ; CREATE_VRP arrive while a volume
        cmp     eax, 19                 ; on our DCB mounts - acknowledge
        jbe     aer_ok                  ; them (16..19 incl DESTROY_VRP)
@@:
        mov     word ptr [ebx+2], 0FFFFh ; default: AEP_FAILURE
        jmp     aer_out
aer_init:
        mov     al, [ebx+8]             ; AEPHDR.AEP_lgn - needed for our
        mov     [aep_lgn_save], al      ; own calldown inserts later
        call    ios_create_ddb
        jc      aer_fail
        push    ebx                     ; msc_mount trashes EBX (AEP ptr)
        call    msc_mount               ; bring the disk up now (sets msc_mounted)
        pop     ebx
        mov     word ptr [ebx+2], 0     ; AEP_SUCCESS (stay resident either way)
        jmp     aer_out
aer_inq:
        mov     esi, [ebx+12]           ; AEP_i_d_dcb
        mov     [inq_dcb_ptr], esi
        cmp     byte ptr [msc_mounted], 0
        je      aer_inq_none            ; no disk -> no device
        movzx   eax, byte ptr [esi + DCB_unit_on_ctl]
        test    eax, eax
        jnz     aer_inq_none            ; we only own unit 0
        mov     word ptr [ebx+2], 0     ; AEP_SUCCESS: claim unit 0
        jmp     aer_out
aer_inq_none:
        mov     word ptr [ebx+2], 2     ; AEP_NO_MORE_DEVICES
        jmp     aer_out
aer_cfg:
        mov     al, [ebx+8]             ; remember our load group number -
        mov     [aep_lgn_save], al      ; needed to insert calldowns later
        call    ios_config_dcb          ; fill geometry + insert request routine
        mov     word ptr [ebx+2], 0     ; AEP_SUCCESS
        jmp     aer_out
aer_ok:
        mov     word ptr [ebx+2], 0     ; AEP_SUCCESS
        jmp     aer_out
aer_fail:
        mov     word ptr [ebx+2], 0FFFFh
aer_out:
        pop     edi
        pop     esi
        pop     ebx
        pop     ebp
        ret
EndProc CFU1_IosAer

; ios_create_ddb: ask IOS to allocate our adapter DDB.  CF set on failure.
BeginProc ios_create_ddb
        sub     esp, 12                 ; ISP_ddb_create on the stack
        mov     edi, esp
        mov     word ptr [edi+0], 0     ; ISP_func = ISP_CREATE_DDB (0)
        mov     word ptr [edi+2], 0     ; ISP_result
        mov     word ptr [edi+4], CFU_DDB_SIZE
        mov     dword ptr [edi+6], 0    ; ISP_ddb_ptr (out)
        mov     byte ptr [edi+10], 0    ; ISP_ddb_flags
        push    edi
        call    dword ptr [cfu_ilb]     ; ILB_service_rtn (offset 0)
        mov     eax, [edi+6]            ; ISP_ddb_ptr
        add     esp, 12 + 4             ; ISP + pushed arg
        mov     [cfu_ddb_ptr], eax
        test    eax, eax
        jz      icd_fail
        clc
        ret
icd_fail:
        stc
        ret
EndProc ios_create_ddb

;----------------------------------------------------------------------------
; ios_config_dcb: EBX = AEP_dcb_config.  Fill the DCB IOS handed us with our
; disk geometry (from the ring-0 mount) and splice our request routine into
; the DCB calldown stack so IOS routes read/write IORs to us.
;----------------------------------------------------------------------------
BeginProc ios_config_dcb
        push    esi
        push    edi
        mov     esi, [ebx+12]           ; AEP_d_c_dcb = the DCB (offset 12)
        mov     [cfg_dcb_ptr], esi
        test    esi, esi
        jz      icfg_done
        cmp     esi, [cd_inserted_dcb]  ; re-broadcasts loop back to us -
        je      icfg_done               ; never insert the calldown twice
        ; Fill the ACTUAL transfer/geometry fields (top-level DCB, not the
        ; apparent DCB_disk_geometry ones - those are the TSD's to compute).
        ; With these zero the disk TSD sees a 0-length device and declines
        ; the DCB, so no partition scan and no drive letter.
        mov     dword ptr [esi + DCB_max_xfer_len], 1000h  ; 4KB: keep PIO bursts short
        mov     eax, [msc_blocks]
        mov     [esi + DCB_actual_sector_cnt], eax
        mov     dword ptr [esi + DCB_actual_sector_cnt + 4], 0
        mov     eax, [msc_bsize]
        mov     [esi + DCB_actual_blk_size], eax
        mov     dword ptr [esi + DCB_actual_head_cnt], 255
        mov     dword ptr [esi + DCB_actual_spt], 63
        mov     eax, [msc_blocks]       ; cyls = blocks / (255*63)
        xor     edx, edx
        mov     ecx, 255*63
        div     ecx
        mov     [esi + DCB_actual_cyl_cnt], eax
        or      dword ptr [esi+20h], 6  ; DCB_DEV_WRITEABLE | REMOVABLE
                                        ; (writeable else VFAT mounts the
                                        ; volume write-protected; removable
                                        ; so VFAT writes through, making a
                                        ; surprise unplug survivable)
        sub     esp, 24                 ; ISP_calldown_insert on the stack
        mov     edi, esp
        mov     word ptr [edi+0], 5     ; ISP_func = ISP_INSERT_CALLDOWN
        mov     word ptr [edi+2], 0                     ; ISP_result
        mov     [edi+4], esi                            ; ISP_i_cd_dcb
        mov     dword ptr [edi+8], OFFSET32 CFU1_IosReq ; ISP_i_cd_req
        mov     eax, [ebx+4]                            ; AEP_ddb
        mov     [edi+12], eax                           ; ISP_i_cd_ddb
        mov     word ptr [edi+16], 0                    ; ISP_i_cd_expan_len
        mov     dword ptr [edi+18], 10h                 ; ISP_i_cd_flags = small_memory
        mov     al, [ebx+8]                             ; AEP_lgn
        mov     [edi+22], al                            ; ISP_i_cd_lgn
        push    edi
        call    dword ptr [cfu_ilb]     ; ILB_service_rtn
        movzx   eax, word ptr [esp+4+2] ; ISP_result (ISP sits above pushed arg)
        mov     [isp_cd_result], eax
        add     esp, 24 + 4
        test    eax, eax
        jnz     icfg_done
        mov     [cd_inserted_dcb], esi  ; insert succeeded - remember the DCB
icfg_done:
        pop     edi
        pop     esi
        ret
EndProc ios_config_dcb

;----------------------------------------------------------------------------
; CFU1_IosReq: DCB calldown request routine.  Called by IOS as req(IOP*).
; Services read/write/verify synchronously through the ring-0 BOT engine,
; then unwinds one callback level to complete the IOP.
;----------------------------------------------------------------------------
BeginProc CFU1_IosReq
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        push    edi
        mov     ebx, [ebp+8]            ; IOP pointer
        movzx   eax, word ptr [ebx + IOP_ior + IOR_func]
        inc     dword ptr [ior_total]
        mov     [ior_last], eax
        cmp     eax, 48
        jae     @F
        inc     dword ptr [ior_counts + eax*4]
@@:
        mov     ecx, [iortr_idx]        ; trace ring: func, flags, lba, cnt
        and     ecx, 31
        shl     ecx, 4
        mov     [iortr_ring + ecx], eax
        mov     eax, [ebx + IOP_ior + IOR_flags]
        mov     [iortr_ring + ecx + 4], eax
        mov     eax, [ebx + IOP_ior + IOR_start_addr]
        mov     [iortr_ring + ecx + 8], eax
        mov     eax, [ebx + IOP_ior + IOR_xfer_count]
        mov     [iortr_ring + ecx + 12], eax
        inc     dword ptr [iortr_idx]
        movzx   eax, word ptr [ebx + IOP_ior + IOR_func]
        cmp     eax, 0                  ; IOR_READ
        je      req_read
        cmp     eax, 1                  ; IOR_WRITE
        je      req_write
        cmp     eax, 2                  ; IOR_VERIFY
        je      req_ok                  ; nothing to verify -> succeed
        cmp     eax, 5                  ; IOR_MEDIA_CHECK /
        je      req_media               ; IOR_MEDIA_CHECK_RESET
        cmp     eax, 6
        je      req_media
        cmp     eax, 7                  ; LOAD_MEDIA / LOCK / UNLOCK:
        je      req_ok                  ; benign no-ops for a stick
        cmp     eax, 9
        je      req_ok
        cmp     eax, 0Ah
        je      req_ok
        cmp     eax, 10h                ; CLEAR_QUEUE / DOS_RESET: no queue,
        je      req_ok                  ; no state - succeed
        cmp     eax, 11h
        je      req_ok
        cmp     eax, 22h                ; SET_WRITE_STATUS: stays writeable
        je      req_ok
        cmp     eax, 27h                ; FLUSH_DRIVE(+DISCARD): we have no
        je      req_ok                  ; internal cache - already flushed
        cmp     eax, 28h
        je      req_ok
        mov     word ptr [ebx + IOP_ior + IOR_status], 16h  ; IORS_INVALID_COMMAND
        jmp     req_done
req_media:
        cmp     byte ptr [msc_mounted], 0
        je      req_nomedia             ; media gone -> tell the truth
        jmp     req_ok                  ; present, not changed
req_read:
        call    req_wait_hold           ; IRQ test window: hold off briefly
        cmp     byte ptr [msc_mounted], 0
        je      req_nomedia
        mov     [xbusy], 1
        mov     eax, [ebx + IOP_ior + IOR_start_addr]
        test    dword ptr [ebx + IOP_ior + IOR_flags], 80000h
        jz      @F                      ; volume-relative -> physical
        add     eax, [part_start]
@@:     mov     [sg_lba], eax
        test    dword ptr [ebx + IOP_ior + IOR_flags], 2
        jnz     rr_sg                   ; IORF_SCATTER_GATHER
        mov     ecx, [ebx + IOP_ior + IOR_xfer_count]
        mov     edi, [ebx + IOP_ior + IOR_buffer_ptr]
        push    ebx
        call    msc_do_read
        pop     ebx
        mov     [xbusy], 0
        jc      req_ioerr
        jmp     req_ok
rr_sg:  mov     eax, [ebx + IOP_ior + IOR_xfer_count]
        mov     [sg_left], eax          ; authoritative bound - the SGD
        mov     eax, [ebx + IOP_ior + IOR_buffer_ptr]
        mov     [sg_cur], eax           ; -> {sectors, linear ptr} pairs,
                                        ; null-terminated (trace-proven)           ; -> null-terminated SGD list
        push    ebx
rr_sgl: cmp     dword ptr [sg_left], 0
        je      rr_sgok
        mov     edx, [sg_cur]
        mov     edi, [edx+4]            ; logical SGD: ptr is SECOND
        test    edi, edi
        jz      rr_sgok
        mov     ecx, [edx]              ; first dword = SECTOR count
        test    ecx, ecx
        jz      rr_sgok
        cmp     ecx, [sg_left]
        jbe     @F
        mov     ecx, [sg_left]          ; clamp final segment
@@:     mov     [sg_step], ecx
        mov     eax, [sg_lba]
        call    msc_do_read
        jc      rr_sgbad
        mov     eax, [sg_step]
        add     [sg_lba], eax
        sub     [sg_left], eax
rr_sgnext:
        add     dword ptr [sg_cur], 8
        jmp     rr_sgl
rr_sgok:
        pop     ebx
        mov     [xbusy], 0
        jmp     req_ok
rr_sgbad:
        pop     ebx
        mov     [xbusy], 0
        jmp     req_ioerr
req_write:
        call    req_wait_hold           ; IRQ test window: hold off briefly
        cmp     byte ptr [msc_mounted], 0
        je      req_nomedia
        mov     [xbusy], 1
        mov     eax, [ebx + IOP_ior + IOR_start_addr]
        test    dword ptr [ebx + IOP_ior + IOR_flags], 80000h
        jz      @F
        add     eax, [part_start]
@@:     mov     [sg_lba], eax
        test    dword ptr [ebx + IOP_ior + IOR_flags], 2
        jnz     rw_sg                   ; IORF_SCATTER_GATHER
        mov     ecx, [ebx + IOP_ior + IOR_xfer_count]
        mov     esi, [ebx + IOP_ior + IOR_buffer_ptr]
        push    ebx
        call    msc_do_write
        pop     ebx
        mov     [xbusy], 0
        jc      req_ioerr
        jmp     req_ok
rw_sg:  mov     eax, [ebx + IOP_ior + IOR_xfer_count]
        mov     [sg_left], eax          ; authoritative bound - the SGD
        mov     eax, [ebx + IOP_ior + IOR_buffer_ptr]
        mov     [sg_cur], eax           ; -> {sectors, linear ptr} pairs,
                                        ; null-terminated (trace-proven)
        ; evidence: log the first SGD {ptr,size} into the trace ring as a
        ; pseudo-entry func=0FEh (ptr value alone tells linear vs physical)
        mov     ecx, [iortr_idx]
        and     ecx, 31
        shl     ecx, 4
        mov     dword ptr [iortr_ring + ecx], 0FEh
        mov     edx, [eax]              ; SG_buff_ptr
        mov     [iortr_ring + ecx + 4], edx
        mov     edx, [eax+4]            ; SG_buff_size
        mov     [iortr_ring + ecx + 8], edx
        mov     edx, [ebx + IOP_ior + IOR_xfer_count]
        mov     [iortr_ring + ecx + 12], edx
        inc     dword ptr [iortr_idx]
        mov     ecx, [iortr_idx]        ; second pseudo-entry: dwords 2,3
        and     ecx, 31
        shl     ecx, 4
        mov     dword ptr [iortr_ring + ecx], 0FDh
        mov     edx, [eax+8]
        mov     [iortr_ring + ecx + 4], edx
        mov     edx, [eax+12]
        mov     [iortr_ring + ecx + 8], edx
        mov     dword ptr [iortr_ring + ecx + 12], 0
        inc     dword ptr [iortr_idx]
        push    ebx
rw_sgl: cmp     dword ptr [sg_left], 0
        je      rw_sgok
        mov     edx, [sg_cur]
        mov     esi, [edx+4]            ; logical SGD: ptr is SECOND
        test    esi, esi
        jz      rw_sgok
        mov     ecx, [edx]              ; first dword = SECTOR count
        test    ecx, ecx
        jz      rw_sgok
        cmp     ecx, [sg_left]
        jbe     @F
        mov     ecx, [sg_left]          ; clamp final segment
@@:     mov     [sg_step], ecx
        mov     eax, [sg_lba]
        call    msc_do_write
        jc      rw_sgbad
        mov     eax, [sg_step]
        add     [sg_lba], eax
        sub     [sg_left], eax
rw_sgnext:
        add     dword ptr [sg_cur], 8
        jmp     rw_sgl
rw_sgok:
        pop     ebx
        mov     [xbusy], 0
        jmp     req_ok
rw_sgbad:
        pop     ebx
        mov     [xbusy], 0
        jmp     req_ioerr
req_ok:
        mov     word ptr [ebx + IOP_ior + IOR_status], 0     ; IORS_SUCCESS
        jmp     req_done
req_ioerr:
        mov     word ptr [ebx + IOP_ior + IOR_status], 15h   ; IORS_DEVICE_ERROR
        jmp     req_done
req_nomedia:
        mov     word ptr [ebx + IOP_ior + IOR_status], 18h   ; IORS_NO_MEDIA
req_done:
        mov     eax, [ebx + IOP_callback_ptr]
        sub     eax, 8                  ; SIZE IOP_callback_entry
        mov     [ebx + IOP_callback_ptr], eax
        push    ebx                     ; IOP offset
        call    dword ptr [eax]         ; IOP_CB_address
        add     esp, 4
        pop     edi
        pop     esi
        pop     ebx
        pop     ebp
        ret
EndProc CFU1_IosReq

;----------------------------------------------------------------------------
; msc_do_read / msc_do_write: EAX=LBA, ECX=sector count, EDI(read)/ESI(write)=
; linear buffer.  Build a SCSI READ(10)/WRITE(10) and run it via msc_bot.
; CF set on transport or CSW failure.
;----------------------------------------------------------------------------
BeginProc msc_do_read
        mov     [rw_lba], eax
        mov     [rw_count], ecx
        mov     [rw_buf], edi
        mov     byte ptr [rw_wr], 0
        call    msc_rw
        ret
EndProc msc_do_read

BeginProc msc_do_write
        mov     [rw_lba], eax
        mov     [rw_count], ecx
        mov     [rw_buf], esi
        mov     byte ptr [rw_wr], 1
        call    msc_rw
        ret
EndProc msc_do_write

BeginProc msc_rw
        push    edi                     ; zero the 16-byte CDB
        cld
        xor     eax, eax
        mov     edi, OFFSET32 msc_cdb
        mov     ecx, 4
        rep     stosd
        pop     edi
        mov     al, 28h                 ; READ(10)
        cmp     byte ptr [rw_wr], 0
        je      @F
        mov     al, 2Ah                 ; WRITE(10)
@@:     mov     [msc_cdb+0], al
        mov     eax, [rw_lba]           ; LBA big-endian into cdb[2..5]
        mov     [msc_cdb+5], al
        shr     eax, 8
        mov     [msc_cdb+4], al
        shr     eax, 8
        mov     [msc_cdb+3], al
        shr     eax, 8
        mov     [msc_cdb+2], al
        mov     eax, [rw_count]         ; block count big-endian into cdb[7..8]
        mov     [msc_cdb+8], al
        mov     [msc_cdb+7], ah
        mov     eax, [rw_count]         ; datalen = count * bsize
        mul     dword ptr [msc_bsize]
        mov     [msc_datalen], eax
        mov     byte ptr [msc_cdblen], 10
        mov     al, 1                   ; dir: read = IN
        cmp     byte ptr [rw_wr], 0
        je      @F
        xor     al, al                  ; write = OUT
@@:     mov     [msc_dir], al
        mov     eax, [rw_buf]
        mov     [msc_databuf], eax
        call    msc_bot
        jc      rw_err
        test    al, al                  ; CSW status
        jnz     rw_err
        clc
        ret
rw_err: stc
        ret
EndProc msc_rw

sg_lba          dd      0               ; SG walk: current LBA
sg_left         dd      0               ; SG walk: sectors remaining
sg_step         dd      0               ; SG walk: sectors this segment
sg_cur          dd      0               ; SG walk: current SGD entry
rw_lba          dd      0
rw_count        dd      0
rw_buf          dd      0
rw_wr           db      0
                db      3 dup (0)

; msc_bulk_out: ESI = linear src, ECX = length.  CF set on failure.
BeginProc msc_bulk_out
        cld
        mov     [xr_tries], 500
mbo_l:
        test    ecx, ecx
        jz      mbo_ok
        mov     ebx, ecx
        cmp     ebx, 64
        jbe     @F
        mov     ebx, 64
@@:     push    ecx
        push    esi
        mov     ecx, ebx
        mov     al, 10h
        call    buf_to_chip
        pop     esi
        pop     ecx
        mov     al, [msc_epout]
        and     al, 0Fh
        or      al, 10h                 ; OUT PID
        mov     [xa_pidep], al
        mov     al, [msc_addr]
        mov     [xa_addr], al
        mov     al, bl
        mov     [xa_len], al
        mov     al, 07h                 ; ARM|ENABLE|OUT
        test    byte ptr [msc_togout], 1
        jz      @F
        or      al, 40h
@@:     mov     [xa_ctl], al
        call    do_xact_retry
        jc      mbo_tmo
        test    al, 01h
        jz      mbo_err
        xor     byte ptr [msc_togout], 1
        add     esi, ebx
        sub     ecx, ebx
        jmp     mbo_l
mbo_ok: clc
        ret
mbo_tmo:
        mov     al, 0FFh                ; xact timeout (no status byte)
mbo_err:
        mov     [bot_st], al            ; SL811 status at the failure
        stc
        ret
EndProc msc_bulk_out

; msc_bulk_in: EDI = linear dst, ECX = length.  Stops on a short packet.
; CF set on failure.
BeginProc msc_bulk_in
        cld
        mov     [xr_tries], 500
mbi_l:
        test    ecx, ecx
        jz      mbi_ok
        mov     ebx, ecx
        cmp     ebx, 64
        jbe     @F
        mov     ebx, 64
@@:     mov     al, [msc_epin]
        and     al, 0Fh
        or      al, 90h                 ; IN PID
        mov     [xa_pidep], al
        mov     al, [msc_addr]
        mov     [xa_addr], al
        mov     al, bl
        mov     [xa_len], al
        mov     al, 03h                 ; ARM|ENABLE
        test    byte ptr [msc_togin], 1
        jz      @F
        or      al, 40h
@@:     mov     [xa_ctl], al
        call    do_xact_retry
        jc      mbi_tmo
        test    al, 01h
        jz      mbi_err
        xor     byte ptr [msc_togin], 1
        mov     al, 04h                 ; SL811 remaining-count register
        call    sl_rd
        cmp     al, bl                  ; remaining > requested = garbage
        ja      mbi_badcnt              ; register read: NEVER walk on it —
                                        ; got would wrap huge and
                                        ; buf_from_chip then marches through
                                        ; all of memory (the canary killer)
        mov     ah, bl                  ; AH = requested chunk
        sub     ah, al                  ; AH = got = requested - remaining
        movzx   edx, ah
        push    ecx
        mov     ecx, edx
        mov     al, 10h
        call    buf_from_chip           ; EDI += got
        pop     ecx
        sub     ecx, edx                ; remaining -= got
        cmp     edx, 64
        jb      mbi_ok                  ; short packet ends the transfer
        jmp     mbi_l
mbi_ok: clc
        ret
mbi_badcnt:
        mov     al, 0FEh                ; garbage count-register read
        jmp     mbi_err
mbi_tmo:
        mov     al, 0FFh                ; xact timeout (no status byte)
mbi_err:
        mov     [bot_st], al            ; SL811 status at the failure
        stc
        ret
EndProc msc_bulk_in

; msc_bot: one Bulk-Only Transport command.  AL = CSW status (0 = good) on
; success; CF set on transport failure.
BeginProc msc_bot
        cld
        mov     byte ptr [msc_cbw+0], 'U'
        mov     byte ptr [msc_cbw+1], 'S'
        mov     byte ptr [msc_cbw+2], 'B'
        mov     byte ptr [msc_cbw+3], 'C'
        mov     eax, [msc_tag]
        mov     dword ptr [msc_cbw+4], eax
        inc     dword ptr [msc_tag]
        mov     eax, [msc_datalen]
        mov     dword ptr [msc_cbw+8], eax
        mov     al, [msc_dir]
        test    al, al
        jz      @F
        mov     al, 80h
@@:     mov     [msc_cbw+12], al
        mov     byte ptr [msc_cbw+13], 0
        mov     al, [msc_cdblen]
        mov     [msc_cbw+14], al
        push    esi
        push    edi
        mov     esi, OFFSET32 msc_cdb
        mov     edi, OFFSET32 msc_cbw
        add     edi, 15
        mov     ecx, 16
        rep     movsb
        pop     edi
        pop     esi

        mov     byte ptr [bot_stage], 1 ; CBW out
        mov     esi, OFFSET32 msc_cbw
        mov     ecx, 31
        call    msc_bulk_out
        jc      mbt_fail

        mov     ecx, [msc_datalen]      ; data phase
        test    ecx, ecx
        jz      mbt_csw
        cmp     byte ptr [msc_dir], 0
        je      mbt_dout
        mov     byte ptr [bot_stage], 3 ; data in
        mov     edi, [msc_databuf]
        mov     ecx, [msc_datalen]
        call    msc_bulk_in
        jmp     mbt_csw
mbt_dout:
        mov     byte ptr [bot_stage], 2 ; data out
        mov     esi, [msc_databuf]
        mov     ecx, [msc_datalen]
        call    msc_bulk_out

mbt_csw:
        mov     byte ptr [bot_stage], 4 ; CSW in
        mov     edi, OFFSET32 msc_csw
        mov     ecx, 13
        call    msc_bulk_in
        jc      mbt_fail
        cmp     byte ptr [msc_csw+0], 'U'
        jne     mbt_fail
        cmp     byte ptr [msc_csw+3], 'S'
        jne     mbt_fail
        mov     al, [msc_csw+12]
        mov     byte ptr [bot_stage], 0 ; clean run
        clc
        ret
mbt_fail:
        stc
        ret
EndProc msc_bot

;----------------------------------------------------------------------------
; do_usbinit_fs: bus reset + full-speed host init in ring 0 (task time).
;----------------------------------------------------------------------------
BeginProc do_usbinit_fs
        mov     al, 06h                 ; INTENA off
        mov     ah, 00h
        call    sl_wr
        mov     al, 05h                 ; CTL1 SE0 (USB reset)
        mov     ah, 08h
        call    sl_wr
        mov     ecx, 30000
        call    iodelay
        mov     al, 05h
        mov     ah, 00h
        call    sl_wr
        mov     ecx, 3000
        call    iodelay
        mov     al, 0Eh                 ; SOF counter low
        mov     ah, 0E0h
        call    sl_wr
        mov     al, 0Fh                 ; CTL2 HOST | SOF-high (full speed)
        mov     ah, 0AEh
        call    sl_wr
        mov     al, 02h                 ; SOF kickstart
        mov     ah, 00h
        call    sl_wr
        mov     al, 03h
        mov     ah, 50h
        call    sl_wr
        mov     al, 04h
        mov     ah, 00h
        call    sl_wr
        mov     al, 00h
        mov     ah, 01h
        call    sl_wr
        mov     al, 05h                 ; CTL1 SOF_ENA
        mov     ah, 01h
        call    sl_wr
        mov     al, 0Dh
        mov     ah, 0FFh
        call    sl_wr
        mov     al, 06h                 ; INTENA SOF-timer gate
        mov     ah, 10h
        call    sl_wr
        mov     ecx, 100000             ; ~100ms recovery
        call    iodelay
        ret
EndProc do_usbinit_fs

;----------------------------------------------------------------------------
; msc_mount: enumerate a direct-attached Bulk-Only mass-storage device in
; ring 0 and read its capacity.  Assumes bulk IN 0x81 / OUT 0x02 (the
; standard flash-drive layout).  Fills msc_addr/blocks/bsize.  CF on error.
;----------------------------------------------------------------------------
BeginProc msc_mount
        mov     byte ptr [msc_mounted], 0
        call    sup_stop
        call    hid_stop
        call    do_usbinit_fs

        mov     al, 80h                 ; GET_DESCRIPTOR(device,8) @ addr 0
        mov     ah, 06h
        mov     bx, 0100h
        xor     cx, cx
        mov     dx, 8
        call    setup_pkt
        mov     byte ptr [ct_addr], 0
        mov     byte ptr [ct_mps], 8
        mov     dword ptr [ct_wlen], 8
        call    do_control
        jc      mnt_fail

        mov     al, 00h                 ; SET_ADDRESS 1 @ addr 0
        mov     ah, 05h
        mov     bx, 1
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     byte ptr [ct_addr], 0
        mov     dword ptr [ct_wlen], 0
        call    do_control
        jc      mnt_fail
        mov     ecx, 5000
        call    iodelay

        mov     al, 00h                 ; SET_CONFIGURATION 1 @ addr 1
        mov     ah, 09h
        mov     bx, 1
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     byte ptr [ct_addr], 1
        mov     dword ptr [ct_wlen], 0
        call    do_control
        jc      mnt_fail

        mov     byte ptr [msc_addr], 1
        mov     byte ptr [msc_epin], 81h
        mov     byte ptr [msc_epout], 2
        mov     byte ptr [msc_togin], 0
        mov     byte ptr [msc_togout], 0
        mov     dword ptr [msc_tag], 1

        ; SCSI READ CAPACITY(10)
        push    edi
        cld
        xor     eax, eax
        mov     edi, OFFSET32 msc_cdb
        mov     ecx, 4
        rep     stosd                   ; zero the 16-byte CDB
        pop     edi
        mov     byte ptr [msc_cdb], 25h
        mov     byte ptr [msc_cdblen], 10
        mov     byte ptr [msc_dir], 1
        mov     dword ptr [msc_datalen], 8
        mov     dword ptr [msc_databuf], OFFSET32 msc_cap
        call    msc_bot
        jc      mnt_fail
        test    al, al                  ; CSW status 0 = good
        jnz     mnt_fail

        ; last LBA (big-endian) -> block count
        movzx   eax, byte ptr [msc_cap+0]
        shl     eax, 8
        mov     al, [msc_cap+1]
        shl     eax, 8
        mov     al, [msc_cap+2]
        shl     eax, 8
        mov     al, [msc_cap+3]
        inc     eax
        mov     [msc_blocks], eax
        movzx   eax, byte ptr [msc_cap+4]
        shl     eax, 8
        mov     al, [msc_cap+5]
        shl     eax, 8
        mov     al, [msc_cap+6]
        shl     eax, 8
        mov     al, [msc_cap+7]
        test    eax, eax
        jnz     @F
        mov     eax, 512
@@:     cmp     eax, 512                ; ONLY 512-byte-block devices.  A
        jne     mnt_fail                ; garbled READ CAPACITY here once
                                        ; inflated datalen and the MBR read
                                        ; overflowed vol_secbuf into the
                                        ; next module ("Press any key to
                                        ; restart" landed on a pointer)
        mov     [msc_bsize], eax
        mov     byte ptr [msc_mounted], 1
        clc
        ret
mnt_fail:
        stc
        ret
EndProc msc_mount

;----------------------------------------------------------------------------
; setup_pkt: fill ct_setup with AL=bmRequestType AH=bRequest BX=wValue
;            CX=wIndex DX=wLength
;----------------------------------------------------------------------------
BeginProc setup_pkt
        mov     [ct_setup+0], al
        mov     [ct_setup+1], ah
        mov     [ct_setup+2], bl
        mov     [ct_setup+3], bh
        mov     [ct_setup+4], cl
        mov     [ct_setup+5], ch
        mov     [ct_setup+6], dl
        mov     [ct_setup+7], dh
        ret
EndProc setup_pkt

;----------------------------------------------------------------------------
; dev_present: quick attach check with the host idle.  AL=1 if attached.
;----------------------------------------------------------------------------
BeginProc dev_present
        mov     al, 05h                 ; CTL1 = 0 (normal, no force)
        mov     ah, 00h
        call    sl_wr
        mov     al, 0Dh                 ; clear INTSTAT
        mov     ah, 0FFh
        call    sl_wr
        push    ecx
        mov     ecx, 5000               ; ~5ms settle
        call    iodelay
        pop     ecx
        mov     al, 0Dh
        call    sl_rd
        test    al, 40h                 ; bit6 = 1 -> NO device
        jnz     dp_no
        mov     al, 1
        ret
dp_no:  xor     al, al
        ret
EndProc dev_present

;----------------------------------------------------------------------------
; hot-plug supervisor: a self-rescheduling 250ms timer that enumerates a
; boot mouse one short control transfer per tick (bounded event-time stall)
; and arms the VMOUSE poll.  States: 0 idle, 1 reset-release, 2 getdesc,
; 3 setaddr, 4 setconfig, 5 setproto+arm, 9 running.
;----------------------------------------------------------------------------
BeginProc CFU1_SupTick, High_Freq
        cmp     [sup_run], 0
        jz      sup_out                 ; disabled: don't re-arm

        mov     eax, 250                ; reschedule
        xor     edx, edx
        mov     esi, OFFSET32 CFU1_SupTick
        VMMCall Set_Global_Time_Out
        mov     [sup_handle], esi

        cmp     [enabled], 0
        jz      sup_out
        cmp     [xbusy], 0              ; a real transfer owns EP A: wait
        jnz     sup_busyw
        mov     [sup_busyct], 0
        mov     [xr_tries], 10          ; tight NAK budget at event time

        movzx   eax, [sup_state]
        cmp     eax, 9
        je      sup_running
        cmp     eax, 0
        je      sup_idle
        cmp     eax, 1
        je      sup_reset
        cmp     eax, 2
        je      sup_getdesc
        cmp     eax, 3
        je      sup_setaddr
        cmp     eax, 4
        je      sup_setcfg
        cmp     eax, 5
        je      sup_setproto
        cmp     eax, 6
        je      sup_classify
        cmp     eax, 10
        je      sup_diskrun
        cmp     eax, 11
        je      sup_hubscan
        cmp     eax, 12
        je      sup_hubenum
        cmp     eax, 13
        je      sup_hubscan             ; legacy hub-run state folds into 11
        mov     [sup_state], 0
        jmp     sup_out

sup_idle:
        call    dev_present
        test    al, al
        jz      sup_out
        mov     al, 05h                 ; assert SE0 (start USB reset)
        mov     ah, 08h
        call    sl_wr
        mov     [sup_state], 1          ; hold SE0 until next tick (~250ms)
        jmp     sup_out

sup_reset:
        mov     al, 05h                 ; release SE0
        mov     ah, 00h
        call    sl_wr
        mov     al, 0Eh                 ; SOF counter low
        mov     ah, 0E0h
        call    sl_wr
        mov     al, 0Fh                 ; CTL2: HOST | SOF-high (full speed)
        mov     ah, 0AEh
        call    sl_wr
        mov     al, 02h                 ; SOF kickstart: EP A len 0
        mov     ah, 00h
        call    sl_wr
        mov     al, 03h
        mov     ah, 50h                 ; PID=SOF ep0
        call    sl_wr
        mov     al, 04h
        mov     ah, 00h
        call    sl_wr
        mov     al, 00h                 ; ARM
        mov     ah, 01h
        call    sl_wr
        mov     al, 05h                 ; CTL1: SOF_ENA
        mov     ah, 01h
        call    sl_wr
        mov     al, 0Dh
        mov     ah, 0FFh
        call    sl_wr
        mov     al, 06h                 ; INTENA bit4 gates SOF timer
        mov     ah, 10h
        call    sl_wr
        mov     ecx, 15000              ; ~15ms recovery (rest via next tick)
        call    iodelay
        mov     [sup_state], 2
        jmp     sup_out

sup_getdesc:
        mov     al, 80h                 ; GET_DESCRIPTOR(device,8) @ addr 0
        mov     ah, 06h
        mov     bx, 0100h               ; wValue = device desc, index 0
        xor     cx, cx
        mov     dx, 8
        call    setup_pkt
        mov     [ct_addr], 0
        mov     [ct_mps], 8
        mov     dword ptr [ct_wlen], 8
        call    do_control
        jc      sup_reidle
        cmp     dword ptr [ct_got], 8
        jb      sup_reidle
        mov     al, [ct_buf+7]          ; bMaxPacketSize0
        test    al, al
        jnz     @F
        mov     al, 8
@@:     mov     [sup_mps], al
        mov     [sup_state], 3
        jmp     sup_out

sup_setaddr:
        mov     al, 00h                 ; SET_ADDRESS 1 @ addr 0
        mov     ah, 05h
        mov     bx, 1
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     [ct_addr], 0
        mov     al, [sup_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 0
        call    do_control
        jc      sup_reidle
        mov     [sup_state], 4
        jmp     sup_out

sup_setcfg:
        mov     al, 00h                 ; SET_CONFIGURATION 1 @ addr 1
        mov     ah, 09h
        mov     bx, 1
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [sup_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 0
        call    do_control
        jc      sup_reidle
        mov     [sup_state], 6          ; classify before assuming a mouse
        jmp     sup_out

sup_classify:
        mov     al, 80h                 ; GET_DESCRIPTOR(config,18) @ addr 1:
        mov     ah, 06h                 ; reaches bInterfaceClass/Protocol
        mov     bx, 0200h
        xor     cx, cx
        mov     dx, 18
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [sup_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 18
        call    do_control
        mov     byte ptr [sup_armmode], 1       ; default: mouse
        jc      sup_setproto            ; read failed -> assume HID mouse
        cmp     dword ptr [ct_got], 17          ; need through byte 16
        jb      sup_setproto            ; short -> assume HID mouse
        mov     al, [ct_buf+14]         ; bInterfaceClass of interface 0
        mov     [sup_class], al
        cmp     al, 08h                 ; mass storage?
        je      sup_cl_disk
        cmp     al, 09h                 ; hub?
        je      sup_cl_hub
        cmp     al, 03h                 ; HID?
        jne     sup_setproto            ; unknown -> mouse default (harmless)
        cmp     byte ptr [ct_buf+16], 1 ; bInterfaceProtocol 1 = keyboard
        jne     sup_setproto            ; 2=mouse / other -> mouse
        mov     byte ptr [sup_armmode], 2       ; keyboard
        jmp     sup_setproto
sup_cl_disk:
        call    sched_volup             ; disk: bring the volume up at appy
        mov     [sup_absent], 0
        mov     [sup_state], 10
        jmp     sup_out
sup_cl_hub:
        ; hub: read its descriptor, power every port, then scan them one
        ; per tick (the 250ms cadence provides the settle delays for free)
        mov     al, 0A0h                ; GET hub descriptor (class)
        mov     ah, 06h
        mov     bx, 2900h
        xor     cx, cx
        mov     dx, 8
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [sup_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 8
        call    do_control
        jc      sup_reidle
        cmp     dword ptr [ct_got], 3
        jb      sup_reidle
        mov     al, [ct_buf+2]          ; bNbrPorts
        test    al, al
        jz      sup_reidle
        cmp     al, 7
        jbe     @F
        mov     al, 7                   ; sanity cap
@@:     mov     [hub_nports], al
        mov     byte ptr [hub_port], 1
        mov     byte ptr [hub_naddr], 0
        mov     [hd2_act], 0
        mov     [hid_port], 0
        mov     [hd2_port], 0
        mov     [hub_parked], 0
        mov     al, [sup_mps]           ; sup_mps still = the hub's ep0 mps
        mov     [hub_mps], al           ; (downstream enum will clobber it)
        mov     [hub_conn], 0
        mov     [hub_rescan], 0
        mov     [hub_pwrfail], 0
        mov     [hub_estage], 0
        mov     [hub_p2pg], 0
        xor     eax, eax
        mov     ecx, 7
@@:     mov     [hub_pstat + ecx*4 - 4], eax
        loop    @B
        cmp     dword ptr [ct_got], 6   ; hub desc byte 5 = bPwrOn2PwrGood
        jb      @F
        mov     al, [ct_buf+5]
        mov     [hub_p2pg], al
@@:     call    hub_pwr_all             ; SET_FEATURE(PORT_POWER), every port
        mov     [sup_absent], 0
        mov     [sup_state], 11
        jmp     sup_out

sup_hubscan:
        ; continuous hub supervisor: watch the hub itself every tick, then
        ; service ONE port per tick (round-robin) — new CONNECTION on a
        ; free-slot port -> reset+enumerate; lost CONNECTION on a bound
        ; port -> tear that slot down; parked ports clear on disconnect
        mov     al, 0Dh                 ; hub still attached?
        call    sl_rd
        test    al, 40h
        jz      hb_here
        inc     [sup_absent]
        cmp     [sup_absent], 3
        jb      sup_out
hb_teardown:
        call    hid_stop                ; hub gone: tear everything down
        mov     [hd2_act], 0
        mov     [sup_armed], 0
        mov     [hid_port], 0
        mov     [hd2_port], 0
        mov     [hub_parked], 0
        mov     [hub_dead], 0
        mov     [sup_absent], 0
        mov     [sup_state], 0          ; state 0 = bus reset + re-descent,
        jmp     sup_out                 ; which revives a crashed hub brain
hb_here:
        mov     [sup_absent], 0
        ; ~4 control transfers/s of port polling wears cheap hub firmware
        ; down (recurring ep0 crashes) — service a port every 4th tick only
        inc     [hub_slow]
        test    [hub_slow], 3
        jnz     sup_out
        mov     cl, [hub_port]          ; port cursor wraps 1..nports
        cmp     cl, [hub_nports]
        jbe     @F
        mov     cl, 1
        mov     [hub_port], cl
        inc     [hub_rescan]            ; diag: completed passes
@@:
        ; GET_STATUS(port): 4 bytes; bit0 of byte0 = connected
        mov     al, 0A3h
        mov     ah, 00h
        xor     bx, bx
        movzx   cx, cl
        mov     dx, 4
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [hub_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 4
        call    do_control
        jc      hs_fail
        cmp     dword ptr [ct_got], 1
        jb      hs_short
        movzx   ecx, byte ptr [hub_port]        ; record for CFU_HUBSTAT
        mov     eax, dword ptr [ct_buf]
        mov     [hub_pstat + ecx*4 - 4], eax
        mov     [hub_dead], 0           ; the hub is talking to us
        mov     cl, [hub_port]
        test    byte ptr [ct_buf], 01h
        jz      hs_gone
        ; connected: bound or parked ports are healthy — leave them be
        cmp     cl, [hid_port]
        je      hs_next
        cmp     cl, [hd2_port]
        je      hs_next
        call    hub_pbit                ; AL = bit for port CL
        test    [hub_parked], al
        jnz     hs_next
        ; new device: only enumerate if a slot is free (class unknown
        ; until enum, so with both slots busy just leave it for later)
        cmp     [sup_armed], 0
        je      hs_enum
        cmp     [hd2_act], 0
        jne     hs_next
hs_enum:
        inc     [hub_conn]
        ; reset it; enumerate on the next tick
        mov     al, 23h
        mov     ah, 03h
        mov     bx, 4                   ; PORT_RESET
        movzx   cx, byte ptr [hub_port]
        xor     dx, dx
        call    setup_pkt
        mov     [ct_addr], 1
        mov     dword ptr [ct_wlen], 0
        call    do_control
        mov     [sup_state], 12
        jmp     sup_out
hs_gone:
        ; port empty: free whatever was tied to it
        call    hub_pbit
        not     al
        and     [hub_parked], al
        cmp     cl, [hid_port]
        jne     @F
        call    hid_stop
        mov     [sup_armed], 0
        mov     [hid_port], 0
@@:     cmp     cl, [hd2_port]
        jne     hs_next
        mov     [hd2_act], 0
        mov     [hd2_port], 0
        jmp     hs_next
hs_fail:
        movzx   ecx, byte ptr [hub_port]
        mov     dword ptr [hub_pstat + ecx*4 - 4], 0FFFFFFFFh
        ; a mute hub (crashed brain: forwards traffic, ep0 dead) never
        ; trips the detach check — watchdog it back to a bus reset
        inc     [hub_dead]
        cmp     [hub_dead], 8           ; ~8s at the 1-port/s scan rate
        jae     hb_teardown
        jmp     hs_next
hs_short:
        movzx   ecx, byte ptr [hub_port]
        mov     dword ptr [hub_pstat + ecx*4 - 4], 0FFFFFFFEh
hs_next:
        inc     byte ptr [hub_port]
        jmp     sup_out

sup_hubenum:
        mov     [hub_estage], 1         ; 1 = entered port enum
        mov     al, [hub_port]          ; per-port device address (8+port):
        add     al, 8                   ; unique forever -> replug reuses it,
        mov     [hub_naddr], al         ; parked devices collide with nobody
        ; clear C_PORT_RESET, then bring the downstream device up
        mov     al, 23h
        mov     ah, 01h
        mov     bx, 14h                 ; C_PORT_RESET
        movzx   cx, byte ptr [hub_port]
        xor     dx, dx
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [hub_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 0
        call    do_control
        ; is the device low-speed?  (wPortStatus bit9)
        mov     byte ptr [dev_ls], 0
        mov     al, 0A3h
        mov     ah, 00h
        xor     bx, bx
        movzx   cx, byte ptr [hub_port]
        mov     dx, 4
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [hub_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 4
        call    do_control
        jc      @F
        movzx   ecx, byte ptr [hub_port]        ; post-reset status (diag)
        mov     eax, dword ptr [ct_buf]
        mov     [hub_pstat + ecx*4 - 4], eax
        test    byte ptr [ct_buf+1], 02h
        jz      @F
        mov     byte ptr [dev_ls], 80h  ; LS: preamble on every transaction
@@:     mov     al, [dev_ls]
        mov     [ct_pre], al
        ; device answers at address 0 now
        mov     al, 80h                 ; GET_DESCRIPTOR(device,8) @ 0
        mov     ah, 06h
        mov     bx, 0100h
        xor     cx, cx
        mov     dx, 8
        call    setup_pkt
        mov     byte ptr [ct_addr], 0
        mov     byte ptr [ct_mps], 8
        mov     dword ptr [ct_wlen], 8
        call    do_control
        jc      he_skip
        cmp     dword ptr [ct_got], 8
        jb      he_skip
        mov     al, [ct_buf+7]
        test    al, al
        jnz     @F
        mov     al, 8
@@:     mov     [sup_mps], al           ; downstream ep0 mps
        mov     [hub_estage], 2         ; 2 = device desc read @ addr 0
        mov     al, 00h                 ; SET_ADDRESS(next)
        mov     ah, 05h
        movzx   bx, byte ptr [hub_naddr]
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     byte ptr [ct_addr], 0
        mov     dword ptr [ct_wlen], 0
        call    do_control
        jc      he_skip
        mov     [hub_estage], 3         ; 3 = SET_ADDRESS ok
        ; classify: config descriptor bytes 14 (class) / 16 (protocol)
        mov     al, 80h
        mov     ah, 06h
        mov     bx, 0200h
        xor     cx, cx
        mov     dx, 18
        call    setup_pkt
        mov     al, [hub_naddr]
        mov     [ct_addr], al
        mov     al, [sup_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 18
        call    do_control
        jc      he_skip
        cmp     dword ptr [ct_got], 17
        jb      he_skip
        mov     [hub_estage], 4         ; 4 = classified
        cmp     byte ptr [ct_buf+14], 03h       ; HID only behind the hub
        jne     he_skip
        ; SET_CONFIGURATION 1
        mov     al, 00h
        mov     ah, 09h
        mov     bx, 1
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     al, [hub_naddr]
        mov     [ct_addr], al
        mov     dword ptr [ct_wlen], 0
        call    do_control
        ; SET_PROTOCOL(boot)
        mov     al, 21h
        mov     ah, 0Bh
        xor     bx, bx
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     al, [hub_naddr]
        mov     [ct_addr], al
        mov     dword ptr [ct_wlen], 0
        call    do_control
        cmp     byte ptr [ct_buf+16], 1 ; keyboard?
        jne     he_mouse
        ; the keyboard OWNS the primary slot (stateful pipeline lives
        ; there).  If a mouse got there first — enum order is a race, the
        ; kbd may fumble its first try at boot — demote it to slot B.
        cmp     [sup_armed], 0
        je      he_kbd_arm
        cmp     [hid_mode], 1           ; primary holds a mouse?
        jne     he_park                 ; no: a second keyboard -> park
        cmp     [hd2_act], 0            ; slot B free for the demotion?
        jne     he_park
        call    hid_stop                ; poll timer restarts at hid_arm
        mov     al, [hid_addr]
        mov     [hd2_addr], al
        mov     al, [hid_ep]
        mov     [hd2_ep], al
        mov     al, [hid_mps]
        mov     [hd2_mps], al
        mov     [hd2_tog], 0
        mov     [hd2_mode], 1
        mov     al, [hid_pre]
        mov     [hd2_pre], al
        mov     al, [hid_port]
        mov     [hd2_port], al
        mov     [hd2_act], 1
        mov     [sup_armed], 0
        mov     [hid_port], 0
he_kbd_arm:
        mov     al, [hub_naddr]
        mov     [hid_addr], al
        mov     [hid_ep], 1
        mov     [hid_mps], 8
        mov     [hid_ivl], 8
        mov     [hid_mode], 2
        mov     al, [dev_ls]
        mov     [hid_pre], al
        mov     al, [hub_port]
        mov     [hid_port], al
        call    hid_arm
        mov     [sup_armed], 1
        jmp     he_next
he_mouse:
        cmp     [sup_armed], 0          ; mouse: primary if free, else slot B
        jne     he_m2
        mov     al, [hub_naddr]
        mov     [hid_addr], al
        mov     [hid_ep], 1
        mov     [hid_mps], 8
        mov     [hid_ivl], 8
        mov     [hid_mode], 1
        mov     al, [dev_ls]
        mov     [hid_pre], al
        mov     al, [hub_port]
        mov     [hid_port], al
        call    hid_arm
        mov     [sup_armed], 1
        jmp     he_next
he_m2:
        cmp     [hd2_act], 0            ; slot B free?  (shouldn't happen —
        jne     he_park                 ; the scan gates on a free slot)
        mov     al, [hub_naddr]
        mov     [hd2_addr], al
        mov     [hd2_ep], 1
        mov     [hd2_mps], 8
        mov     [hd2_tog], 0
        mov     [hd2_mode], 1
        mov     al, [dev_ls]
        mov     [hd2_pre], al
        mov     al, [hub_port]
        mov     [hd2_port], al
        mov     [hd2_act], 1
        jmp     he_next
he_park:
        ; enumerated but no slot for it (e.g. a second keyboard): leave
        ; it addressed+idle and stop rescanning it until it disconnects
        mov     cl, [hub_port]
        call    hub_pbit
        or      [hub_parked], al
        mov     [hub_estage], 6         ; 6 = parked
        jmp     he_skip
he_next:
        mov     [hub_estage], 5         ; 5 = armed
he_skip:
        mov     byte ptr [ct_pre], 0    ; preamble only during LS enum
        inc     byte ptr [hub_port]
        mov     [sup_state], 11
        jmp     sup_out

sup_diskrun:
        mov     al, 0Dh                 ; live presence (SOF running)
        call    sl_rd
        test    al, 40h                 ; bit6 = no device
        jz      sup_disk_still
        inc     [sup_absent]
        cmp     [sup_absent], 3
        jb      sup_out
        mov     byte ptr [msc_mounted], 0       ; fail I/O fast
        call    sched_voldown           ; tear the letter down at appy
        mov     [sup_absent], 0
        mov     [sup_state], 0
        jmp     sup_out
sup_disk_still:
        mov     [sup_absent], 0
        cmp     byte ptr [msc_mounted], 0
        jne     sup_out                 ; mounted: nothing to heal
        cmp     dword ptr [log_dcb_ptr], 0
        jne     sup_out                 ; ejected on purpose: stay down
        ; device present but never mounted (e.g. it answered late after
        ; socket power-up): retry the bring-up every ~4s
        inc     [sup_retry]
        cmp     [sup_retry], 16
        jb      sup_out
        mov     [sup_retry], 0
        call    sched_volup
        jmp     sup_out

sup_setproto:
        mov     al, 21h                 ; SET_PROTOCOL(boot) @ addr 1, iface 0
        mov     ah, 0Bh
        xor     bx, bx                  ; wValue = 0 (boot)
        xor     cx, cx
        xor     dx, dx
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [sup_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 0
        call    do_control              ; ignore result (some mice STALL it)
        ; arm the VMOUSE poll: ep1, addr1, mps8 (boot report), 8ms
        mov     [hid_ep], 1
        mov     [hid_addr], 1
        mov     [hid_mps], 8
        mov     [hid_ivl], 8
        mov     al, [sup_armmode]       ; 1=mouse (VMOUSE), 2=keyboard (VKD)
        mov     [hid_mode], al
        call    hid_arm
        mov     [sup_armed], 1
        mov     [sup_absent], 0
        mov     [sup_state], 9
        jmp     sup_out

sup_running:
        mov     al, 0Dh                 ; live presence (SOF running)
        call    sl_rd
        test    al, 40h                 ; bit6 = no device
        jz      sup_still
        inc     [sup_absent]
        cmp     [sup_absent], 3
        jb      sup_out
        call    hid_stop
        mov     [sup_armed], 0
        mov     [sup_absent], 0
        mov     [sup_state], 0
        jmp     sup_out
sup_still:
        mov     [sup_absent], 0
        jmp     sup_out

sup_reidle:                             ; enumeration step failed: retry later
        mov     [sup_state], 0
        jmp     sup_out
sup_busyw:
        ; xbusy watchdog: legit holders finish in <1 tick (4KB IOR ~70ms)
        ; or ~4 ticks (IRQ test).  ~5s of consecutive busy = a leaked
        ; error path wedged the engine — force-free it so input and
        ; detach detection can never freeze forever.
        inc     [sup_busyct]
        cmp     [sup_busyct], 20
        jb      sup_out
        mov     [xbusy], 0
        mov     [sup_busyct], 0
sup_out:
        ret
EndProc CFU1_SupTick

;----------------------------------------------------------------------------
; hub_pwr_all: SET_FEATURE(PORT_POWER) on hub ports 1..hub_nports (hub is
; always downstream addr 1).  Counts failures in hub_pwrfail for CFU_HUBSTAT.
;----------------------------------------------------------------------------
; hub_pbit: AL = port bit (1 << (CL-1)) for the hub_parked mask; CL preserved
BeginProc hub_pbit
        push    ecx
        mov     al, 1
        dec     cl
        shl     al, cl
        pop     ecx
        ret
EndProc hub_pbit

BeginProc hub_pwr_all
        mov     cl, 1
hp_next:
        push    ecx
        mov     al, 23h
        mov     ah, 03h
        mov     bx, 8                   ; PORT_POWER
        movzx   cx, cl
        xor     dx, dx
        call    setup_pkt
        mov     [ct_addr], 1
        mov     al, [hub_mps]
        mov     [ct_mps], al
        mov     dword ptr [ct_wlen], 0
        call    do_control
        pop     ecx
        jnc     @F
        inc     [hub_pwrfail]
@@:     inc     cl
        cmp     cl, [hub_nports]
        jbe     hp_next
        ret
EndProc hub_pwr_all

; sup_start: begin the supervisor if not already running (ESI-safe)
BeginProc sup_start
        cmp     [sup_run], 0
        jnz     ss_done
        mov     [sup_run], 1
        mov     [sup_armed], 0
        mov     [sup_absent], 0
        mov     [sup_state], 0
        push    esi
        mov     eax, 250
        xor     edx, edx
        mov     esi, OFFSET32 CFU1_SupTick
        VMMCall Set_Global_Time_Out
        mov     [sup_handle], esi
        pop     esi
ss_done:
        ret
EndProc sup_start

; sup_resume_disk: restart the supervisor tick in disk-run state after a
; ring-0 mount stopped it (msc_mount calls sup_stop to own the chip).
; No bus reset — the freshly mounted device must survive.  Idempotent.
; If the mount FAILED (msc_mounted=0), state 10's sup_retry logic then
; re-attempts vol_up every ~4s — late-answering sticks self-heal too.
BeginProc sup_resume_disk
        cmp     [sup_run], 0
        jnz     srd_done
        mov     [sup_run], 1
        mov     [sup_absent], 0
        mov     [sup_retry], 0
        mov     [sup_state], 10         ; watch the mounted disk
        push    esi
        mov     eax, 250
        xor     edx, edx
        mov     esi, OFFSET32 CFU1_SupTick
        VMMCall Set_Global_Time_Out
        mov     [sup_handle], esi
        pop     esi
srd_done:
        ret
EndProc sup_resume_disk

BeginProc sup_stop
        mov     [sup_run], 0
        cmp     [sup_handle], 0
        jz      @F
        push    esi
        mov     esi, [sup_handle]
        mov     [sup_handle], 0
        VMMCall Cancel_Time_Out
        pop     esi
@@:     cmp     [sup_armed], 0
        jz      @F
        call    hid_stop
        mov     [sup_armed], 0
@@:     ret
EndProc sup_stop

;----------------------------------------------------------------------------
; HID poll timer: reschedules itself, then runs one interrupt-IN
; transaction; ACKed data goes into the ring buffer.
; Callback convention: EBX=VM, ECX=ms late, EDX=refdata.
;----------------------------------------------------------------------------
BeginProc CFU1_HidTick, High_Freq
        cmp     [hid_run], 0
        jz      ht_out                  ; stop requested: don't re-arm

        movzx   eax, [hid_ivl]          ; reschedule first: steady cadence
        xor     edx, edx
        mov     esi, OFFSET32 CFU1_HidTick
        VMMCall Set_Global_Time_Out
        mov     [hid_handle], esi       ; handle comes back in ESI

        cmp     [xbusy], 0              ; someone mid-transaction? skip tick
        jnz     ht_out
        mov     [xbusy], 1

        mov     al, [hid_ep]
        and     al, 0Fh
        or      al, 90h                 ; IN | endpoint
        mov     [xa_pidep], al
        mov     al, [hid_addr]
        mov     [xa_addr], al
        mov     al, [hid_mps]
        mov     [xa_len], al
        mov     al, [hid_tog]
        or      al, 03h                 ; ARM|ENABLE
        or      al, [hid_pre]           ; PRE for LS-behind-hub
        mov     [xa_ctl], al
        mov     [xa_budget], 3000       ; short budget at event time
        call    do_xact
        mov     [xa_budget], 50000
        jc      ht_noreport             ; timeout: no new report this tick
        test    al, 01h                 ; ACK?
        jz      ht_noreport             ; NAK/error: no data this tick
        xor     [hid_tog], 40h
        mov     al, 04h                 ; remaining
        call    sl_rd
        mov     ah, [hid_mps]
        sub     ah, al
        movzx   ecx, ah                 ; ECX = got
        test    ecx, ecx
        jz      ht_done
        cmp     ecx, 8
        jbe     @F
        mov     ecx, 8                  ; slot payload cap
@@:
        ; store into ring slot at head
        mov     eax, [hid_head]
        lea     edx, [eax+eax*8]        ; head * 9
        add     edx, OFFSET32 hidring
        mov     [edx], cl               ; slot: len byte first
        push    edi
        lea     edi, [edx+1]
        mov     al, 10h
        call    buf_from_chip
        pop     edi

        ; dispatch the report by mode
        test    [hid_mode], 2           ; keyboard?
        jz      ht_trymouse
        push    edx
        lea     esi, [edx+1]            ; report bytes
        call    hid_kbd_report
        call    kb_flush                ; schedule task-time injection
        pop     edx
        jmp     ht_nopost
ht_trymouse:
        ; ring-0 pointer: hand a boot-protocol report straight to VMOUSE
        test    [hid_mode], 1
        jz      ht_nopost
        cmp     byte ptr [edx], 3       ; need buttons+dx+dy at least
        jb      ht_nopost
        push    ebx
        push    esi
        push    edi
        push    edx
        movzx   ebx, byte ptr [edx+1]   ; HID buttons
        and     ebx, 7
        mov     al, [btntab+ebx]        ; AL = VMOUSE button format
        movsx   esi, byte ptr [edx+2]   ; ESI = delta X
        movsx   edi, byte ptr [edx+3]   ; EDI = delta Y
        VxDCall VMD_Post_Pointer_Message
        pop     edx
        pop     edi
        pop     esi
        pop     ebx
ht_nopost:
        mov     eax, [hid_head]
        inc     eax
        and     eax, HIDRING_SLOTS-1
        mov     [hid_head], eax
        cmp     eax, [hid_tail]         ; full? drop oldest
        jne     ht_done
        mov     eax, [hid_tail]
        inc     eax
        and     eax, HIDRING_SLOTS-1
        mov     [hid_tail], eax
ht_done:
        call    hid2_poll               ; second device (chip still ours)
        mov     [xbusy], 0
ht_out:
        ret

ht_noreport:                            ; NAK/timeout: no fresh report
        test    [hid_mode], 2           ; keyboard typematic repeat
        jz      ht_done
        call    hid_kbd_idle
        call    kb_flush
        jmp     ht_done
EndProc CFU1_HidTick

; hid2_poll: one interrupt-IN poll of slot B (called with xbusy held).
; Mouse-only dispatch (stateless VMD post); the stateful keyboard pipeline
; stays on the primary slot.
BeginProc hid2_poll, High_Freq
        cmp     [hd2_act], 0
        jz      h2_out
        mov     al, [hd2_ep]
        and     al, 0Fh
        or      al, 90h
        mov     [xa_pidep], al
        mov     al, [hd2_addr]
        mov     [xa_addr], al
        mov     al, [hd2_mps]
        mov     [xa_len], al
        mov     al, [hd2_tog]
        or      al, 03h
        or      al, [hd2_pre]
        mov     [xa_ctl], al
        mov     [xa_budget], 3000
        call    do_xact
        mov     [xa_budget], 50000
        jc      h2_out
        test    al, 01h
        jz      h2_out
        xor     [hd2_tog], 40h
        mov     al, 04h                 ; remaining
        call    sl_rd
        mov     ah, [hd2_mps]
        sub     ah, al
        movzx   ecx, ah
        cmp     ecx, 3
        jb      h2_out                  ; need buttons+dx+dy
        cmp     ecx, 8
        jbe     @F
        mov     ecx, 8
@@:     push    edi
        mov     edi, OFFSET32 hd2_buf
        mov     al, 10h
        call    buf_from_chip
        pop     edi
        test    [hd2_mode], 1
        jz      h2_out
        push    ebx
        push    esi
        push    edi
        movzx   ebx, byte ptr [hd2_buf]
        and     ebx, 7
        mov     al, [btntab+ebx]
        movsx   esi, byte ptr [hd2_buf+1]
        movsx   edi, byte ptr [hd2_buf+2]
        VxDCall VMD_Post_Pointer_Message
        pop     edi
        pop     esi
        pop     ebx
h2_out:
        ret
EndProc hid2_poll

;----------------------------------------------------------------------------
; keyboard: HID boot report -> scan codes -> VKD_Force_Keys.
; Helpers preserve EBX/ECX/EDX/ESI/EDI so caller loops stay intact.
;----------------------------------------------------------------------------
; kb_force: enqueue one raw scan byte (AL) for task-time injection.
; The tick runs at interrupt time where VKD_Force_Keys is unsafe, so we
; queue here and drain from a Schedule_Global_Event callback (kb_drain).
BeginProc kb_force
        push    eax
        push    ebx
        push    edx
        mov     ebx, [kb_sh]
        mov     edx, ebx
        and     edx, 127
        mov     [kb_sbuf + edx], al
        inc     ebx
        mov     [kb_sh], ebx
        inc     dword ptr [kb_dbg_enq]
        pop     edx
        pop     ebx
        pop     eax
        ret
EndProc kb_force

; kb_flush: schedule a task-time drain if scan codes are queued
BeginProc kb_flush
        mov     eax, [kb_sh]
        cmp     eax, [kb_st]
        je      kf_done                 ; nothing queued
        cmp     [kb_sched], 0
        jne     kf_done                 ; drain already scheduled
        mov     [kb_sched], 1
        inc     dword ptr [kb_dbg_sched]
        push    esi
        push    edx
        xor     edx, edx
        mov     esi, OFFSET32 kb_drain
        VMMCall Schedule_Global_Event
        pop     edx
        pop     esi
kf_done:
        ret
EndProc kb_flush

; kb_drain: task-time global-event callback; batch-inject the queue via VKD
BeginProc kb_drain
        push    ebx
        push    ecx
        push    edx
        push    esi
        push    edi
        inc     dword ptr [kb_dbg_drains]
        xor     edx, edx                ; batch length
kd_loop:
        mov     eax, [kb_st]
        cmp     eax, [kb_sh]
        je      kd_send
        and     eax, 127
        mov     cl, [kb_sbuf + eax]
        mov     [kb_batch + edx], cl
        inc     edx
        inc     dword ptr [kb_st]
        cmp     edx, 120
        jb      kd_loop
kd_send:
        test    edx, edx
        jz      kd_done
        add     [kb_dbg_forced], edx
        mov     esi, OFFSET32 kb_batch
        mov     ecx, edx
        VxDCall  VKD_Force_Keys
kd_done:
        mov     [kb_sched], 0
        pop     edi
        pop     esi
        pop     edx
        pop     ecx
        pop     ebx
        ret
EndProc kb_drain

; kb_key: emit a HID keyboard usage (AL) as make (AH=0) or break (AH=80h)
BeginProc kb_key
        push    ebx
        push    edx
        movzx   ebx, al
        cmp     ebx, KSCAN_MAX
        jae     kk_done
        movzx   edx, word ptr [kscan + ebx*2]
        test    dx, dx
        jz      kk_done                 ; unmapped usage
        test    dh, 80h                 ; E0-prefixed?
        jz      @F
        push    eax
        mov     al, 0E0h
        call    kb_force
        pop     eax
@@:     mov     al, dl                  ; make code
        or      al, ah                  ; |80h for break
        call    kb_force
kk_done:
        pop     edx
        pop     ebx
        ret
EndProc kb_key

; kb_mod: emit a modifier (AL = bit index 0-7) make (AH=0) / break (AH=80h)
BeginProc kb_mod
        push    ebx
        push    edx
        movzx   ebx, al
        movzx   edx, word ptr [kmod + ebx*2]
        test    dh, 80h
        jz      @F
        push    eax
        mov     al, 0E0h
        call    kb_force
        pop     eax
@@:     mov     al, dl
        or      al, ah
        call    kb_force
        pop     edx
        pop     ebx
        ret
EndProc kb_mod

; kb_contains: is keycode AL present in the 6 keycodes at [EDI]?  ZF=1 if yes
BeginProc kb_contains
        push    ecx
        push    edi
        mov     ecx, 6
        repne   scasb
        pop     edi
        pop     ecx
        ret
EndProc kb_contains

;----------------------------------------------------------------------------
; hid_kbd_report: ESI -> fresh 8-byte boot report.  Diff against kb_prev,
; emit make/break for modifiers and keys, update the typematic target.
;----------------------------------------------------------------------------
BeginProc hid_kbd_report
        push    ebx
        push    ecx
        push    edx
        push    esi
        push    edi
        inc     dword ptr [kb_dbg_reports]

        ; ---- modifiers (byte 0) ----
        mov     cl, [esi]               ; new mod state (shifts down)
        mov     ch, [kb_prev]           ; old mod state (shifts down)
        mov     bl, cl
        xor     bl, ch                  ; changed bits
        xor     bh, bh                  ; bit index 0..7
km_loop:
        test    bl, 1
        jz      km_next
        mov     al, bh
        xor     ah, ah                  ; assume make
        test    cl, 1
        jnz     @F
        mov     ah, 80h                 ; now clear -> break
@@:     call    kb_mod
km_next:
        shr     bl, 1
        shr     cl, 1
        inc     bh
        cmp     bh, 8
        jb      km_loop

        ; ---- breaks: old keys no longer present ----
        mov     ecx, 6                  ; index 2..7
        mov     ebx, 2
kb_brk:
        mov     al, [kb_prev + ebx]
        test    al, al
        jz      kb_brk_next
        lea     edi, [esi + 2]          ; new keycodes
        call    kb_contains
        jz      kb_brk_next             ; still down
        mov     ah, 80h
        call    kb_key                  ; break
kb_brk_next:
        inc     ebx
        cmp     ebx, 8
        jb      kb_brk

        ; ---- makes: new keys not previously present; track last for repeat ----
        mov     [kb_held], 0
        mov     ebx, 2
kb_mk:
        mov     al, [esi + ebx]
        test    al, al
        jz      kb_mk_next
        cmp     al, 4                   ; usage < 4 = error/rollover, skip
        jb      kb_mk_next
        mov     [kb_held], al           ; last valid key = repeat target
        push    ebx
        lea     edi, [kb_prev + 2]      ; old keycodes
        call    kb_contains
        pop     ebx
        jz      kb_mk_next              ; was already down
        xor     ah, ah
        call    kb_key                  ; make
kb_mk_next:
        inc     ebx
        cmp     ebx, 8
        jb      kb_mk

        mov     [kb_ticks], 0           ; restart typematic delay

        ; ---- save report ----
        mov     eax, [esi]
        mov     dword ptr [kb_prev], eax
        mov     eax, [esi + 4]
        mov     dword ptr [kb_prev + 4], eax

        pop     edi
        pop     esi
        pop     edx
        pop     ecx
        pop     ebx
        ret
EndProc hid_kbd_report

;----------------------------------------------------------------------------
; hid_kbd_idle: no fresh report; synthesize typematic repeat for a held key.
;----------------------------------------------------------------------------
BeginProc hid_kbd_idle
        cmp     [kb_held], 0
        jz      ki_done
        mov     eax, [kb_ticks]
        inc     eax
        mov     [kb_ticks], eax
        cmp     eax, KB_DELAY
        jb      ki_done
        sub     eax, KB_DELAY
        test    eax, KB_RATE - 1        ; KB_RATE is a power of two
        jnz     ki_done
        mov     al, [kb_held]
        xor     ah, ah
        call    kb_key                  ; repeated make
ki_done:
        ret
EndProc hid_kbd_idle

;----------------------------------------------------------------------------
; hardware interrupt handler: ack the SL811, count, EOI
;----------------------------------------------------------------------------
BeginProc CFU1_HwInt, High_Freq
        ; one-master rule: we may only touch the SL811 when the engine is
        ; parked in a do_xact wait window (await) or the canned IRQ test
        ; owns the chip (irq_hold).  Otherwise: count + EOI, nothing else
        ; (the PIC line is masked outside windows, so no storm).
        mov     al, [await]
        or      al, [irq_hold]
        jz      hw_blind
        push    edx
        movzx   edx, [iobase]
        mov     al, 0Dh                 ; SL811 interrupt status
        out     dx, al
        inc     edx
        in      al, dx
        test    al, 01h                 ; USB-A done?
        jnz     hw_done
hw_sof:
        mov     al, 0FEh                ; SOF etc: ack all but USB-A done
        out     dx, al                  ; (INTSTAT still selected)
        ; completion can latch between our read and that ack — the pin
        ; then stays high with no fresh edge and the IRQ is lost.  Re-read.
        in      al, dx
        test    al, 01h
        jz      hw_out
hw_done:
        dec     edx                     ; grab EP A packet status for the
        mov     al, 03h                 ; waiting engine
        out     dx, al
        inc     edx
        in      al, dx
        mov     [astat], al
        dec     edx
        mov     al, 0Dh
        out     dx, al
        inc     edx
        mov     al, 0FEh                ; ack all EXCEPT DONE: the engine's
        out     dx, al                  ; count/buffer readback needs the
                                        ; chip in the same state polled mode
                                        ; sees (DONE latched until the next
                                        ; xact preamble clears it)
        mov     [aflag], 1              ; signal the spinning engine
hw_out:
        pop     edx
hw_blind:
        inc     [irqcount]
        mov     eax, [irqhandle]        ; Phys_EOI wants the IRQ handle
        VxDCall VPICD_Phys_EOI
        clc
        ret
EndProc CFU1_HwInt

VxD_LOCKED_CODE_ENDS
        end
