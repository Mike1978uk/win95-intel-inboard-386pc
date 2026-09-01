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
    total = 0

    # ---- PORTAER.ASM ------------------------------------------------------
    aer = os.path.join(out, 'PORTAER.ASM')
    total += edit(aer, [
        ('XTIDE_Inquiry extern',
         r'(extrn' + WS + r'+XTIDE_Probe:near[^\n]*\n)',
         lambda m: m.group(1) + TAB + 'extrn' + TAB + 'XTIDE_Inquiry:near' + TAB
                   + '; device inquiry (phase 2c)' + NL,
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

    if nocalldown:
        total += edit(aer, [
            ('skip calldown insert',
             r'(' + NL + r'Port_cfg_insert:)',
             lambda m: NL + TAB + 'jmp' + TAB + 'vcd_ret' + TAB + '; BISECT: no calldown insert'
                       + m.group(1),
             0),
        ])
        print('BISECT: calldown insert skipped')

    # ---- PORTREQ.ASM : service the request instead of dropping it ----------
    req = os.path.join(out, 'PORTREQ.ASM')
    total += edit(req, [
        ('XTIDE_StartRequest extern',
         r'(VXD_LOCKED_CODE_SEG\s*\n)',
         lambda m: m.group(1) + NL + TAB + 'extrn' + TAB + 'XTIDE_StartRequest:near' + NL,
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

    print('Patched: %d' % total)
    want = 9 if nocalldown else 8
    assert total == want, 'expected %d edits, got %d' % (want, total)


main()
