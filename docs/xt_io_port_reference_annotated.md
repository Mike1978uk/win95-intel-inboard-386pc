# XT I/O port reference — curated extract, annotated for this project

Source: **"XT, AT and PS/2 I/O port addresses"**, compiled by **Wim Osterholt**
(`wim@djo.wtm.tudelft.nl`), last changed 11/6/94. Full copy:
`C:\Users\lycet\OneDrive\Desktop\XT_project\inboard_files\win95_attempts_files\XT, AT and PS2 IO port addresses.txt`
(3,480 lines). Credits in the original also name Chuck Proctor, Richard W. Watson, Frank van
Gilluwe's *The Undocumented PC*, Dave Williams' DOSREF v3.0, and FractInt v18.0's `FR8514A.ASM` for
the 8514/A ports.

The original carries its own warning, which applies here too:

> Do NOT consider this information as complete and accurate. If you want to do hardware programming
> check ALWAYS the appropriate data sheets.

**Why this file matters to us:** it is one of the few references that marks entries **`(XT)`** or
**`(XT only)`** versus AT/PS-2. Most of our Windows 95 problems are the OS assuming AT hardware that
this machine does not have, so the XT-vs-AT annotations are the useful part — not the port numbers
themselves.

**Reading caution:** the list is *AT-centric by default*. Where an entry is not explicitly marked
`(XT)`, assume it describes AT behaviour and verify before relying on it for this machine. See the
DMA page-register note below for a case where this matters.

---

## ⚠️ NEW LEAD: port `0x00A0` means something different on an XT

```
00A0-00AF ----  PIC 2  (Programmable Interrupt Controller 8259)

00A0    r/w     NMI mask register (XT)
00A0    r/w     PIC 2  same as 0020 for PIC 1
```

On an **AT**, `0xA0` is the second PIC. On an **XT** there is no second PIC, and `0xA0` is the
**NMI mask register**.

**This project's Inboard emulation also uses port `0xA0`.** `inboard386.c` carries
`uint8_t port_a0;  /* XT-only port 0xA0 shadow (memory-size/remap related) */`, and the XT-variant
card is described throughout as the "port 0xA0/0x60" variant.

So on the real 5160 there are potentially **two consumers of the same port**: the motherboard's NMI
mask register and the Inboard card. Worth resolving, because it is exactly the class of thing that
produces "works on hardware, differs in emulation" symptoms:

- Does the real Inboard card fully decode `0xA0` and shadow/replace the NMI mask function, or does
  it alias onto it?
- Does our emulation let a BIOS/DOS write to the NMI mask register land in `port_a0` (or vice
  versa)?
- The 1986 BIOS writes the NMI mask during POST. If our device swallows that write, or if the NMI
  mask write clobbers the Inboard's shadow, either could produce subtle divergence.

**Not yet investigated.** Recorded here as a lead, not a finding. Cheapest check is an I/O trace of
`0xA0` accesses during POST (skill Technique 47, the I/O-dispatch hook) comparing who writes what
and when.

---

## Confirms the machine's PPI-not-8042 keyboard story

```
0060-006F ----  Keyboard controller 804x (8041, 8042)  (or PPI (8255) on PC,XT)
                XT uses 60-63,  AT uses 60-64
0060    r       KeyBoard or KB controller data output buffer (via PPI on XT)
0061    w       PPI  Programmable Peripheral Interface 8255 (XT only)
0062    r/w     PPI (XT only)
0063    r/w     PPI (XT only) command mode register  (read dipswitches)
```

Primary-source confirmation of the whole `VKD.VXD` / `KEYBOARD.DRV` line of work: **there is no port
`0x64` on this machine at all.** `VKD_Int_09`'s AT-only port-`0x64` check had nothing to read. Also
relevant to open issue **#2** (`#` in place of `\`) — that is a layout/scancode issue rather than a
controller issue, so this section says where *not* to look.

---

## DMA page registers — for the Sound Blaster work (issue #5)

```
0080-008F ----  DMA page registers   (74612)

0081    r/w     DMA channel 2 address byte 2
0082    r/w     DMA channel 3 address byte 2
0083    r/w     DMA channel 1 address byte 2
0087    r/w     DMA channel 0 address byte 2
0089/008A/008B  DMA channels 5/6/7          <-- AT only, do not exist here
008F    r/w     DMA refresh page register
```

