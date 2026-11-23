#!/usr/bin/env python3
"""Disassemble a 32-bit LE VxD whole: every object loaded at a synthetic base
(object n at n * 0x100000), internal fixups applied, VxD service calls named.

  python tools/vxddis.py <file.vxd> [out.txt]

`int 20h` is followed by a service dword (device id << 16 | service), which a
plain sweep would decode as instructions; it is printed as VxDCall/VMMCall
instead. Fixup targets in other objects show as their synthetic address, so a
reference reads as object:offset. Only internal-reference fixups are applied.
"""
import struct
import sys

from capstone import Cs, CS_ARCH_X86, CS_MODE_32

DEVICES = {
    0x0001: 'VMM', 0x0002: 'Debug', 0x0003: 'VPICD', 0x0004: 'VDMAD', 0x0005: 'VTD',
    0x0006: 'V86MMGR', 0x0007: 'PageSwap', 0x000A: 'VDD', 0x000C: 'VMD', 0x000D: 'VKD',
    0x0015: 'DOSMGR', 0x0017: 'SHELL', 0x0026: 'VPOWERD', 0x0027: 'VXDLDR',
    0x0033: 'CONFIGMG', 0x0034: 'DWCFGMG', 0x0436: 'VSHARE', 0x357E: 'DSOUND',
    0x0440: 'VSBD?',
}
VPICD = {0: 'Get_Version', 1: 'Virtualize_IRQ', 2: 'Set_Int_Request', 3: 'Clear_Int_Request',
         4: 'Phys_EOI', 5: 'Get_Complete_Status', 6: 'Get_Status', 7: 'Test_Phys_Request',
         8: 'Physically_Mask', 9: 'Physically_Unmask', 10: 'Set_Auto_Masking',
         11: 'Get_IRQ_Complete_Status', 12: 'Convert_Handle_To_IRQ', 13: 'Convert_IRQ_To_Int',
         14: 'Convert_Int_To_IRQ', 15: 'Call_When_Hw_Int', 16: 'Force_Default_Owner',
         17: 'Force_Default_Behavior', 18: 'Auto_Mask_At_Inst_Swap', 19: 'Begin_Inst_Page_Swap',
         20: 'End_Inst_Page_Swap', 21: 'Virtual_EOI', 22: 'Get_Virtualization_Count',
         23: 'Post_Sys_Critical_Init', 24: 'VM_SlavePIC_Mask_Change'}
VDMAD = {0: 'Get_Version', 1: 'Lock_DMA_Region', 2: 'Unlock_DMA_Region', 3: 'Virtualize_Channel',
         4: 'Get_Region_Info', 5: 'Set_Region_Info', 6: 'Get_Virt_State', 7: 'Set_Virt_State',
         8: 'Set_Phys_State', 9: 'Mask_Channel', 10: 'UnMask_Channel', 11: 'Default_Handler',
         12: 'Scatter_Lock', 13: 'Scatter_Unlock', 14: 'Reserve_Buffer_Space',
         15: 'Request_Buffer', 16: 'Release_Buffer', 17: 'Copy_To_Buffer', 18: 'Copy_From_Buffer',
         19: 'Get_EISA_Adr_Mode', 20: 'Set_EISA_Adr_Mode', 21: 'Unlock_DMA_Region_No_Dirty',
         22: 'Phys_Mask_Channel', 23: 'Phys_Unmask_Channel', 24: 'Unvirtualize_Channel',
         25: 'Set_IO_Address', 26: 'Get_Phys_Count', 27: 'Get_Phys_Status',
         28: 'Get_Max_Phys_Page', 29: 'Set_Channel_Callbacks', 30: 'Get_Virt_Count',
         31: 'Set_Virt_Count'}
NAMES = {0x0003: VPICD, 0x0004: VDMAD}


