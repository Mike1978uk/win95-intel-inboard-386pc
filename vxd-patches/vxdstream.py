"""Linear sweep that understands the Win9x VxD calling convention:
   CD 20 <dword service id>   (INT 20h + inline DWORD) = 6 bytes, not an INT.
Without this, every disassembler desyncs after each VxD service call."""
import struct, capstone
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)

def sweep(buf, start=0):
    """yield (offset, size, mnemonic, op_str) over the true instruction stream"""
    pc=start; n=len(buf)
    while pc < n-1:
        if buf[pc]==0xCD and buf[pc+1]==0x20:
            sid=struct.unpack_from('<I',buf,pc+2)[0] if pc+6<=n else 0
            yield (pc,6,"VxDCall",f"0x{sid:08x}")
            pc+=6; continue
        ins=next(md.disasm(bytes(buf[pc:pc+16]),pc,1),None)
        if ins is None:
            pc+=1; continue
        yield (pc,ins.size,ins.mnemonic,ins.op_str)
        pc+=ins.size

def le_objects(path):
    d=open(path,'rb').read()
    h=struct.unpack_from('<I',d,0x3c)[0]
    n=struct.unpack_from('<I',d,h+68)[0]
    ot=struct.unpack_from('<I',d,h+64)[0]+h
    opt=struct.unpack_from('<I',d,h+72)[0]+h
    dp=struct.unpack_from('<I',d,h+128)[0]
    ps=struct.unpack_from('<I',d,h+40)[0]
    objs=[]
    for i in range(n):
        vsize,base,flags,pidx,npages,_=struct.unpack_from('<IIIIII',d,ot+i*24)
        objs.append(dict(n=i+1,vsize=vsize,flags=flags,pidx=pidx,npages=npages))
    return d,objs,opt,dp,ps

def file_off(d,opt,dp,ps,o,off):
    pt=opt+(o['pidx']-1+off//ps)*4
    pnum=(d[pt]<<16)|(d[pt+1]<<8)|d[pt+2]
    return dp+(pnum-1)*ps+off%ps

def true_boundaries(path):
    """file_offset -> (objnum, obj_off, mnemonic, size, ops)"""
    d,objs,opt,dp,ps=le_objects(path)
    out={}
    for o in objs:
        if not (o['flags'] & 0x4): continue      # EXECUTABLE
        buf=bytearray(); fmap=[]
        for off in range(o['vsize']):
            fo=file_off(d,opt,dp,ps,o,off); buf.append(d[fo]); fmap.append(fo)
        for off,size,mn,ops in sweep(buf):
            out[fmap[off]]=(o['n'],off,mn,size,ops)
    return out

# ---------------------------------------------------------------------------
# Site verification for the patch scripts.
#
# 2026-08-23: this module exists because the previous `realign()` helper in
# patch_vdmad.py / patch_vpicd.py accepted ANY start alignment that happened to
# decode an IN/OUT at the target offset. Fed `80 E4 C0` (AND AH,0C0h), it
# "found" an `IN AL,0C0h` at target+1, overwrote 2 bytes with B0 00, and welded
# the remains into a 7-byte `XOR byte ptr [EAX+7420A800h], 6`. That shipped
# inside the card's VMM32.VXD and is the exact cause of
#   "fatal exception 0E at 0028:C002F330 in VXD VDMAD(01) + 00001660".
# Never verify an instruction by re-disassembling from a guessed start. Decode
# the object's real stream once, VxD calling convention included, and require
# the candidate to sit on a boundary in it.
# ---------------------------------------------------------------------------

def verify_sites(path, candidates):
    """candidates: iterable of file offsets. Returns (good, bad) where good is a
    list of (offset, mnemonic, op_str, size) sitting on TRUE instruction
    boundaries and bad is a list of (offset, reason)."""
    b = true_boundaries(path)
    good, bad = [], []
    for off in candidates:
        info = b.get(off)
        if info and info[2] in ("in", "out") and info[3] == 2:
            good.append((off, info[2], info[4], info[3]))
            continue
        owner = None
        for fo, (n, oo, mn, sz, ops) in b.items():
            if fo < off < fo + sz:
                owner = (fo, n, oo, mn, sz, ops)
        if owner:
            fo, n, oo, mn, sz, ops = owner
            bad.append((off, f"inside OBJ{n}:0x{oo:x} {mn} {ops} ({sz}B at file 0x{fo:x})"))
        elif info:
            bad.append((off, f"boundary but not a 2-byte IN/OUT: {info[2]} {info[4]} ({info[3]}B)"))
        else:
            bad.append((off, "not on the instruction stream of any executable object"))
    return good, bad
