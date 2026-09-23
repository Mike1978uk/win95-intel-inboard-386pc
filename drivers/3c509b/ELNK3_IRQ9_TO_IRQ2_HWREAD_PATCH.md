# ELNK3.VXD IRQ9 -> XT IRQ2: hardware-read patch

Status: candidate / not yet tested on the real IBM 5160.

This supersedes the earlier ELNK3_IRQ9_TO_IRQ2_PATCH.py candidate.

## Why this is the correct path

The earlier patch changed IRQ 9 at the Windows resource-validation point around file offset 0x3A8B. That was too late.

The driver actually reads the 3C509B hardware Resource Configuration register during initialization.

At file offset 0x3100 the original code is:

    lea edx,[ecx+8]
    in  ax,dx
    shr ax,12
    mov ecx,eax
    and ecx,0xffff
    mov [ebx+80h],ecx

This is an actual I/O read of the card's Window 0 / Resource Configuration register at I/O base + 0x08.

The 3Com technical reference confirms that this register contains the IRQ field in bits 15..12, and that the 3C509B's automatic configuration loads it from EEPROM offset 09h. It documents IRQ values 3,5,7,9,10,11,12,15 as valid interrupt selections.

Therefore the physical card legitimately contains 9; the driver must read 9 first.

## New behaviour

The patch changes the software interpretation immediately after that hardware read:

    card register
         |
         | reads 0x9000 when configured for IRQ9
         v
    SHR AX,12
         |
         | AX = 9
         v
    compare with 9
         |
         +---- no ----> retain original value
         |
         +---- yes ---> AX = 2
                         |
                         v
                    existing code

So:

- The physical card remains configured as IRQ9.
- The driver really reads IRQ9 from the hardware.
- Only the software value derived from that read becomes IRQ2.
- Other IRQ values are unchanged.
- The card's EEPROM/resource register is not changed to 2 by this patch.

This distinction matters because the 3Com documentation explicitly says that IRQ2 is one of the values that disables the card's IRQ line driver. Therefore we must not reprogram the 3C509B itself with IRQ2. The desired arrangement is hardware configuration = 9, XT interrupt interpretation = 2.

## Exact binary patch

Original at file offset 0x3100:

    8D 51 08 66 ED 66 C1 E8 0C

Replaced with:

    E9 7B 1E 00 00

This jumps to a zero-filled code cave at 0x4F80.

The cave contains:

    8D 51 08             lea edx,[ecx+8]
    66 ED                in ax,dx
    66 C1 E8 0C          shr ax,12
    66 83 F8 09          cmp ax,9
    75 04                jne +4
    66 B8 02 00          mov ax,2
    E9 71 E1 FF FF       jmp 0x3109

0x3109 is the original mov ecx,eax, so the existing code resumes immediately after the original shift.

The code cave is 24 bytes and was verified to be zero-filled in the original 30,773-byte ELNK3.VXD.

## Why this differs from the failed candidate

The failed candidate modified:

    resource value -> 9 -> 2

at the later validation/storage path around 0x3A8B.

The new candidate instead modifies:

    actual hardware read -> 9 -> 2

before the value leaves the hardware-discovery path.

That directly tests the hypothesis that the Protection Error is caused by ELNK3.VXD reading the real 3C509B configuration and subsequently treating the returned IRQ9 as an AT/ISA resource number rather than translating it for the XT.

## Important hardware detail

The 3Com technical reference says:

- Resource Configuration register = Window 0 offset 08h.
- IRQ is bits 15..12.
- IRQ9 is a valid 3C509B hardware IRQ selection.
- IRQ2 is listed as a value that disables the IRQ line driver.
- The 3C509B automatically loads the Resource Configuration register from EEPROM offset 09h.

Therefore this patch deliberately does not attempt to make the card believe it is configured for IRQ2.

## Test procedure

1. Keep the known-good IRQ3 ELNK3.VXD untouched.
2. Configure the physical 3C509B to IRQ9.
3. Run:

       python ELNK3_IRQ9_TO_IRQ2_HWREAD_PATCH.py ELNK3.VXD ELNK3_IRQ9_TO_IRQ2_HWREAD.VXD

4. Install only the generated VXD.
5. Boot Windows 95.
6. Record whether the Protection Error changes.
7. If Windows boots, test networking.
8. If Windows boots but interrupts do not work, inspect the later interrupt-registration path separately. Do not change the physical card to IRQ2: the 3C509B must remain configured as IRQ9.

## Current conclusion

This is now the first patch candidate at the point where we can prove the driver is reading the actual 3C509B Resource Configuration register.

It is still a candidate until tested on the real 5160.

The earlier resource-validation patch should be treated as obsolete and archived rather than used for further testing.