def load(path):
    d = open(path, 'rb').read()
    le = struct.unpack_from('<I', d, 0x3C)[0]
    h = lambda off, f: struct.unpack_from(f, d, le + off)[0]
    pgsize, nobj = h(0x28, 'I'), h(0x44, 'I')
    objtab, pagemap = le + h(0x40, 'I'), le + h(0x48, 'I')
    fpt, frt = le + h(0x68, 'I'), le + h(0x6C, 'I')
    datapages, npages = h(0x80, 'I'), h(0x14, 'I')
    lastpg = h(0x2C, 'I')
    eip_obj, eip = h(0x18, 'I'), h(0x1C, 'I')
    objs = []
    for i in range(nobj):
        vsize, base, flags, pmi, pmn, _ = struct.unpack_from('<6I', d, objtab + i * 24)
        objs.append(dict(vsize=vsize, flags=flags, pmi=pmi, pmn=pmn, base=(i + 1) << 20))
    pages = {}
    for o in objs:
        img = bytearray(max(o['vsize'], o['pmn'] * pgsize))
        for k in range(o['pmn']):
            pg = o['pmi'] + k                      # 1-based page number
            ent = pagemap + (pg - 1) * 4
            num = (d[ent] << 16) | (d[ent + 1] << 8) | d[ent + 2]
            ln = lastpg if num == npages else pgsize
            src = datapages + (num - 1) * pgsize
            img[k * pgsize:k * pgsize + ln] = d[src:src + ln]
            pages[pg] = (o, k * pgsize)
        o['img'] = img
    # Fixups: internal references only (target flags & 3 == 0).
    for pg in range(1, npages + 1):
        if pg not in pages:
            continue
        o, pbase = pages[pg]
        a, b = struct.unpack_from('<II', d, fpt + (pg - 1) * 4)
        p = frt + a
        end = frt + b
        while p < end:
            src, flg = d[p], d[p + 1]
            p += 2
            if src & 0x20:
                cnt = d[p]; p += 1
                offs = None
            else:
                offs = [struct.unpack_from('<h', d, p)[0]]; p += 2
            if flg & 3 != 0:
                raise SystemExit('import/entry fixups not handled')
            if flg & 0x40:
                tobj = struct.unpack_from('<H', d, p)[0]; p += 2
            else:
                tobj = d[p]; p += 1
            stype = src & 0x0F
            if stype == 2:
                toff = 0
            elif flg & 0x10:
                toff = struct.unpack_from('<I', d, p)[0]; p += 4
            else:
                toff = struct.unpack_from('<H', d, p)[0]; p += 2
            if offs is None:
                offs = []
                for _ in range(cnt):
                    offs.append(struct.unpack_from('<h', d, p)[0]); p += 2
            tgt = objs[tobj - 1]['base'] + toff
            for so in offs:
                at = pbase + so
                if at < 0 or at + 4 > len(o['img']):
                    continue
                if stype == 7:
                    struct.pack_into('<I', o['img'], at, tgt)
                elif stype == 8:
                    struct.pack_into('<I', o['img'], at, (tgt - (o['base'] + at + 4)) & 0xFFFFFFFF)
    return objs, objs[eip_obj - 1]['base'] + eip if eip_obj else None


def service(dw):
    dev, svc = dw >> 16, dw & 0x7FFF
    dn = DEVICES.get(dev, '%04X' % dev)
    sn = NAMES.get(dev, {}).get(svc, '%X' % svc)
    return '%s(%s_%s)' % ('VxDJmp' if dw & 0x8000 else 'VxDCall', dn, sn)


def main():
    objs, entry = load(sys.argv[1])
    out = open(sys.argv[2], 'w') if len(sys.argv) > 2 else sys.stdout
    md = Cs(CS_ARCH_X86, CS_MODE_32)
    for n, o in enumerate(objs, 1):
        print('; object %d  base %08X  vsize %X  flags %X' % (n, o['base'], o['vsize'], o['flags']), file=out)
        if not o['flags'] & 4:                     # not executable: hex dump only
            img = o['img'][:o['vsize']]
            for i in range(0, len(img), 16):
                row = img[i:i + 16]
                print('%08X  %-48s %s' % (o['base'] + i, row.hex(' '),
                      ''.join(chr(c) if 32 <= c < 127 else '.' for c in row)), file=out)
            continue
        img = bytes(o['img'][:o['vsize']])
        pc = 0
        while pc < len(img):
            if img[pc] == 0xCD and img[pc + 1:pc + 2] == b'\x20' and pc + 6 <= len(img):
                dw = struct.unpack_from('<I', img, pc + 2)[0]
                print('%08X  %-20s %s' % (o['base'] + pc, img[pc:pc + 6].hex(), service(dw)), file=out)
                pc += 6
                continue
            ins = next(md.disasm(img[pc:pc + 16], o['base'] + pc), None)
            if ins is None:
                print('%08X  %-20s db %02Xh' % (o['base'] + pc, '%02x' % img[pc], img[pc]), file=out)
                pc += 1
                continue
            print('%08X  %-20s %s %s' % (ins.address, ins.bytes.hex(), ins.mnemonic, ins.op_str), file=out)
            pc += ins.size
    if entry:
        print('; entry %08X' % entry, file=out)


if __name__ == '__main__':
    main()
