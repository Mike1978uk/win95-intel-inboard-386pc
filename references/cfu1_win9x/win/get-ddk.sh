#!/bin/sh
# Fetch the Windows 98 DDK includes needed to build CFU1.VXD and patch
# them for JWasm.  The DDK is Microsoft-copyrighted so it is not part of
# this repository; this script pulls it from the Internet Archive.
#
# Result: ./ddkinc/ with vmm.inc (patched), vpicd.inc, vwin32.inc, etc.
# The patches make MASM6/OLDMACROS-era constructs digestible by JWasm:
#   1. comment out the MakeCodeSeg factory invocations (nested macro-name
#      generation JWasm cannot parse; the VxD_LOCKED_* segment macros are
#      defined separately and unaffected)
#   2. rename @@-prefixed symbols (JWasm reserves @@ for anonymous labels)
#   3. collapse && substitution operators to & (OPTION OLDMACROS semantics;
#      JWasm ignores that directive)
#   4. unescape &macro/&endm nested-definition keywords
set -e
cd "$(dirname "$0")"

DDK_URL="https://archive.org/download/DDK-9x-ME/98DDK.RAR"
WORK=ddk-tmp

command -v unar >/dev/null || { echo "need 'unar' (brew install unar)"; exit 1; }
command -v cabextract >/dev/null || { echo "need 'cabextract' (brew install cabextract)"; exit 1; }

mkdir -p "$WORK" ddkinc
[ -f "$WORK/98ddk.rar" ] || curl -L -o "$WORK/98ddk.rar" "$DDK_URL"
[ -d "$WORK/98DDK" ] || unar -q -o "$WORK" "$WORK/98ddk.rar"
cabextract -q -d "$WORK/x" "$WORK/98DDK/CABS/I386/INCS_DDK.CAB"
# block-device DDK (IOS structures for the drive-letter work)
cabextract -q -d "$WORK/x" "$WORK/98DDK/CABS/I386/BLCK_DDK.CAB"

# VMM/HID/mouse/keyboard + IOS registration includes
for f in VMM VPICD PCCARD CONFIGMG VWIN32 DEBUG VMMREG BASEDEF REGSTR SHELL \
         VXDLDR VMD VKD DRP ILB AEP IOS; do
    lc=$(echo "$f" | tr 'A-Z' 'a-z')
    cp "$WORK/x/INC_WIN98_$f.INC" "ddkinc/$lc.inc"
done
cp ddkinc/vmm.inc ddkinc/vmm.inc.orig

python3 - <<'EOF'
lines = open('ddkinc/vmm.inc', encoding='latin-1').read().split('\n')
for i in range(311, 326):   # MakeCodeSeg invocation block (1-based 312..326)
    s = lines[i].strip()
    if s.startswith('MakeCodeSeg') or s.startswith('LOCKABLE'):
        lines[i] = '; JWASM-SKIP ' + lines[i]
open('ddkinc/vmm.inc', 'w', encoding='latin-1').write('\n'.join(lines))
EOF

# order matters: named @@ symbols first, then the @@& prefix pattern,
# then && collapse, then nested-macro keyword unescape
sed -i.bak \
    -e 's/@@NextInternalID/JW_NextInternalID/g' \
    -e 's/@@VxDName/JW_VxDName/g' \
    -e 's/@@Get_Crit_Status_No_Block/JWS_Get_Crit_Status_No_Block/g' \
    -e 's/@@Test_DBCS_Lead_Byte/JWS_Test_DBCS_Lead_Byte/g' \
    ddkinc/vmm.inc
sed -i.bak 's/@@&/JWS_\&/g' ddkinc/vmm.inc
sed -i.bak 's/&&/\&/g' ddkinc/vmm.inc
sed -i.bak -e 's/&macro/macro/g' -e 's/&endm/endm/g' ddkinc/vmm.inc
rm -f ddkinc/vmm.inc.bak

echo "ddkinc ready:"
ls ddkinc/
