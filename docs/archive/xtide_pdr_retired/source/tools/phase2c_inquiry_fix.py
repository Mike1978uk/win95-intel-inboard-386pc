#!/usr/bin/env python3
r"""Phase 2c fix: make AEP_DEVICE_INQUIRY do what the DDK design guide says.

NEW95DOC/STORAGE.DOC, on AEP_DEVICE_INQUIRY:

    "If the given unit exists, the driver sets the DCB_product_id,
     DCB_vendor_id and DCB_rev_level members to appropriate values and
     returns AEP_SUCCESS. ... If the given unit does not exist, the driver
     returns AEP_NO_INQ_DATA to direct the IOS to inquire about the next
     unit."

Inquiry identifies the unit. It does not publish geometry - which is what the
first attempt tried to do, through DCB_BLOCKDEV offsets applied to a DCB.

  python drivers/xtide_pdr/tools/phase2c_inquiry_fix.py [--check]
"""
import sys, os

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'XTIDETR.ASM')
TAB = chr(9)


def T(s):
    return s.replace('|', TAB)


DATA = T("""
;  Per-unit model string, de-swapped. ATA stores its text two characters per
;  16-bit word with the pair reversed, so it has to be unswapped before it is
;  fit to hand to anything that will display it.
XTIDE_UnitModel||db|80 dup (32)|; 2 units x 40 bytes, space filled
""")

COPYID = T("""
;----------------------------------------------------------------------------
; XTIDE_CopyId - de-swap this unit's 40-byte model string out of the IDENTIFY
;                buffer into XTIDE_UnitModel.
;
;   In:   EAX = unit index, ESI -> IDENTIFY buffer
;   Uses: EAX, ECX, EDX
;----------------------------------------------------------------------------
BeginProc XTIDE_CopyId

|push|esi
|push|edi
|mov|edx, eax
|imul|edx, edx, 40
|add|edx, OFFSET32 XTIDE_UnitModel
|mov|edi, edx
|add|esi, ID_MODEL_OFF
|mov|ecx, 20|||; words
xci_loop:
|mov|al, [esi+1]||; ATA word order is reversed
|mov|[edi], al
|mov|al, [esi]
|mov|[edi+1], al
|add|esi, 2
|add|edi, 2
|dec|ecx
|jnz|xci_loop
|pop|edi
|pop|esi
|ret

EndProc XTIDE_CopyId
""")

# --- inquiry body: identify, do not describe geometry ----------------------

OLD_BODY_START = T("""|call|XTIDE_SelectUnit||; geometry for THIS unit
""")

NEW_BODY_START = T("""|call|XTIDE_SelectUnit||; geometry for THIS unit

;  What the IOS actually asks for here: who is this unit? The three strings
;  below are real DCB fields. Geometry is NOT this function's job - see the
;  note further down about DCB_BLOCKDEV.
|mov|edx, [esp+8]||; unit index, saved on entry
|imul|edx, edx, 40
|add|edx, OFFSET32 XTIDE_UnitModel

|lea|edi, [esi].DCB_vendor_id
|mov|ecx, 8
|call|XTIDE_CopyStr
|lea|edi, [esi].DCB_product_id
|mov|ecx, 16
|call|XTIDE_CopyStr
|lea|edi, [esi].DCB_rev_level
|mov|ecx, 4
|mov|edx, OFFSET32 XTIDE_RevText
|call|XTIDE_CopyStr
""")

COPYSTR = T("""
;----------------------------------------------------------------------------
; XTIDE_CopyStr - copy ECX bytes from EDX to EDI, advancing EDX.
;
;   Uses: EAX, ECX, EDX, EDI
;----------------------------------------------------------------------------
XTIDE_RevText||db|'1.0 '

BeginProc XTIDE_CopyStr

xcs_loop:
|mov|al, [edx]
|mov|[edi], al
|inc|edx
|inc|edi
|dec|ecx
|jnz|xcs_loop
|ret

EndProc XTIDE_CopyStr
""")

# Save the unit index on entry so the string copy can find it again.
OLD_PROLOGUE = T("""|push|ebx
|push|edi

|cmp|eax, 1
|ja|xin_absent
""")
NEW_PROLOGUE = T("""|push|ebx
|push|edi
|push|eax|||; unit index, read back at [esp+8]

|cmp|eax, 1
|ja|xin_absent
""")

OLD_EXITS = T("""|xor|eax, eax|||; present
|pop|edi
|pop|ebx
|ret

xin_absent:
|mov|eax, 1
|pop|edi
|pop|ebx
|ret
""")
NEW_EXITS = T("""|xor|eax, eax|||; present
|pop|edx|||; discard the saved unit index
|pop|edi
|pop|ebx
|ret

xin_absent:
|mov|eax, 1
|pop|edx
|pop|edi
|pop|ebx
|ret
""")

# Stash the model alongside the geometry.
OLD_STASH = T("""|mov|XTIDE_UnitPresent[ecx], 1
|ret
""")
NEW_STASH = T("""|mov|XTIDE_UnitPresent[ecx], 1
|call|XTIDE_CopyId|| ; EAX still the unit, ESI still the ID buffer
|ret
""")


def main():
    p = os.path.normpath(SRC)
    d = open(p, encoding='latin1').read()
    if 'XTIDE_CopyId' in d:
        print('already applied')
        return
    edits = 0

    a = T('XTIDE_ReqUnit||dd|0\n')
    assert a in d, 'anchor missing: XTIDE_ReqUnit'
    d = d.replace(a, a + DATA, 1); edits += 1

    a = ';----------------------------------------------------------------------------\n' \
        '; XTIDE_StashGeometry'
    assert a in d, 'anchor missing: StashGeometry banner'
    d = d.replace(a, COPYID + COPYSTR + '\n' + a, 1); edits += 1

    assert OLD_STASH in d, 'anchor missing: stash tail'
    d = d.replace(OLD_STASH, NEW_STASH, 1); edits += 1

    assert OLD_PROLOGUE in d, 'anchor missing: inquiry prologue'
    d = d.replace(OLD_PROLOGUE, NEW_PROLOGUE, 1); edits += 1

    assert OLD_BODY_START in d, 'anchor missing: inquiry body'
    d = d.replace(OLD_BODY_START, NEW_BODY_START, 1); edits += 1

    assert OLD_EXITS in d, 'anchor missing: inquiry exits'
    d = d.replace(OLD_EXITS, NEW_EXITS, 1); edits += 1

    print('Patched: %d' % edits)
    assert edits == 6, 'expected 6 edits'
    if '--check' in sys.argv:
        print('--check: not written')
        return
    open(p, 'w', encoding='latin1', newline='').write(d)
    print('wrote ' + p)


main()
