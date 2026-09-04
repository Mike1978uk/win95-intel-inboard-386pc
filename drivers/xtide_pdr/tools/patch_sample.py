#!/usr/bin/env python3
r"""Phase 2c build-time edits to Microsoft's DDK sample, applied to the copies
in the build directory - never to the DDK itself.

  python patch_sample.py <build-dir>

The sample mixes tabs and spaces, so every anchor is a regex over \s+ rather
than a literal. Each edit asserts it matched; a moved anchor fails the build
instead of producing a driver that silently does nothing (Technique 28).
"""
import sys, os, re

TAB = chr(9)
NL = chr(10)
WS = '[ \t]'


def edit(path, subs):
    d = open(path, encoding='latin1').read()
    n = 0
    for name, pat, repl, flags in subs:
        new, cnt = re.subn(pat, repl, d, count=1, flags=flags)
        assert cnt == 1, 'anchor missing in %s: %s' % (os.path.basename(path), name)
        d = new
        n += cnt
    open(path, 'w', encoding='latin1', newline='').write(d)
    return n


def main():
    out = sys.argv[1]
    # BISECT (Technique 80): skip the sample's ISP calldown insert, so the
    # driver claims a unit but never joins the request chain. Splits
    # "Port_cfg_device is the fault" from everything downstream, in one run.
    nocalldown = '--nocalldown' in sys.argv
    # See the --novolume block below.
    novolume = '--novolume' in sys.argv
    total = 0

    # ---- PORTAER.ASM ------------------------------------------------------
    aer = os.path.join(out, 'PORTAER.ASM')
    total += edit(aer, [
        ('XTIDE_Inquiry extern',
         r'(extrn' + WS + r'+XTIDE_Probe:near[^\n]*\n)',
         lambda m: m.group(1) + TAB + 'extrn' + TAB + 'XTIDE_Inquiry:near' + TAB
                   + '; device inquiry (phase 2c)' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_WantDcb:near' + TAB
                   + '; calldown guard (phase 2d)' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_ConfigDcb:near' + TAB
                   + '; describe the device (phase 2d)' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_NoteDdb:near' + TAB
                   + '; remember our own DDB (phase 2d)' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_NoteLgn:near' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_SchedVol:near' + TAB
                   + '; be our own TSD (phase 3)' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_ForgetDcb:near' + TAB
                   + '; AEP_UNCONFIG_DCB teardown' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_PendUnconfig:near' + TAB
                   + '; AEP_PEND_UNCONFIG_DCB means STOP ALL I/O' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_VolDown:near' + TAB
                   + '; AEP_SYSTEM_SHUTDOWN teardown' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_Uninit:near' + TAB
                   + '; AEP_UNINITIALIZE is a command, not a notice' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_DbgAep:near' + TAB
                   + '; report every AEP as it arrives' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_ShutdownArm:near' + TAB
                   + '; AEP_SYSTEM_SHUTDOWN fast-fail gate' + NL,
         0),

        # The load group number the logical DCB's calldown entry will need.
        ('stash the load group number',
         r'(' + WS + r'*mov' + WS + r'+\[edi\]\.ISP_i_cd_lgn,' + WS + r'*al[^\n]*\n)',
         lambda m: m.group(1) + TAB + 'movzx' + TAB + 'eax, byte ptr [ebx.AEP_lgn]' + NL
                   + TAB + 'call' + TAB + 'XTIDE_NoteLgn' + NL,
         0),


        # Our DDB is how a request is later identified as ours - see
        # XTIDE_WantIop. AEP_ddb carries it on every AEP.
        ('stash our DDB',
         r'(' + WS + r'*mov' + WS + r'+eax,\[ebx\.AEP_ddb\][^\n]*\n)',
         lambda m: m.group(1) + TAB + 'call' + TAB + 'XTIDE_NoteDdb' + NL,
         0),

        # AEP_CONFIG_DCB is broadcast for DCBs this driver never claimed, and the
        # same DCB comes round again on a re-broadcast. The sample splices its
        # request routine into every one of them, so IOS routes another
        # controller's I/O down our calldown. XTIDE_WantDcb answers "did we claim
        # this DCB at inquiry, and have we not already inserted on it".
        # zikolas/cfu1-win9x guards the same way and says why. The sample's own
        # header claims it "examines a DCB to determine if we want to work with
        # this device" and then examines nothing.
        ('cfg_device claim guard',
         WS + r'*inc' + WS + r'+\[port_device_count\][^\n]*\n',
         (TAB + 'or' + TAB + 'esi, esi' + TAB + TAB + '; nothing to configure' + NL +
          TAB + 'jz' + TAB + 'vcd_ret' + NL +
          TAB + 'call' + TAB + 'XTIDE_WantDcb' + TAB + '; ours, and not already done?' + NL +
          TAB + 'or' + TAB + 'eax, eax' + NL +
          TAB + 'jnz' + TAB + 'vcd_ret' + NL +
          NL +
          TAB + 'inc' + TAB + '[port_device_count]' + TAB + '; show one more device' + NL +
          TAB + 'call' + TAB + 'XTIDE_ConfigDcb' + TAB + '; geometry belongs HERE, not at inquiry' + NL),
         0),

        # ISP_result is an output field, but the packet is raw stack memory and
        # cfu1 zeroes it explicitly before the call. One instruction.
        ('zero ISP_result',
         r'(' + WS + r'*mov' + WS + r'+\[edi\]\.ISP_func,ISP_insert_calldown[^\n]*\n)',
         lambda m: m.group(1) + TAB + 'mov' + TAB + '[edi].ISP_result, 0' + TAB
                   + '; not stack garbage' + NL,
         0),

        # Claim units instead of refusing them.
        ('phase 1 refusal',
         WS + r'*jmp' + WS + r'+Port_di_no_more_devices' + WS + r'*;' + WS
         + r'*phase 1 claims nothing' + WS + r'*\r?\n',
         (TAB + 'mov' + TAB + 'eax, ecx' + TAB + TAB + '; unit number; ESI already holds the DCB' + NL +
          TAB + 'call' + TAB + 'XTIDE_Inquiry' + TAB + '; eax = 0 present, 1 absent' + NL),
         0),

        # The sample loads ESI with the DCB from the AEP, then overwrites it with
        # EDI before handing it to ISP_insert_calldown - so IOS is asked to insert
        # a calldown on whatever EDI happened to hold. Harmless while the driver
        # claims nothing, because this path never runs. A Windows protection error
        # the moment it claims a device. Microsoft's bug, not ours.
        ('cfg_device DCB clobber',
         WS + r'*mov' + WS + r'+esi,' + WS + r'*edi' + WS + r'*\r?\n',
         ('; ' + TAB + 'mov' + TAB + 'esi,edi' + TAB
          + '; REMOVED - clobbers the DCB from AEP_d_c_dcb' + NL),
         0),
    ])

    # STORAGE.DOC: an absent unit returns AEP_NO_INQ_DATA "to direct the IOS to
    # inquire about the next unit". The sample answers AEP_FAILURE, which is the
    # code for a driver error, not for an empty slot.
    total += edit(aer, [
        ('no-device result code',
         r'(Port_di_no_device:' + NL + WS + r'*mov' + WS + r'+\[ebx\]\.AEP_result,' + WS + r'*)AEP_FAILURE',
         lambda m: m.group(1) + 'AEP_NO_INQ_DATA',
         0),
    ])

    # ---- shutdown and teardown -------------------------------------------
    # The sample dispatches five AEP function codes and answers AEP_FAILURE to
    # every other one. IOS broadcasts at least four it has never heard of, all
    # of them at System_Exit:
    #
    #   AEP_SYSTEM_SHUTDOWN 14   AEP_SYSTEM_CRIT_SHUTDOWN 1
    #   AEP_UNCONFIG_DCB     4   AEP_PEND_UNCONFIG_DCB   21
    #
    # ...plus AEP_ASSOCIATE_DCB 12, which OUR OWN ISP_ASSOCIATE_DCB causes IOS
    # to issue. These are notifications; "I refuse" is not an answer to one.
    #
    # The default becomes SUCCESS rather than enumerating every code IOS might
    # broadcast. Technique 51, in a new place: an exit condition gated on a
    # hand-written list of values fails silently and badly the first time
    # reality produces one the list does not have, and this project has now
    # paid for that four times over.
    total += edit(aer, [
        ('unconfig handler + benign default',
         WS + r'*mov' + WS + r'*\[ebx\.AEP_result\],AEP_FAILURE;' + WS
         + r'*set result code to indicate error\.' + WS + r'*\r?\n',
         lambda m: (
             TAB + 'call' + TAB + 'XTIDE_DbgAep' + TAB + '; TELL US WHAT WE ACTUALLY RECEIVE' + NL +
             TAB + 'cmp' + TAB + 'si, AEP_UNCONFIG_DCB' + TAB
             + '  ; a DCB we may hold a pointer to?' + NL +
             TAB + 'jne' + TAB + 'pa_check_pend' + NL +
             TAB + 'mov' + TAB + 'esi, [ebx].AEP_d_u_dcb' + NL +
             TAB + 'call' + TAB + 'XTIDE_ForgetDcb' + TAB + '; drop it before IOS frees it' + NL +
             TAB + 'LeaveProc' + NL +
             TAB + 'Return' + NL +
             NL +
             'pa_check_pend:' + NL +
             ';  AEP_PEND_UNCONFIG_DCB is a COMMAND, and it is the FIRST thing' + NL +
             ';  IOS sends when a DCB is being destroyed. STORAGE.DOC: "layer' + NL +
             ';  drivers are expected to stop and prevent all further input and' + NL +
             ';  output to the device". We answered SUCCESS and kept serving,' + NL +
             ';  so C: never quiesced, its VRP was never destroyed, the thread' + NL +
             ';  holding it never exited, and VMM span waiting for the System VM' + NL +
             ';  thread list to empty. Measured 2026-09-04: 4 VRPs created, 3' + NL +
             ';  destroyed, and the orphan is drive 2 - our own claimed disk.' + NL +
             ';' + NL +
             ';  It must also be ANSWERED. Falling through to pa_note_only gave' + NL +
             ';  code 21 AEP_FAILURE - refusing permission to unconfigure - and' + NL +
             ';  IOS then abandoned the teardown of our DCB: A: completed' + NL +
             ';  21/16/19/4, ours stopped dead after 21. AEP_SUCCESS is preset' + NL +
             ';  at the top of this routine, so returning here answers yes.' + NL +
             TAB + 'cmp' + TAB + 'si, AEP_PEND_UNCONFIG_DCB' + NL +
             TAB + 'jne' + TAB + 'pa_not_pend_unconfig' + NL +
             TAB + 'mov' + TAB + 'esi, [ebx].AEP_d_u_p_dcb' + NL +
             TAB + 'call' + TAB + 'XTIDE_PendUnconfig' + TAB + '; quiesce, if it is ours' + NL +
             TAB + 'LeaveProc' + NL +
             TAB + 'Return' + NL +
             NL +
             'pa_not_pend_unconfig:' + NL +
             'pa_not_unconfig:' + NL +
             ';  THE TEARDOWN WE NEVER JOINED. Windows runs a shutdown that' + NL +
             ';  mirrors its boot and broadcasts it; we registered for the boot' + NL +
             ';  half (SYS_DYNAMIC_DEVICE_INIT, AEP_INITIALIZE, AEP_CONFIG_DCB)' + NL +
             ';  and answered nothing on the way down. We publish a logical DCB' + NL +
             ';  and link it into the physical DCB chain; nothing ever removed' + NL +
             ';  it, and IFSMGR walks that chain at System_Exit.' + NL +
             TAB + 'cmp' + TAB + 'si, AEP_SYSTEM_SHUTDOWN' + NL +
             TAB + 'jne' + TAB + 'pa_note_only' + NL +
             TAB + 'call' + TAB + 'XTIDE_ShutdownArm' + TAB + '; gate: stop waiting 0.6 s per dead request' + NL +
             TAB + 'call' + TAB + 'XTIDE_VolDown' + TAB + '; destroy and UNLINK our volume' + NL +
             TAB + 'LeaveProc' + NL +
             TAB + 'Return' + NL +
             NL +
             'pa_not_shutdown:' + NL +
             ';  AEP_UNINITIALIZE (15) is a COMMAND. The sample answers it' + NL +
             ';  AEP_SUCCESS from the catch-all below and releases nothing.' + NL +
             TAB + 'cmp' + TAB + 'si, AEP_UNINITIALIZE' + NL +
             TAB + 'jne' + TAB + 'pa_note_only' + NL +
             TAB + 'call' + TAB + 'XTIDE_Uninit' + TAB + '; really release the DDB' + NL +
             TAB + 'LeaveProc' + NL +
             TAB + 'Return' + NL +
             NL +
             'pa_note_only:' + NL +
             ';  MATCH zikolas/cfu1-win9x, a port driver that shuts down' + NL +
             ';  cleanly: acknowledge 16..19 (DCB_LOCK / MOUNT_NOTIFY /' + NL +
             ';  CREATE_VRP / DESTROY_VRP) and answer AEP_FAILURE to every' + NL +
             ';  other code we do not implement. Answering AEP_SUCCESS to a' + NL +
             ';  command we did not carry out is a claim, not a courtesy.' + NL +
             TAB + 'cmp' + TAB + 'si, 16' + NL +
             TAB + 'jb' + TAB + 'pa_unimpl' + NL +
             TAB + 'cmp' + TAB + 'si, 19' + NL +
             TAB + 'jbe' + TAB + 'pa_ack' + NL +
             'pa_unimpl:' + NL +
             TAB + 'mov' + TAB + '[ebx.AEP_result], AEP_FAILURE' + NL +
             'pa_ack:' + NL +
             ';  Everything else is a NOTIFICATION - shutdown, mount, VRP create,' + NL +
             ';  associate. AEP_SUCCESS is already preset at the top of this' + NL +
             ';  routine and is the right answer to all of them. The sample' + NL +
             ';  overwrote it with AEP_FAILURE, which is how a driver reports its' + NL +
             ';  own error, and it did so for every code it did not dispatch -' + NL +
             ';  including all four that Windows sends on the way down.' + NL),
         0),
    ])

    if nocalldown:
        total += edit(aer, [
            ('skip calldown insert',
             r'(' + NL + r'Port_cfg_insert:)',
             lambda m: NL + TAB + 'jmp' + TAB + 'vcd_ret' + TAB + '; BISECT: no calldown insert'
                       + m.group(1),
             0),
        ])
        print('BISECT: calldown insert skipped')

    # ---- PORT.ASM : declare what layer we actually are ---------------------
    #
    # DRP_LGN is the declaration. IOS orders the calldown chain - and its
    # teardown - by load group. The sample ships DRP_MISC_PD and calls itself
    # 'Generic Port Drv'; read off the binaries on this machine's own card:
    #
    #   ESDI_506.PDR   DRP_ESDI_PD    (bit 16h)   'ESDI port driver'
    #   SCSIPORT.PDR   DRP_NT_PD      (bit 15h)   'SCSIPORT'
    #   HSFLOP.PDR     DRP_NEC_FLOPPY (bit 1Bh)   'NEC Floppy NEC'
    #   ours           DRP_MISC_PD    (bit 13h)   <- the odd one out
    #
    # We are an ATA port driver and ESDI_506 is Win95's own IDE/ESDI port
    # driver, so ESDI_PD is the honest group. DRP_bus_type is already
    # DRP_BT_ESDI and matches ESDI_506 exactly; only the layer differed.
    prt = os.path.join(out, 'PORT.ASM')
    total += edit(prt, [
        # Anchor on the DRP initialiser itself. The file mentions DRP_MISC_PD
        # in a comment first, and edit() replaces one occurrence - a bare
        # pattern silently rewrites the comment and leaves the declaration.
        ('load group',
         r'(DRP\s*<\s*EyeCatcher\s*,\s*)DRP_MISC_PD',
         lambda m: m.group(1) + 'DRP_ESDI_PD',
         0),
    ])
    print('load group: DRP_MISC_PD -> DRP_ESDI_PD')

    # ---- PORTREQ.ASM : service the request instead of dropping it ----------
    req = os.path.join(out, 'PORTREQ.ASM')
    total += edit(req, [
        ('XTIDE_StartRequest extern',
         r'(VXD_LOCKED_CODE_SEG\s*\n)',
         lambda m: m.group(1) + NL + TAB + 'extrn' + TAB + 'XTIDE_StartRequest:near' + NL
                   + TAB + 'extrn' + TAB + 'XTIDE_WantIop:near' + NL,
         0),

        # Never drive the hardware for a DCB we did not insert a calldown on.
        # Measured 2026-09-01: one insert, on one DCB, and a request still
        # arrived carrying a different DCB - unit 0, the boot disk. Refusing
        # costs a failed request; obeying costs the volume. Port_r_not_io
        # completes it through the normal callback unwind.
        ('refuse a foreign DCB',
         r'(' + WS + r'*mov' + WS + r'+esi,' + WS + r'*\[ebx\.IOP_physical_dcb\][^\n]*\n)',
         lambda m: m.group(1) + TAB + 'call' + TAB + 'XTIDE_WantIop' + TAB
                   + '; ours, or somebody else\'s?' + NL
                   + TAB + 'or' + TAB + 'eax, eax' + NL
                   + TAB + 'jnz' + TAB + 'Port_r_not_io' + NL,
         0),

        ('start-hardware hook',
         WS + r'*add' + WS + r'+ebx,' + WS + r'*ecx[^\n]*\n.*?;' + WS
         + r'*call' + WS + r'+Port_Start_Hardware' + WS + r'*\r?\n',
         (';  The IOP itself is what we need, not an expansion area we did not ask' + NL +
          ';  for. Synchronous and polled: the transfer happens inside this call and' + NL +
          ';  the IOP is completed before it returns, so there is no ISR to come back' + NL +
          ';  through - the card is jumpered without an interrupt.' + NL +
          TAB + 'call' + TAB + 'XTIDE_StartRequest' + TAB + '; EBX = IOP, ESI = DCB' + NL +
          TAB + 'jmp' + TAB + 'Port_Start_Request' + TAB + '; drain the rest of the queue' + NL),
         re.S),

        ('release the active flag when the queue empties',
         r'(' + WS + r'*or' + WS + r'+eax,' + WS + r'*eax[^\n]*\n' + WS + r'*)jz'
         + WS + r'+Port_r_exit',
         lambda m: m.group(1) + 'jz' + TAB + 'Port_r_idle',
         0),

        # Port_request is the calldown entry IOS calls. Its own header says it
        # "can only destroy eax, ecx, edx"; it destroys EBX, ESI and EDI.
        # Microsoft's contract, broken by Microsoft's sample - invisible while
        # the sample never got called. zikolas/cfu1-win9x saves all three.
        # Port_Start_Request becomes a called subroutine so PORTISR's own call
        # into it stays balanced.
        ('save callee-saved registers',
         r'(' + WS + r'*EnterProc' + WS + r'*\r?\n)',
         lambda m: m.group(1) + TAB + 'push' + TAB + 'ebx' + NL
                   + TAB + 'push' + TAB + 'esi' + NL
                   + TAB + 'push' + TAB + 'edi' + NL,
         0),

        # THE ARGUMENT IS NOT WHERE THE SAMPLE THINKS IT IS.
        # ArgVar/EnterProc resolve IOP_Ptr to [ebp+8], but VMM.INC's EnterProc
        # only emits "push ebp / mov ebp,esp" when ??_pf_ArgsUsed is ALREADY
        # set - and that flag is set by referencing the ArgVar, which happens on
        # the next line. Outside a DEBUG build the frame is never built, so the
        # sample reads its argument through whatever EBP the caller left in the
        # register. Confirmed from the listing: the proc's first three bytes are
        # our own pushes, and there is no prologue.
        # Read it off the stack instead. Entry layout at this point:
        #   [esp]=edi [esp+4]=esi [esp+8]=ebx [esp+12]=return [esp+16]=the IOP.
        ('argument is ESP-relative - there is no frame',
         r'(' + WS + r'*)mov' + WS + r'+ebx,' + WS + r'+IOP_Ptr[^\n]*\n',
         lambda m: TAB + 'mov' + TAB + 'ebx, [esp+16]' + TAB
                   + '; the IOP. NOT IOP_Ptr - see patch_sample.py' + NL,
         0),

        ('busy exit takes the restoring path',
         WS + r'*jc' + WS + r'+Port_r_exit' + WS + r'*\r?\n',
         TAB + 'jc' + TAB + 'Port_rq_ret' + NL,
         0),

        ('invalid-command exit takes the restoring path',
         WS + r'*jmp' + WS + r'+Port_r_exit' + WS + r'*\r?\n',
         TAB + 'jmp' + TAB + 'Port_rq_ret' + NL,
         0),

        ('call the queue drain, then restore',
         r'(\n' + WS + r'*public' + WS + r'+Port_Start_Request)',
         lambda m: (NL + TAB + 'call' + TAB + 'Port_Start_Request' + TAB
                    + '; drain the queue, then unwind' + NL +
                    'Port_rq_ret:' + NL +
                    TAB + 'pop' + TAB + 'edi' + NL +
                    TAB + 'pop' + TAB + 'esi' + NL +
                    TAB + 'pop' + TAB + 'ebx' + NL +
                    TAB + 'LeaveProc' + NL +
                    TAB + 'Return' + NL + m.group(1)),
         0),

        ('queue drain returns to its caller',
         r'\nPort_r_exit:' + WS + r'*\r?\n' + WS + r'*LeaveProc' + WS + r'*\r?\n'
         + WS + r'*Return' + WS + r'*\r?\n',
         NL + 'Port_r_exit:' + NL + TAB + 'ret' + NL,
         0),

        ('idle label',
         r'(\nPort_r_exit:)',
         (NL + ';  Nothing left queued. The sample never cleared this, because it never' + NL +
          ';  completed anything - leave it set and every later request queues behind a' + NL +
          ';  driver that believes it is still busy.' + NL +
          'Port_r_idle:' + NL +
          TAB + 'btr' + TAB + '[edi].DDB_port_flags, DDB_BF_ACTIVE_BIT' + NL +
          NL + 'Port_r_exit:'),
         0),
    ])

    # Publishing the volume is no longer optional. Without it the driver claims
    # a device and services requests that nobody ever issues, because a
    # dynamically registered port driver gets no disk TSD (technique 83) - so
    # the switch only ever selected between "works" and "does nothing visible".
    total += edit(aer, [
        # NOT at AEP_BOOT_COMPLETE - measured 2026-09-01, it never arrives.
        # A dynamically registered driver gets its own AEP_CONFIG_DCB and
        # nothing else - which is the same reason no disk TSD ever engages our
        # DCB. Everything the volume needs (physical DCB, DDB, ILB, load group,
        # a working read path) exists by the time the insert returns, so there
        # was never a reason to wait.
        # Placed BEFORE vcd_ret, so the guard's own "jz/jnz vcd_ret" bail-outs
        # skip it: a DCB we declined never gets a volume built on it.
        # --novolume is a BISECT, not a feature: insert the calldown but never
        # publish a volume. Splits "the request path hangs shutdown" from "the
        # published volume does". Emits a comment rather than skipping the edit
        # so the anchor still matches exactly once and the edit count is
        # unchanged - a bisect switch that changes the assertion arithmetic is
        # one more thing to get wrong mid-investigation.
        ('publish the volume once the calldown is in',
         r'(\nvcd_ret:)',
         lambda m: (NL + TAB + '; BISECT: volume publish skipped (--novolume)' + NL
                    if novolume else
                    NL + TAB + 'call' + TAB + 'XTIDE_SchedVol' + TAB
                    + '; be our own TSD - nobody else will' + NL) + m.group(1),
         0),
    ])

    if '--reqmarker' in sys.argv:
        total += edit(req, [
            ('MarkEntry extern',
             r'(' + WS + r'*extrn' + WS + r'+XTIDE_StartRequest:near' + WS + r'*\r?\n)',
             lambda m: m.group(1) + TAB + 'extrn' + TAB + 'XTIDE_MarkEntry:near' + NL,
             0),
            ('MarkEntry call',
             r'(' + WS + r'*mov' + WS + r'+ebx,' + WS + r'*\[esp\+16\][^\n]*\n)',
             lambda m: m.group(1) + TAB + 'call' + TAB + 'XTIDE_MarkEntry' + TAB
                       + '; DIAGNOSTIC: IOS called our calldown' + NL,
             0),
        ])
        print('DIAGNOSTIC: calldown-entry marker wired')


        # Report Port_cfg_device itself. Three outcomes in one run: no marker at
        # all = AEP_CONFIG_DCB never reached us; cfg>0 want=0 = it did and we
        # declined every DCB; want>0 isp!=0 = IOS refused the calldown insert;
        # want>0 isp=0 = the insert took and the silence is above us.
        total += edit(aer, [
            ('MarkConfig extern',
             r'(' + WS + r'*extrn' + WS + r'+XTIDE_ConfigDcb:near[^\n]*\n)',
             lambda m: m.group(1) + TAB + 'extrn' + TAB + 'XTIDE_MarkConfig:near' + NL
                       + TAB + 'extrn' + TAB + 'XTIDE_NoteIsp:near' + NL,
             0),
            ('capture ISP_result',
             r'(' + WS + r'*call' + WS + r'+\[Port_ilb\.ILB_Service_rtn\][^\n]*\n)',
             lambda m: m.group(1) + TAB + 'movzx' + TAB + 'eax, word ptr [esp+6]' + TAB
                       + '; ISP_result' + NL
                       + TAB + 'call' + TAB + 'XTIDE_NoteIsp' + NL,
             0),
            ('report on the way out',
             r'(\nvcd_ret:' + WS + r'*\r?\n)',
             lambda m: m.group(1) + NL + TAB + 'call' + TAB + 'XTIDE_MarkConfig' + NL,
             0),
        ])
        print('DIAGNOSTIC: config-path marker wired')

    print('Patched: %d' % total)
    want = 23 if nocalldown else 22	# +1: the DRP load group in PORT.ASM
    if '--reqmarker' in sys.argv:
        want += 5
    assert total == want, 'expected %d edits, got %d' % (want, total)


main()
