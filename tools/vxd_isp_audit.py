"""Verify candidate ISP call sites instead of trusting a byte scan.

A real ISP request is: store ISP_func into the packet, then push the packet and
call indirectly through ILB_service_rtn.  So a candidate is only credible if an
indirect CALL follows it within a short window.  A bare `mov word [reg+d], imm`
that never reaches a call is a field store in some other structure - which is
exactly how a DCB field write at +68h was misread as ISP_DISASSOCIATE_DCB.

Handles the VxD trap: CD 20 is followed by an inline 4-byte service id.
"""
import re
import struct
import sys

from capstone import CS_ARCH_X86, CS_MODE_32, Cs

NAMES = {0: 'CREATE_DDB', 1: 'CREATE_DCB', 2: 'CREATE_IOP', 3: 'ALLOC_MEM',
         4: 'DEALLOC_MEM', 5: 'INSERT_CALLDOWN', 6: 'ASSOCIATE_DCB', 7: 'GET_DCB',
         8: 'GET_FIRST_NEXT_DCB', 9: 'DEALLOC_DDB', 10: 'DESTROY_DCB',
         11: 'QUERY_MATCHING_DCBS', 12: 'QUERY_REMOVE_DCB', 13: 'DEVICE_REMOVED',
         14: 'DEVICE_ARRIVED', 15: 'DISASSOCIATE_DCB', 16: 'DRIVE_LETTER_PICK',
         17: 'REGISTRY_READ', 18: 'FIND_LDM_ENTRY', 19: 'DELETE_LDM_ENTRY',
         20: 'BROADCAST_AEP'}

# ISP_func is at offset 0 of the packet, so ONLY a zero-displacement store can
# be one. Allowing any disp8 is what let a DCB field write at [edi+68h] read as
# ISP_DISASSOCIATE_DCB (0Fh). mod=00 forms, plus the explicit +0 encodings.
PATS = [rb'\x66\xc7[\x00-\x03\x06\x07]([\x00-\x14])\x00',      # [eax] [ecx] [edx] [ebx] [esi] [edi]
        rb'\x66\xc7[\x40-\x47]\x00([\x00-\x14])\x00',          # [reg+0]
        rb'\x66\xc7\x04\x24([\x00-\x14])\x00',                 # [esp]
        rb'\x66\xc7\x44\x24\x00([\x00-\x14])\x00']             # [esp+0]

md = Cs(CS_ARCH_X86, CS_MODE_32)


def follows_with_call(d, off, max_ins=40, max_bytes=160):
    """Walk forward from off; True if an indirect call appears before a ret."""
    p = off
    end = min(len(d), off + max_bytes)
    for _ in range(max_ins):
        if p >= end:
            return None
        if d[p] == 0xCD and p + 6 <= len(d) and d[p + 1] == 0x20:
            p += 6
            continue
        ins = next(md.disasm(d[p:p + 16], p), None)
        if ins is None:
            return None
        if ins.mnemonic == 'call' and 'ptr [' in ins.op_str:
            return p - off
        if ins.mnemonic in ('ret', 'retf'):
            return None
        p += ins.size
    return None


for path in sys.argv[1:]:
    d = open(path, 'rb').read()
    name = path.replace('\\', '/').split('/')[-1]
    seen = {}
    for pat in PATS:
        for m in re.finditer(pat, d, re.S):
            off = m.start()
            code = m.group(1)[0]
            dist = follows_with_call(d, off)
            if dist is not None:
                seen.setdefault(code, []).append((off, dist))
    print('== %s' % name)
    if not seen:
        print('   (no candidate reached an indirect call)')
    for c in sorted(seen):
        sites = ', '.join('0x%05X(+%d)' % (o, dd) for o, dd in seen[c])
        print('   ISP_%-20s x%d   %s' % (NAMES.get(c, '?%d' % c), len(seen[c]), sites))
