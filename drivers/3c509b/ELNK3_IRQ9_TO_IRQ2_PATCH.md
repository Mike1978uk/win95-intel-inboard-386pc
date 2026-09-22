# ELNK3.VXD IRQ9 -> XT IRQ2 candidate

This is a test candidate, not a replacement for the known-good ELNK3.VXD.

The 3C509B can be configured with resource/EEPROM IRQ 9. On an IBM 5160/XT the physical interrupt is usable on the XT IRQ2 input, but this Windows 95 ELNK3 driver keeps the resource number as 9.

This candidate changes that value to 2 inside ELNK3.VXD immediately before the existing code stores the selected IRQ for later interrupt setup.

Only IRQ9 is changed:

- IRQ 3 -> 3
- IRQ 5 -> 5
- IRQ 7 -> 7
- IRQ 9 -> 2
- IRQ 10 -> 10
- IRQ 11 -> 11
- IRQ 12 -> 12
- IRQ 15 -> 15

The original driver is not overwritten.

## Exact patch

Original ELNK3.VXD is 30,773 bytes.

At file offset 0x3A8B:

    83 F8 09 74 14

is replaced by:

    E9 EB 14 00 00

This jumps to unused zero-filled space at 0x4F7B.

At 0x4F7B:

    83 F8 09             cmp eax,9
    0F 85 06 00 00 00    jne continue_validation
    B8 02 00 00 00       mov eax,2
    E9 16 EB FF FF       jmp store_irq
    E9 FD EA FF FF       jmp next_comparison

The existing store is at 0x3AA4; the next IRQ comparison is at 0x3A90.

## How to build

From this directory:

    python ELNK3_IRQ9_TO_IRQ2_PATCH.py ELNK3.VXD ELNK3_IRQ9_TO_IRQ2.VXD

The script checks the original bytes before modifying anything and refuses to patch a different driver build.

## Test plan

1. Keep a copy of the known-good IRQ3 ELNK3.VXD.
2. Configure the physical 3C509B to IRQ9 with the existing 3CCFG utility.
3. Install the generated ELNK3_IRQ9_TO_IRQ2.VXD in place of the working ELNK3.VXD.
4. Boot Windows 95.
5. First check whether the previous protection error disappears.
6. Then verify NIC transmit and receive.
7. If it boots but networking fails, the next thing to inspect is the later ELNK3 interrupt registration/VPICD path.

Do not overwrite the known-good driver until the candidate has been tested.

The card remains configured as IRQ9. The conversion happens only inside the Windows driver.
