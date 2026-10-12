# Toolchain spike: the DDK FRAMEBUF minidriver, built with MASM 6.11 + Open Watcom

Goal: prove we can build a Windows 95 16-bit display driver (NE `.DRV`) before writing our own. Started
2026-10-12; **not finished - the link stops at 6 errors.** Microsoft's sample is not in this repo; the steps
below rebuild it from the Windows 95 DDK (`XT_project/Windows95_ddk`).

## Steps that work

1. Resources (`DISPLAY/SAMPLES/MINI/RES`): `ML -c -Zm -Cp` each of CONFIG, COLORTAB, FONTS, FONTS120, then
   `wlink format raw bin name x.bin file X.obj` (replaces 16-bit LINK + EXE2BIN; the stack/start warnings
   are expected). Copy `FRAMEBUF.RCV` to `display.rcv`, then
   `wrc -q -r -bt=windows -i=<OW>/h/win;<DDK>/INC16 DISPLAY.RC -fo=display.res`.
2. Assemble all 22 FRAMEBUF sources with the DDK's `ML.EXE -c -Zm -Cp`, **INCLUDE order
   `DISPLAY/INC;INC32;INC16;.`** (the DDK `master.mk` order, from Christian Holzapfel's `xga-win9x`
   `buildall.bat`). INC16 first picks the wrong `CMACROS.INC` and nothing assembles.
3. Link: `ms2wlink` converts MS's link line + `FRAMEBUF.DEF` (`framebuf.lnk` here); replace
   `system windows dll` with `format windows dll` (the system file is not found), add
   `libpath <DDK>/LIB`, `option resource=../RES/display.res`, `option alignment=16`.

## Where it stops

`wlink` E2055 "frame must be the same as the target in protect mode": a far jump/call to a DIB engine import
whose `externFP` sits inside the code segment, so MASM uses our segment as the frame (MS LINK accepted it).
`extern_outside_segments.patch` moves those declarations outside any segment: 60 errors -> 6. The 6 left are
in DIBLINK.obj at 0001:02ac, 02c4, 02e7 - not yet identified (the `extra` path of the DIBLINK macro is the
first suspect: `mov ax,DGROUP` / `push extra`).

Tools: Open Watcom v2 snapshot (2026-10-10) at `C:\Users\lycet\tools\openwatcom` (velocity9x uses the same
toolchain for its 16-bit driver); MASM 6.11c from the DDK.
