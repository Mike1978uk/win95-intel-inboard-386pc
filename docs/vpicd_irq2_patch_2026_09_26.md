# VPICD: delivering the XT's IRQ 2 as IRQ 9 (#42)

Static read of our `vxd-patches/VPICD_INBOARD.VXD` (md5 `65fa4757`), 2026-09-26. **Nothing
here has run yet.** The patch exists so the bed run is traceable to a commit.

## Base

`VPICD_INBOARD.VXD` differs from the OSR1 stock `vxd-patches/osr1/VPICD.VXD` (md5 `96fefeb1`)
in exactly the 36 phantom-slave port sites and nothing else. Everything below is stock
Microsoft behaviour that the slave patch never touched.

## Why a card on B4 cannot work today

| | where | what it does on a 5160 |
|---|---|---|
| Master init | file `7076h`: ICW1 `11h`, ICW2 `50h`, **ICW3 `04h`**, ICW4 `01h` | IR2 is declared a cascade input. An 8259A master in cascade mode hands the INTA vector cycle to the slave on that input; there is none, so the vector the CPU reads is undefined |
| Vector hooks | init loop at `70B0h`: for IRQ 0-15, vector `50h+IRQ` hooked with `Hook_V86_Fault`, `Hook_PM_Fault`, `Hook_VMM_Fault` to `table[IRQ]` at file `750Eh` | entry 2 is object-1 `0FC9h`, a **bare `ret`**: no EOI, so master IR2 stays in service and blocks IRQ 3-7 |
| IRQ 9 stub | file `1EC0h` | in-service bit and mask in the slave byte (`[1ACBh]`/`[1AC9h]`), physical write NOPed; ends in a specific EOI `62h` to master IR2 |
| IRQ 9 descriptor | file `2D40h`, mask word `0200h` | `Physically_Mask`/`Unmask` and end-of-interrupt act on the slave byte, which goes nowhere |

UniPCemu's 8259 model agrees on the first row (`hardware/pic.c`, `startSlaveMode`: ICW3 bit
set, master, cascade mode -> the slave supplies the vector). Corroboration from a second
emulator, not from hardware.

86Box (`src/pic.c`): on an XT `pic.slaves[2]` is NULL, and `pic_slave_on()` is true for IR2
under ICW3 `04h`, so `picinterrupt()` dereferences the missing slave. What the bed showed on
2026-09-24 with the card at 9 has not been re-read against this.

## The patch - `vxd-patches/patch_vpicd_irq2.py`

Output `VPICD_INBOARD_IRQ2.VXD`, md5 `6c304f8c`, 46,543 bytes, 37 bytes changed.

| | change |
|---|---|
| A | master ICW3 `04h` -> `00h` at init (`7083h`) and at exit (`14A6h`) |
| B | stub-table entry 2 -> `0EC0h` (the IRQ 9 stub), page data and fixup record |
| C | IRQ 9 stub works on the master: in-service bit 2 of `[1ACAh]`, `out 21h` |
| D | IRQ 9 mask word `0200h` -> `0004h` |
| E | three fixup-list entries moved between the `[1AC8h..1ACBh]` records; block stays 126 bytes |

`ELNK3.VXD` stays stock: it reads 9 from the card and virtualises IRQ 9, which now arrives
through master IR2. The script checks the input md5, every original byte, and re-parses the
fixup block after rebuilding it.

**Known imprecision:** the status services test the IRQ number's own bit, so
`VPICD_Get_Complete_Status` for IRQ 9 does not reflect master IR2.

**Not handled:** IRQ 2's own descriptor shares master bit 2. Nothing virtualises IRQ 2 on this
machine, but a driver that did would fight IRQ 9 for the mask.

## To test, in this order

1. Bed `vm_3c509b`, card `irq = 9`, on an 86Box build carrying `ef082884b` (IRQ 9 raised as
   IRQ 2 without a slave). Pre-monolith deploy (`deploy_premonolith.sh`); `BOOTLOG.TXT` must
   show `VPICD` loaded from the file.
2. Control: the same bed with stock `VPICD_INBOARD.VXD` - expect the 09-24 failure.
3. Pass = Windows boots, the NIC passes traffic, and IRQ 3-7 devices (mouse, SB Pro) still work.
4. Only then the 5160, with the card set to 9 in `3C5X9CFG` and the T130B on IRQ 3.
