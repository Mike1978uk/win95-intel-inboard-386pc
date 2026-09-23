# OBSOLETE: ELNK3.VXD IRQ9 -> XT IRQ2 candidate

This candidate is superseded by the hardware-read patch.

The old patch changed IRQ9 at file offset 0x3A8B, in the later resource-validation path. Real-hardware testing showed that it did not remove the Windows Protection Error.

Do not use this candidate for further testing.

The replacement is:
ELNK3_IRQ9_TO_IRQ2_HWREAD_PATCH.py
ELNK3_IRQ9_TO_IRQ2_HWREAD_PATCH.md

The new patch operates at file offset 0x3100, where ELNK3.VXD reads the actual 3C509B Resource Configuration register and extracts the hardware IRQ field.
