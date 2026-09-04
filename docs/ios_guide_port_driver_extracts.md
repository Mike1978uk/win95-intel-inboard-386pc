# I/O Supervisor Guide — the port-driver extracts, verbatim

**Source.** Microsoft, *I/O Supervisor Guide for Windows 9x/Me Operating Systems* (c. 2000).
Posted by @andrew-hoffman on
[issue #21](https://github.com/Mike1978uk/win95-intel-inboard-386pc/issues/21).
Local copy: `C:\IOSGuide\IOS_Guide.doc`, 1,009,664 bytes, dated 2000-11-01.
**It is NOT in the Windows 95 DDK** — it postdates it and covers through Windows Me. The DDK's own
`STORAGE.DOC` says nothing about any of the below.

Companion local copy of the DDK itself (not redistributable, not in this repo):
`C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk` — `BLOCK\INC\DCB.INC`,
`BLOCK\SAMPLES\PORT\SAMPLE\`, `INC32`, `INC16`, `MASM611C`, `MSVC20`.

**How to extract the Guide** (Word binary; no parser needed — the same printable-run trick used on
`SYSTEM.DAT`, technique 65). Emit each run on its own line, which preserves the assembly listings:

```python
import re
d = open('C:/IOSGuide/IOS_Guide.doc', 'rb').read()
runs = re.findall(rb'[\x09\x20-\x7e]{3,}', d)
open('ios_guide_lines.txt', 'w', encoding='latin1').write(b'\n'.join(runs).decode('latin1'))
# 238,769 characters, 19,488 lines
```

Joining the runs with **spaces** instead gives 185,129 characters and destroys every code block — do
not do that if you want the source out of it.

**What it contains that we needed:** the polling contract in plain English, the full `IOS_serialize`
source, the `DCB_max_sg_elements` value confirmed three ways, and a `MaximumTransferLength` rule that
names a VMM deadlock.

**What it does not contain, checked explicitly:** no mention of `AEP_DCB_LOCK`,
`AEP_PEND_UNCONFIG_DCB`, `CREATE_VRP`, `DESTROY_VRP` or `System_Exit`. So it does **not** explain the
teardown truncation that is our actual symptom. Recording that saves the next reader a dead end.

> A keyword miss is not a document review. This file sat on disk from 2026-09-03 and was written off
> on 2026-09-04 after grepping four exact tokens, while a session went into reverse-engineering its
> contents out of binaries.

---

## 1. The request procedure a port driver must follow

Section 6, *IOS Port Driver general theory of operation*. Numbering is the Guide's own.

> Here is the general process that should occur when an IOS port driver receives a new IOP. Note it
> is assumed that the port driver is connected to a piece of hardware.
>
> 1. The port driver checks to see if the hardware is already busy performing I/O. If it isn't, go to
>    step 2 below. If it is (busy), it enqueues the new IOP using `ILB_enqueue_iop`. This function is
>    used to serialize I/O requests for your port driver. **After the enqueue, the driver does a
>    simple return (no IOS `IOP_callback_ptr` callback).**
>
>    Microsoft's ESDI_506.PDR source code uses the CLI instruction before the `ILB_enqueue_iop`:
>
>    ```asm
>            cli                                ; avoid race
>            push    esi                        ; *DCB
>            push    ebx                        ; *IOP
>            call    [esdi_ilb].ILB_enqueue_iop ; queue the request.
>            add     esp, 4+4
>    ```
>
>    STI is used afterwards. This is a precautionary measure to prevent re-entrant thread problems.
>
> 2. At this step, the hardware is not already busy, so the driver starts I/O for the device.
>    Normally at this step, the port driver is going to have to wait for hardware to respond. **If the
>    hardware polling method is used (instead of waking up when an interrupt arrives), the driver then
>    calls `Set_Global_Time_Out` or `Set_Async_Time_Out`** so that the driver's timeout (polling
>    handler) routine gets called back later, for example in 10 milliseconds. **Immediately after this
>    `Set_Global_Time_Out` call, simply return (WITHOUT doing a JMP to the `IOP_callback_ptr`
>    routine). This releases the system from your driver, so the system can run normally for a
>    while.**
>
> 3. After a time (for example 10 milliseconds), your polling handler gets called when the global
>    timer times out. Your handler checks your hardware's status. If the hardware has not completed,
>    re-issue a global timeout so the hardware can be checked again (10ms) later. If the hardware has
>    completed, finish the I/O, and **CALL (not JMP to) the `IOP_callback_ptr` routine**. This has the
>    effect of handing the IOP back to IOS in order to truly complete the request. Next, call
>    `ILB_dequeue_iop` to see if there are any queued IOP's. If there are, take the new IOP, and jump
>    to step 2 above, to start a new I/O. If there are no enqueued requests, do a simple return.
>
> 4. If hardware has an associated hardware interrupt, the procedure is more efficient because the
>    driver doesn't have to poll the hardware.

**Read against our own `PORTREQ.ASM`:** steps 1 and 4 are already there, inherited from the DDK
sample — the `ILB_enqueue_iop`, the `bts DDB_BF_ACTIVE_BIT` / `jc Port_rq_ret`
return-without-completing, and the `ILB_dequeue_iop` drain. **Only step 2's wait is wrong.**
`XTIDE_WaitNotBusy` / `XTIDE_WaitDrq` busy-spin `XT_SPIN` = 400,000 `in al,dx` *while holding*
`DDB_BF_ACTIVE_BIT`, where the Guide requires arm-a-timeout-and-return. `Port_iop_timeout`
(`AEP_IOP_TIMEOUT`) is an empty stub answering `AEP_SUCCESS` — the natural home for the step-3
handler.

---

## 2. `IOS_serialize` — full source, verbatim

The Guide's own framing:

> When set, the bit `DCB_DEV_SERIAL_CMD` in `dcb_device_flags` instructs IOS to add a special entry
> into the calldown stack for the DCB (after the port driver gets its `AEP_CONFIG_DCB` call). The
> call is to a routine named `IOS_serialize`. This is used internally only, to support real mode
> devices and blockdev (Win 3.1 Fastdisk (32-bit ring 0) drivers).
>
> The source code for `IOS_serialize` (below) is enlightening, since it demonstrates use of queueing
> and dequeueing. **Your port driver should do its own enqueueing via `ILB_enqueue_iop` etc.** Study
> the code below to help understand IOP queuing mechanics. Note that if you write a VSD (Vendor
> Supplied Device), residing in the IOS layered hierarchy between IOS and the port driver, and your
> VSD needs to enqueue IOPs, **you cannot use `ILB_enqueue_iop` because it is reserved for use by
> port drivers.** Instead, your VSD will need to implement a private queuing mechanism.
>
> You may have noticed the flag `DCB_dmd_serialize` in the header file DCB.H. This flag is never used.

```asm
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  IOS_serialize
;       Routine serializes all I/O to a physical device
;       INPUT:  *IOP on stack
;       OUTPUT: none
;       USES:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
BeginProc       IOS_serialize, esp
ArgVar  IOPPtr, DWORD
        EnterProc
        SaveReg <edi>
        mov     eax, IOPPtr
        AssertIOP <eax>
        mov     edi, [eax].IOP_physical_dcb
        ;
        ; insert in callback stack
        ;
        mov     edx, [eax.IOP_callback_ptr]                    ; set CB
        mov     [edx.IOP_cb_address],offset32 IOS_serialize_callback
        add     [eax.IOP_callback_ptr],size IOP_callBack_entry ; move down
        AssertDCB <edi>
        ;
        ; enqueue the IOP
        ;
        push    edi
        push    eax
        call    IOS_enqueue_iop
        add     esp, 8
        call    IOS_bd_send_next_command
IOS_s_exit:
        RestoreReg <edi>
        LeaveProc
        Return
EndProc IOS_serialize

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;  IOS_serialize_callback
;       Completion routine for request serialization
;       INPUT:  *IOP on stack
;       OUTPUT: none
;       USES:
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
BeginProc       IOS_serialize_callback, esp
ArgVar  CBIOPPtr, DWORD
        EnterProc
        SaveReg <esi, edi>
        mov     esi, CBIOPPtr
        AssertIOP <esi>

        mov     edi, [esi].IOP_physical_dcb
        AssertDCB <edi>
        mov     ecx, [esi].IOP_callback_ptr    ; get our callback ptr
        sub     ecx, size IOP_CallBack_Entry   ; point to next available
                                               ; callback entry
        mov     [esi].IOP_callback_ptr, ecx    ; update CallBack Pointer
        ;IOP pointer is passed on the stack
        push    esi                            ; IOP's offset
        call    [ecx].IOP_cb_address           ; make the call
        add     esp, 4                         ; restore stack
        AssertDCB <edi>
        ;
        ; dequeue the next request
        ;
        ASSERT_INTS_ENABLED
        cli
        cmp     [edi].DCB_BDD.DCB_BDP_Current_Command, 0
        je      IOS_sc_send_next
        cmp     esi, [edi].DCB_BDD.DCB_BDP_Current_Command  ; Is this the current cmd?
        jne     ios_sc_exit
        mov     [edi].DCB_BDD.DCB_BDP_Current_Command, 0    ; No current command!
IOS_sc_send_next:
        call    IOS_bd_send_next_command
IOS_sc_exit:
        sti
        RestoreReg <edi, esi>
        LeaveProc
        Return
EndProc IOS_serialize_callback

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;       Starts the next request for a device
;       ENTRY   edi => DCB
;               ints disabled
;       EXIT    ints enabled
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
BeginProc       IOS_bd_send_next_command
        AssertDCB<edi>
        cmp     [edi].DCB_BDD.DCB_BDP_Current_Command, 0
        jne     short ios_snc_exit
        push    edi
        call    IOS_dequeue_iop
        add     esp, 4
        or      eax, eax
        jz      IOS_snc_exit
        AssertIOP <eax>
        mov     [edi].DCB_BDD.DCB_BDP_Current_Command, eax
        sti
        SaveReg <esi, edi, ebx>
        lea     edi, [edi].DCB_BDD              ; (edi) = BDD
        mov     ecx, [eax].IOP_calldown_ptr     ; call down the request
        mov     ecx, [ecx].DCB_cd_next          ; get next entry
        mov     [eax].IOP_calldown_ptr, ecx     ; store in IOP
        push    eax                             ; place IOP pointer on stack
        call    [ecx].DCB_CD_IO_Address         ;
        add     esp, 4                          ; call next layer down
        RestoreReg <ebx, edi, esi>

IOS_snc_exit:
        sti
        ret
EndProc IOS_bd_send_next_command
```

### What to take from it

- **Callback-stack insertion is the deferral mechanism.** `IOS_serialize` pushes its own completion
  routine onto the IOP's callback stack (`mov [edx.IOP_cb_address], offset32 IOS_serialize_callback`,
  then `add [eax.IOP_callback_ptr], size IOP_callBack_entry`) *before* queueing, so the request comes
  back through it later. Its callback walks the pointer back down
  (`sub ecx, size IOP_CallBack_Entry`), calls the next entry, and only then dequeues the next request.
- **The lock is a field, not a spin.** `DCB_BDP_Current_Command` holds the in-flight IOP;
  `IOS_bd_send_next_command` refuses to start anything while it is non-zero, and the callback clears
  it. Ours is `DDB_BF_ACTIVE_BIT` in the DDB — same idea, one bit.
- **`cli` around the queue check, `sti` before any call out.** `IOS_bd_send_next_command` states its
  contract as *"ENTRY ints disabled / EXIT ints enabled"* and re-enables before calling down. Note it
  `sti`s on **both** exits.
- The routine uses `IOS_enqueue_iop` / `IOS_dequeue_iop` — IOS's *internal* names. From a port driver
  the same services are reached through the ILB, as `[port_ilb].ILB_enqueue_iop`.

---

## 3. A semaphore worker thread — the reported alternative

The Guide gives this as *"a reportedly successful implementation of an IOS port driver"*. Its context
there is Ring-0 file I/O, but the structure is the general one — and it is the shape `HSFLOP.PDR`
actually uses (`Set_Global_Time_Out` at `2D95h`, `Wait_Semaphore` 43 bytes later at `2DBEh`, one
`Signal_Semaphore_No_Switch` elsewhere in the file):

```
CreateSemaphore()
VWIN32_CreateRing0Thread()
Set_Thread_Win32_Pri(16)   - other values may work

Thread()
while not stop
    Wait_Semaphore(BLOCK_THREAD_IDLE)
    Dequeue IOP
    Do R0 file I/O
    Do CallBack
endwhile

IOHandler(IOP)
if read or write
    Enqueue IOP
    Signal_Semaphore()
    return
endif
```

**A port driver MAY block, provided something asynchronous can free it.** That is a smaller change
than a full state machine, and it is why `HSFLOP` is the better template for us than `ESDI_506`: it is
the driver that must survive a device which may simply not answer.

---

## 4. `DCB_max_sg_elements` — 17, confirmed three ways

A field we never write. Ours stays `0` while scatter/gather requests demonstrably reach us
(technique 85 — the bug that wrote a descriptor list into the volume's root directory).

> It would be safe to assign (17*8) bytes of memory for `IOR_sgd_lin_phys` since **the maximum number
> of SGDs is 17.**

> `UCHAR DCB_max_sg_elements` — Max # of logical and/or physical s/g elements. **Set initially by
> port**, but may be MORE RESTRICTIVELY updated by other layers.

And three live `.IDCB` debugger dumps in the Guide's appendix agree on both offset and value:

```
vendor_id@8A GENERIC       current_unit@AE 00         max_sg_elements@77   11
```

`@77` and `11h` are exactly what disassembling `ESDI_506.PDR`'s `AEP_CONFIG_DCB` gave
(`mov byte ptr [ecx+77h], 11h`). Three independent sources, one answer.

`DCB_max_sg_elements` is declared in `STRUC DCB` (`BLOCK\INC\DCB.INC` line 94) — the same structure as
`DCB_bus_type` (line 91) and `DCB_max_xfer_len` (line 83), both of which we already write through
`ESI`. So `[esi].DCB_max_sg_elements` resolves correctly and technique 80's two-structures trap does
not apply here. Verified, not assumed.

---

## 5. `MaximumTransferLength` — a VMM deadlock, and why it is not ours

> A small max transfer size causes IOS to massively double-buffer I/O requests. **Under rare
> circumstances this can lead to an attempt to reenter VMM's memory manager, which causes a
> deadlock.** The recommended corrective action is to set `MaximumTransferLength` to 64K or larger.

Worth recording because our wedge *is* inside ring-0 VMM, so this reads like a hit. **It is not.**
`XTIDE_ConfigDcb` already sets `DCB_max_xfer_len` to 65536, which meets the stated bar. Written down
so nobody re-derives it as a lead. (`ESDI_506` uses `0FFFFFFFFh`.)

The Guide also names the deciding routine, for reference: `ILB_int_io_criteria_rtn`, called in
SCSIPORT just before `IOS_Send_Command`. It fails, and sets `IORF_DOUBLE_BUFFER`, if the request
exceeds `DCB_max_xfer_len`, if buffer address **or** length fails alignment, or if IOS cannot generate
the requested physical SGDs. Note the second of those: double-buffering can be triggered by alignment
alone, on a short transfer.

---

## 6. PIO devices and scatter/gather — the miniport escape hatch

Directly relevant to @andrew-hoffman's steer on #21 that a SCSI miniport is the better-supported
route for a third-party storage device:

> Under Windows 95, if the miniport driver does not report that it supports scatter-gather, then it
> will only see read requests for one block at a time. [...] **This is obviously a problem for PIO
> devices, which do not typically support the concept of scatter-gather. In these cases it is possible
> to force SCSIPORT.PDR to emulate scatter-gather on behalf of your device.** This is done by setting
> `BufferAccessScsiPortControlled=TRUE` in the `PORT_CONFIGURATION_INFORMATION` structure in your SCSI
> miniport.
>
> Please note that this field was originally only documented in the Windows NT 3.5x DDK (it was
> accidentally omitted from the Win95 DDK documentation). It is however correctly defined in the
> Windows 95 DDK include files.

With it set, never touch the data buffer directly — use only the `ScsiPort{Read,Write}...Buffer...`
functions, because the buffer is a linear SGD list rather than a contiguous block. Set `Master=TRUE`
only if you really bus-master; otherwise SCSIPORT allocates a 64K physically contiguous DMA buffer,
which on this machine is exactly what we do not want (technique 62, the 20-bit DMA reach).

A miniport additionally gets a documented shutdown notification, which the port-driver interface does
not offer at all:

> `SRB_FUNCTION_SHUTDOWN` — The system is being shut down. The request is passed to the miniport
> driver if `CachesData` was set to TRUE in the `PORT_CONFIGURATION_INFORMATION` data. The miniport
> driver can receive several of these notifications before all system activity is actually stopped;
> however, the last shutdown notification will occur after the last start I/O. Only the `Function`,
> `PathId`, `TargetId` and `Lun` fields are valid.

Also from the Guide, and relevant if we go this way: during SCSIPORT's own `AEP_CONFIG_DCB` it sets a
ceiling on `DCB_max_sg_elements` of 17, and if `Dma32BitAddresses = 0` it sets `DCB_dmd_small_memory`.
So on the miniport path, section 4 above stops being our job.

---

## Sources relied on, with what each gave

| source | gave us | did not contain |
|---|---|---|
| *I/O Supervisor Guide*, `C:\IOSGuide\IOS_Guide.doc` (@andrew-hoffman, #21) | the polling contract, `IOS_serialize` source, SG element count, the miniport SG escape hatch | anything on `AEP_DCB_LOCK`, `PEND_UNCONFIG_DCB`, `CREATE_VRP`/`DESTROY_VRP`, `System_Exit` |
| Win95 DDK `BLOCK\INC\DCB.INC` (local, not redistributable) | which `STRUC` declares each field | — |
| Win95 DDK `NEW95DOC\STORAGE.DOC` | `AEP_CONFIG_DCB` is where geometry goes; the `IOR_buffer_ptr` flag-dependent meaning | the polling contract; anything on deferral |
| `ESDI_506.PDR` / `HSFLOP.PDR` / `SCSIPORT.PDR` on the CF (`roms/xtcf_card/*_reference.PDR`) | the same contract as executed code, and `max_sg_elements = 11h` | — |
| [zikolas/cfu1-win9x](https://github.com/zikolas/cfu1-win9x) | a working MIT-licensed Win9x IOS port driver to diff against | — |
