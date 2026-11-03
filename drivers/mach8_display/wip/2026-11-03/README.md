# Work in progress, 2026-11-03 (not on the card yet unless stated)

Saved so nothing is lost; the source on master does not include these.

- `segment_split_and_black_front.diff` - two changes in the working tree, applied together:
  - Both buffers of a new 16 bpp flip chain start described black; the frame buffer is put in RAM
    before the exit send. Fixes dxdiag test 3's static background (the frame buffer kept the
    desktop at the old mode's pitch). Card: `M8FLIP5`, boot 202, GDI pixels = ATI-run baseline;
    dxdiag check by the owner pending.
  - dd16.c in its own code segment `M8DD_TEXT` (`-nt=M8DD_TEXT`, `option packcode=0`), five calls
    made far, two 32-bit multiplies as one MUL. `_TEXT` 62,921 bytes (2,615 free; was 39).
    Built, not yet on the card.
- `d3d_stub_wiring.diff` + `m8d3d.c` - Direct3D HAL stub in M8DD32.DLL (flat RGB, DDBD_8, no Z,
  no textures; ContextCreate, RenderState, RenderPrimitive, SceneCapture). Did not link before the
  segment split (`_TEXT` 89 bytes over). Gate: the device creates in the bed (plan P6/D1).
  The diff predates the split: re-apply by hand on top of it.