Two things to take from this:

1. **The channel→port mapping is not in numeric order.** Our SB Pro runs on **DMA 1**
   (`SET BLASTER=A220 I5 D1 T4`), so its page register is **`0x83`**, not `0x81`.
2. **The `74612` in the heading is the AT part.** The XT uses a smaller page latch, which is where
   @andrew-hoffman's point comes from — the XT page register is only **4 bits**, giving 20-bit
   (1 MB) DMA reach, not the 24-bit the "address byte 2" phrasing implies. **This list documents the
   AT width; do not read 8 bits into it for this machine.** That is the whole basis of the
   `DMABufferIn1MB=Yes` test in the plan.

`008F` (refresh page) is also worth noting against the `F000:E507` DMA-refresh self-test fix already
in the emulator.

Also confirmed absent on XT: the **second DMA controller at `00C0-00DF`** is an AT addition. The
original `vmad.vxd` theory (Win95 poking a non-existent secondary 8237) was architecturally sound —
it just did not turn out to be the failure.

---

## 8514/A register ranges — for the Mach8 work (issues #4 and #8)

The ATI Graphics Ultra is named explicitly. The 8514/A register set is **sparse and spread across
four separate ranges**, which is easy to miss when tracing:

| Range | Notes |
|---|---|
| `02E8-02EF` | display status (r) / horizontal total (w), DAC mask, DAC read/write index, DAC data |
| `06E8-06EF` | — |
| `0AE8-0AEF` | horizontal sync start |
| `0EE8-0EEF` | — |

```
02E8    r       display status
02E8    w       horizontal total
02EA    w       DAC mask
02EB    w       DAC read index
02EC    w       DAC write index
02ED    w       DAC data
```

Relevant because the Mach8 option-ROM self-test is still only *worked around*, not root-caused
(`AX = BX+1` at `C000:7B16/7B23/7B37`). If the "uninitialised VRAM / unexpected register state"
hypothesis is to be tested, `02E8` **display status** is the natural first read to compare between
emulator and real hardware — and COMrade's DOS agent can do port I/O directly on the real machine
(skill Technique 6).

Note the plain VGA/EGA side of the card is separate, at `03C0-03CF` (with `02B0-02DF` as the
alternate EGA range) — consistent with the two-engine description in the Mach8 clue notes, though
that clue's specific "28800 + 38800" wording still wants checking against ardent-tool.

---

## Sound Blaster port ranges

```
0220-0223 ---- Sound Blaster / Adlib port
0220-022F ----  Soundblaster PRO 2.0        <-- ours (CT1600)
0388-0389 ---- Soundblaster PRO FM-Chip
```

Our card claims **16 ports** (`0220-022F`), not the 4 of an original SB, plus the FM chip at
`0388-0389`. Worth having exact when tracing what Win95's audio stack actually touches before it
dies.

---

## Other XT-marked entries worth knowing

| Port(s) | Entry |
|---|---|
| `0041` | PIT counter 1, **RAM refresh counter** (XT, AT) — behind the `F000:E507` refresh self-test |
| `0210-0217` | **Expansion unit (XT)** — the IBM 5161 |
| `0320-0323` | XT HDC 1 (hard disk controller); `0322 r` reads the DIP switches on the XT controller card |
| `0324-0327`, `0328-032B`, `032C-032F` | XT HDC 2/3/4 |
| `0340-0357` | RTC (1st real-time clock for XT), alternates at `0140-0157` and `0240-0257` |
| `0360-0367` | PC Network (XT only) |

⚠️ **`0340-0357` is a conflict to keep in mind:** this project's **Trantor T130B SCSI is configured
at I/O `0x340`**, which this list gives as the primary XT RTC range. Not currently known to cause a
problem (the T130B works), but if an RTC or clock driver is ever added, that is where it will
collide. The machine already runs `c:\clock\smwclock` from `AUTOEXEC.BAT`.

---

## Memory-mapped section

The file ends with a short memory-mapped list. Nothing Inboard-relevant, but note
`C0000000-C000FFFF Weitek "Abacus" math coprocessor`, and a Compaq Deskpro 386 RAM relocation
register — the latter is mildly interesting given the project previously used `deskpro386` as an AT
reference machine.
