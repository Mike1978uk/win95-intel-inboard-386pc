import sys,struct,capstone
f=open(sys.argv[1],'rb').read()
pe=struct.unpack_from('<I',f,0x3c)[0]; ns=struct.unpack_from('<H',f,pe+6)[0]; osz=struct.unpack_from('<H',f,pe+20)[0]
sec=[]
o=pe+24+osz
for i in range(ns):
    n,vs,va,rs,ro=struct.unpack_from('<8sIIII',f,o+40*i); sec.append((n.rstrip(b'\0'),va,vs,ro,rs))
def rd(rva,n):
    for nm,va,vs,ro,rs in sec:
        if va<=rva<va+max(vs,rs): return f[ro+rva-va:ro+rva-va+n]
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32)
if sys.argv[2]=='sec':
    for s in sec: print(s[0],hex(s[1]),hex(s[2]))
    # exports
    ed=struct.unpack_from('<I',f,pe+24+96)[0]
    if ed:
        d=rd(ed,40); nf,nn,af,an,ao=struct.unpack_from('<IIIII',d,20)
        names=[]
        for i in range(nn):
            np=struct.unpack('<I',rd(an+4*i,4))[0]; nm=rd(np,64).split(b'\0')[0].decode()
            oi=struct.unpack('<H',rd(ao+2*i,2))[0]; fa=struct.unpack('<I',rd(af+4*oi,4))[0]; names.append((fa,nm))
        for fa,nm in sorted(names): print(hex(fa),nm)
    sys.exit()
a=int(sys.argv[2],16); n=int(sys.argv[3],16)
for i in md.disasm(rd(a,n),a): print(f'{i.address:6x}: {i.mnemonic} {i.op_str}')
