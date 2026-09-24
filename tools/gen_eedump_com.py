"""Emit EEDUMP.COM: read all 64 words of a 3C509B's EEPROM through the ID port.

Uses the ID-port contention read (3C5x9B Technical Reference, ch. 7): after the
ID sequence the card is in ID_CMD, command 80h|n loads EEPROM word n into the
data register, and each read of the ID port returns its next bit, MSB first, in
bit 0. The card is never activated and nothing is written to the EEPROM. The
final 00h returns the card to ID_WAIT, where a driver load will find it.

Writes EEPROM.BIN (128 bytes, little-endian words) in the current directory.
Exit code 1 if the file could not be created.

    python tools/gen_eedump_com.py --out EEDUMP.COM
"""
import argparse
import struct

ID_PORT = 0x110           # this machine's packet driver uses 0110h
ORG     = 0x100


def build():
    code = bytearray()

    def here():
        return ORG + len(code)

    code += bytes([0xBA]) + struct.pack("<H", ID_PORT)   # mov dx,ID_PORT
    code += bytes([0x30, 0xC0])                           # xor al,al
    code += bytes([0xEE, 0xEE])                           # out dx,al x2: select, reset
    code += bytes([0xB9, 0xFF, 0x00])                     # mov cx,0FFh
    code += bytes([0xB0, 0xFF])                           # mov al,0FFh
    seq = here()
    code += bytes([0xEE])                                 # out dx,al
    code += bytes([0xD0, 0xE0])                           # shl al,1
    code += bytes([0x73, 0x02])                           # jnc +2
    code += bytes([0x34, 0xCF])                           # xor al,0CFh
    code += bytes([0xE2, (seq - (here() + 2)) & 0xFF])    # loop seq

    buf_fix = len(code) + 1
    code += bytes([0xBF, 0, 0])                           # mov di,BUF
    code += bytes([0x31, 0xDB])                           # xor bx,bx
    rd = here()
    code += bytes([0x88, 0xD8])                           # mov al,bl
    code += bytes([0x0C, 0x80])                           # or al,80h
    code += bytes([0xEE])                                 # out dx,al  (read word n)
    # The EEPROM needs 162 us; 200 reads of the PPI take ~1 ms on the 5160.
    code += bytes([0xB9, 0xC8, 0x00])                     # mov cx,200
    dly = here()
    code += bytes([0xE4, 0x61])                           # in al,61h
    code += bytes([0xE2, (dly - (here() + 2)) & 0xFF])    # loop dly
    code += bytes([0xB9, 0x10, 0x00])                     # mov cx,16
    code += bytes([0x31, 0xF6])                           # xor si,si
    bit = here()
    code += bytes([0xEC])                                 # in al,dx
    code += bytes([0xD1, 0xE6])                           # shl si,1
    code += bytes([0x25, 0x01, 0x00])                     # and ax,1
    code += bytes([0x09, 0xC6])                           # or si,ax
    code += bytes([0xE2, (bit - (here() + 2)) & 0xFF])    # loop bit
    code += bytes([0x89, 0x35])                           # mov [di],si
    code += bytes([0x47, 0x47])                           # inc di x2
    code += bytes([0x43])                                 # inc bx
    code += bytes([0x83, 0xFB, 0x40])                     # cmp bx,64
    code += bytes([0x72, (rd - (here() + 2)) & 0xFF])     # jb rd

    code += bytes([0x30, 0xC0])                           # xor al,al
    code += bytes([0xEE])                                 # out dx,al  (back to ID_WAIT)

    code += bytes([0xB4, 0x3C])                           # mov ah,3Ch
    code += bytes([0x31, 0xC9])                           # xor cx,cx
    name_fix = len(code) + 1
    code += bytes([0xBA, 0, 0])                           # mov dx,FNAME
    code += bytes([0xCD, 0x21])                           # int 21h
    jc_at = len(code)
    code += bytes([0x72, 0])                              # jc err
    code += bytes([0x89, 0xC3])                           # mov bx,ax
    code += bytes([0xB4, 0x40])                           # mov ah,40h
    code += bytes([0xB9, 0x80, 0x00])                     # mov cx,128
    buf_fix2 = len(code) + 1
    code += bytes([0xBA, 0, 0])                           # mov dx,BUF
    code += bytes([0xCD, 0x21])                           # int 21h
    code += bytes([0xB4, 0x3E])                           # mov ah,3Eh
    code += bytes([0xCD, 0x21])                           # int 21h
    code += bytes([0xB8, 0x00, 0x4C])                     # mov ax,4C00h
    code += bytes([0xCD, 0x21])                           # int 21h
    err = len(code)
    code += bytes([0xB8, 0x01, 0x4C])                     # mov ax,4C01h
    code += bytes([0xCD, 0x21])                           # int 21h
    code[jc_at + 1] = err - (jc_at + 2)

    fname = ORG + len(code)
    code += b"EEPROM.BIN\x00"
    buf = ORG + len(code)
    code += bytes(128)

    struct.pack_into("<H", code, buf_fix, buf)
    struct.pack_into("<H", code, buf_fix2, buf)
    struct.pack_into("<H", code, name_fix, fname)
    return bytes(code)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="EEDUMP.COM")
    a = ap.parse_args()
    data = build()
    open(a.out, "wb").write(data)
    print(f"{a.out}: {len(data)} bytes")
