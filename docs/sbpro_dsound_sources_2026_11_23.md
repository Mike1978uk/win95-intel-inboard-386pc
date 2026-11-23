# SB Pro 2 DirectSound HAL driver: sources read, 2026-11-23

For T5 (`docs/next_session_2026_11_24.md`, issue #58). Read-only pass; nothing built, nothing run on the card.

## Source caveats (read first)

- **DX5 DDK file names are shifted by one.** `references/dxddk/dx5ddk/src/` prefixes are right but the
  name after the prefix belongs to the neighbouring entry. Real content, by prefix:

  | Prefix | Named | Is |
  |---|---|---|
  | `0021e8d5` | `dscert.h` | `dsdriver.h` (288 lines) |
  | `0021f2a1` | `dsdriver.h` | `dsound.h` (DSCAPS_*, DSBCAPS_*, DSERR_*) |
  | `0021e7c1` | `dmemmgr.h` | `dscert.h` (`DsCertification()` only) |
  | `0064b4e6` | `cnfgmgr.h` | `drvobj.c` (613 lines) |
  | `0064643f` | `messages.inc` | `bufobj.c` (663 lines) |
  | `0064ca6e` | `drvobj.c` | `dsnd.h` (374 lines) |
  | `0064dd3e` | `dsnd.h` | `dsnd.inc` (33 lines) |
  | `0064de8f` | `dsnd.inc` | `dsound.asm` (345 lines) |
  | `00647aab` | `bufobj.c` | `cnfgmgr.c` (calls `dsInitializeDriver`) |
  | `005169de` | `ds-ddk.cnt` | `ds-ddk.hlp` (81 KB, WinHelp) |
  | `005164c2` | `ds-ddk.doc` | `ds-ddk.cnt` (topic list) |
  | `0052565d` | `ds-ddk.hlp` | not help; an `MZ` executable (975 KB) |

  There is no `ds-ddk.doc`. The DDK text is a WinHelp file.
- **The HLP is decoded** (`helpdeco`, `docs/host_tools.md`): all 77 topics as RTF and plain text in
  `references/dxddk/ds_ddk_decoded/` (gitignored). Quotes below marked "DDK guide" come from it.
- **`SBPRO2.pdf` is a 2-page jumper sheet**, not a programmer's manual (title `50402.DOC`, 959 + 564
  characters of text). Section C cannot come from it. Where C says "emulator" the source is
  `86box_upstream/src/sound/snd_sb_dsp.c` / `snd_sb.c`: that is 86Box's model of the card, not evidence of
  what the CT1600 does. Where C says "recollection" it is general SB programming knowledge with no local
  source; probe before relying on it.
- **No DirectSound sample in the Win95 DDK.** `Windows95_ddk/MMEDIA/SAMPLES/MSSNDSYS/` (DRV 27 files, VXD 18
  files, ~1 MB) has no `dsound` or `IDsDriver` string. The DirectSound HAL half exists only in the DX5 DDK.
  `dx7ddk/src` and `ddk9x/` hold display/Direct3D sources only (grep of names: no sound, wave or DS file);
  `references/dx7sdk` is headers and `references/dx61samples/dsound` is application samples.

## A. The DirectSound HAL contract

Source: `0021e8d5` (dsdriver.h), `0021f2a1` (dsound.h), sample `0064b4e6`/`0064643f`.

**Interfaces.** `IID_IDsDriver` {8C4233C0-B4CC-11CE-9294-444553540000},
`IID_IDsDriverBuffer` ...C1, `IID_IDsDriverPropertySet` (3D/speaker property sets; optional, skip).
Plain vtables of `__stdcall` functions in a VxD; COM only in shape. First DWORD of the object is the vtable
pointer, the rest is ours.

| IDsDriver | Notes from the sample |
|---|---|
| QueryInterface / AddRef / Release | refcount only |
| `GetDriverDesc(PDSDRIVERDESC)` | fills the description below |
| `Open()` / `Close()` | acquire/release the device; sample fails `DSERR_ALLOCATED` if busy |
| `GetCaps(PDSDRIVERCAPS)` | see caps below |
| `CreateSoundBuffer(pwfx, dwFlags, dwCardAddress, LPDWORD pSize, LPBYTE *ppBuf, LPVOID *ppCtx)` | returns buffer pointer, size, object |
| `DuplicateSoundBuffer` | sample returns `DSERR_UNSUPPORTED` |

The header declares `CreateSoundBuffer` with six arguments (no `dwCardAddress`); the sample's C code has
seven. The sample's version is what compiled against this DDK. Take the sample as the truth.

| IDsDriverBuffer | Notes |
|---|---|
| `Lock` / `Unlock` | sample: `DSERR_UNSUPPORTED`; legal only because `DONTNEEDPRIMARYLOCK` is set |
| `SetFormat` / `SetFrequency` | sample: `DSERR_UNSUPPORTED`; wave driver does it (flag below) |
| `SetVolumePan(PDSVOLUMEPAN)` | sample: unsupported. Carries `lVolume` (1/100 dB), `lPan`, amp factors |
| `SetPosition(DWORD)` | sample: unsupported (secondary-buffer call) |
| `GetPosition(&play, &write)` | sample: bytes played = size - (DMA count + 1); both cursors equal |
| `Play(res, res, dwFlags)` | `DSBPLAY_LOOPING` always set for a primary |
| `Stop()` | position resets to 0 |

**`DSDRIVERDESC`** fields the sample sets: `dwFlags`, `szDesc[256]`, `szDrvname[256]` (the VxD file),
`dnDevNode`, `wVxdId`, `ulDeviceNum` = 0, `dwHeapType` = `DSDHEAP_NOHEAP`, `wReserved` =
`DsCertification()` (an import from `dscert.lib`; the real library is not in the dump, so whether DSOUND
refuses an uncertified driver is unknown).

**Flags, exact names** (`dsdriver.h`):

- `DSDDESC_DOMMSYSTEMOPEN` 0x1, `DSDDESC_DOMMSYSTEMSETFORMAT` 0x2, `DSDDESC_USESYSTEMMEMORY` 0x4,
  `DSDDESC_DONTNEEDPRIMARYLOCK` 0x8, `DSDDESC_DONTNEEDSECONDARYLOCK` 0x10.
- `DSDHEAP_NOHEAP` 0, `DSDHEAP_CREATEHEAP` 1, `DSDHEAP_USEDIRECTDRAWHEAP` 2, `DSDHEAP_PRIVATEHEAP` 3.
- Sample sets `DOMMSYSTEMOPEN | DOMMSYSTEMSETFORMAT | DONTNEEDPRIMARYLOCK`. Header has no comment on any
  flag. Reading from the sample's own comments: `DOMMSYSTEMOPEN` = DSOUND opens the wave device through
  MMSYSTEM first; `DOMMSYSTEMSETFORMAT` = DSOUND sets rate through the wave device, so `SetFormat` /
  `SetFrequency` stay unsupported ("On DMA based cards DirectSound should use MMSYSTEM to set the rate").
  `USESYSTEMMEMORY` is never used by the sample. DDK guide: it asks DSOUND to allocate *secondary*
  buffers "for convenience only"; primaries are always the driver's own, and "drivers should always make
  sure that the buffer memory for primary buffers is page-locked". `DONTNEEDPRIMARYLOCK` is meant for
  "a DMA-based device in which the driver needs to supply the location and size of the DMA buffer".

**Caps for a primary-only driver** (`IDsDriver_GetCaps`, `0064b4e6`): `dwFlags` =
`DSCAPS_PRIMARYMONO | PRIMARYSTEREO | PRIMARY8BIT | PRIMARY16BIT` (SB Pro: drop `PRIMARY16BIT`);
`dwPrimaryBuffers` = 1; `dwMin/MaxSecondarySampleRate` = 0; every `dwMaxHwMixing*`, `dwFreeHwMixing*`,
`dwMaxHw3D*`, `dwFreeHw3D*` = 0; `dwTotalHwMemBytes`, `dwFreeHwMemBytes`, `dwMaxContigFreeHwMemBytes` = 0.
Other flag names in `dsound.h`: `DSCAPS_CONTINUOUSRATE` 0x10, `DSCAPS_EMULDRIVER` 0x20 (DSOUND's own
emulation, today's case on the card), `DSCAPS_CERTIFIED` 0x40, `DSCAPS_SECONDARYMONO/STEREO/8BIT/16BIT`
0x100-0x800.

**Does DSOUND still mix secondaries in software into our primary?** Yes. DDK guide, "Hardware Mixing":
"DirectSound will always reserve one mixing channel for the DirectSound software mixer", and "for an ISA
card, hardware mixing is really only useful if the card has on-board memory". The HAL "must never emulate
functions in software; emulation is best performed by the optimized code in DirectSound". The sample
agrees: zero hardware mixing, one buffer, no mixer.

**Open catch.** The sample's `CreateSoundBuffer` returns `DSERR_CONTROLUNAVAIL` if flags contain
`DSBCAPS_CTRLFREQUENCY | CTRLPAN | CTRLVOLUME`. What DSOUND does then (fall back to emulation?) is not in
the sources. Probe D1.

**Registration** (`dsInitializeDriver`, `0064b4e6`): the wave VxD, on config-manager start
(`00647aab` calls it after `CM_Get_Bus_Info`; PCMCIA skipped), (1) allocates the driver object, (2)
`VXDLDR_LoadDevice(&pDDB, &pdi, "dsound.vxd", TRUE)`, (3) `DSOUND_RegisterDeviceDriver(pDriver, 0)`
(service in `dsdriver.h`, `DSOUND_DEVICE_ID` 0x357E; also `_GetVersion`, `_DeregisterDeviceDriver`,
`_DD_IOCTL`). On failure it unloads DSOUND. Deregister on shutdown/reconfig (`dsTerminateDriver`). The
`wVxdId` = `MSSNDSYS_Device_ID` = 0x0440 + 31 (Microsoft OEM id range); we need our own id. DSOUND finds
no driver by scan: the driver registers itself, so ours must be loaded by something. Candidate: our VxD
loads at the same point the SB Pro wave driver's VxD starts, or is a static VxD in SYSTEM.INI. Not
settled by the sources.

**Who allocates the buffer memory.** The driver. DSOUND hands over only a requested size; the driver
returns `*ppBuffer` (ring-0 linear address in the sample) and may shrink `*pRequestedSize`
(`AllocateBuffer`: clip to what was allocated at start-up, `&= 0xfffc`). In the sample the VxD allocates
one block at start (`MSSNDSYS_Allocate_DMA_Buffer`, `0065dd6b`):

- `_PageAllocate(pages, PG_SYS, 0, alignMask, 0, maxPhysPage, &phys, PageUseAlign + PageContig + PageFixed)`.
- Alignment mask = next power of two of the page count, minus 1, so a block of up to 64 KB never crosses a
  64 KB boundary. Size is halved until allocation succeeds. Request capped at 64 KB.
- `maxPhysPage` = min(`VDMAD_Get_Max_Phys_Page`, 0xFFF). **For us this must be 0xFF (below 1 MB), and
  whether `VDMAD_Get_Max_Phys_Page` already says so on the XT is a probe (D2).**
- The DS primary IS the wave driver's DMA buffer (`FillInDsBufferStruct` copies `ssi_lpDMABufferPhys`,
  `...Linear`, `...Len`, the DMA handle). `CreateSoundBuffer` requires 4096 < size < 65535 (sample
  check), so an exact 64 KB block fails the sample's own test; use a smaller size or fix the check.
- Wrap-around: a primary needs no `Lock`; DSOUND writes straight to the pointer.

## B. The MSSNDSYS DirectSound code

DS-specific files in the DX5 DDK (additions to the Win95 sample): `drvobj.c` 613 lines, `bufobj.c` 663,
`dsnd.h` 374, `dsnd.inc` 33, `dsound.asm` 345; about 2,000 lines. Hooks into existing files: the
config-manager start calls `dsInitializeDriver`; `api.asm` `MSSNDSYS_IRQ_Hw_Int_Proc` tests `ssi_DsRunning`;
`mssndsys.asm` holds the `ssi_DsRunning` flag; `sbvirt.inc` clears it.

- **Buffer mapping.** `DSDRIVERBUFFER` (`dsnd.h`): vtable, parent, `DacState`, `DmaPhysAddr`,
  `DmaLinAddr`, `DmaSize`, `DmaHandle` (from `VDMAD_Virtualize_Channel` at start-up), `RequestedDmaSize`,
  refcount, format, codec class, base I/O, DMA channel. Mirrored in `dsnd.inc`; the headers warn to keep
  both in step.
- **Play** (`bufobj.c`): `DMA_Mask` -> `VDMAD_Set_Region_Info(handle, phys, size, locked)` ->
  `VDMAD_Set_Phys_State(read | auto-init | single-mode)` -> `VDMAD_Phys_UnMask_Channel` -> program the codec
  (WSS count registers, enable playback). A SB equivalent replaces the last step with DSP commands
  (`0x40` rate, `0x48` block size, `0x1C` or `0x90`).
- **GetPosition.** `VDMAD_Get_Phys_count` on the DMA handle (two 8237 count reads); bytes played =
  `RequestedDmaSize - (count + 1)`, doubled on 16-bit channels. `Play` and `Stop` keep `DacState`; when
  stopped it reports 0. The comment says write cursor may be PlayCursor + 1 at best; the sample reports
  both equal. On the XT each 8237 read crosses the slow bus (about 5.7 us per byte, per the project's
  measured I/O cost in `MEMORY.md` -> `iowidth-measured-2026-09-20`), so a poll costs tens of us.
- **Coexistence with the 16-bit wave driver.** `fwodDMABusy` is 1 for the wave driver, 2 for DS.
  `AcquireMSSNDSYS` (`dsound.asm`) sets `ssi_DsRunning` only if the codec owner is the System VM, or if
  the owner has input open and no output. `Close` clears it; `Close` also needs `ObjRefCount == 0`.
  While `ssi_DsRunning` and no capture is active the IRQ proc eats the interrupt (`MI_Unowned`): DS needs
  no IRQ for position because it reads the DMA count. One buffer object at a time (`ObjRefCount` assert).
- **Reuse structurally:** the driver/buffer vtable layout, registration and deregistration sequence,
  `AllocateBuffer` clipping, `_PageAllocate` call and alignment trick, the VDMAD calls
  (`Virtualize_Channel`, `Set_Region_Info`, `Set_Phys_State`, `Phys_Mask/UnMask_Channel`,
  `Get_Phys_count`), and the flag bookkeeping between wave driver and DS. Replace: codec register I/O,
  the `DAK_DUALDMA` halving, 16-bit paths, AGA/PCMCIA code.
- **Not in the sample:** any hardware-mixing, any secondary buffer, volume or pan, position notify, the
  property-set interface. `sbvirt.asm` (`0065d613`) is a trap-and-map of DOS SB ports onto the WSS codec; it
  lists the SB port offsets (`0Ah` read, `0Ch` write, `0Eh` data-available, `04h/05h` mixer, `06h` reset,
  `08h/09h` FM) and a short DSP command table (`10h 14h 1Ch 24h 2Ch 40h 48h 90h D0h D1h D3h D4h D8h DAh E0h
  E1h F2h`; mixer regs `00h 02h 04h 06h 08h 0Ah 0Ch 0Eh`). Useful as a port map; no real-SB driving code.

## C. SB Pro 2 levers

Manual (`SBPRO2.pdf`) says only: CT1600; "Maximum DRAM N/A"; connectors for CD-ROM interface (CN6), CD audio
(CN1), line in, microphone, speaker, volume knob, PC-speaker, game/MIDI port (JP2, enable jumper JP4);
I/O 220h (default) or 240h (JP13); IRQ 5 (default), 2, 7, 10 (JP18-JP20); DRQ/DACK 1 (default), 0, 3
(JP5-JP7, JP15-JP17); DMA-sharing jumper JP11. It lists "Data Bus 16-bit ISA", which is doubtful for this
8-bit card and is not relied on. **Silent on:** DSP commands, rates, stereo, ADPCM, mixer map, filters, FIFO,
latency, IRQ acknowledge.

Emulator: SB Pro 2 is DSP 3.01/3.02 in `sb_dsp_versions`; `snd_sb.c` `sb_ct1345_mixer_*`.

| Lever | What it would save | Catch |
|---|---|---|
| **Auto-init 8-bit DMA** (`0x1C`, `0x48` block size) | One DMA program per play, zero CPU per block. The DS design needs exactly this: one ring. | Needs an IRQ ack (read `2xEh`) each block on a real DSP; whether playback continues unacked is unknown (D4). Block size sets IRQ rate. |
| **High-speed DMA** (`0x90` auto-init, `0x91` single) | Reaches the top rate (44.1 kHz mono, recollection). | Recollection: in high-speed mode the DSP takes no commands until a reset ends it, so no pause, no rate change, no stop except DSP reset. Emulator treats `0x90` as `0x1C` with no such lock. Needs D3. |
| **Stereo** (mixer reg `0Eh` bit 1; DSP 3.xx) | One DMA stream carries both channels, same bytes per frame as two mono buffers but one device. | Recollection: DSP rate is the byte rate, so time constant must be for 2x the per-channel rate; stereo max 22.05 kHz per channel (44.1 kHz byte rate); left/right order and a leading-sample quirk. Emulator: `sb_dsp_set_stereo` on mixer write `0Eh` bit 1. D5. |
| **Output filter** (mixer `0Eh` bit 5 = filter off in emulator) | Free anti-alias on the card at low rates. | Recollection: fixed low-pass is too low for 22 kHz and above, so set it off there. One mixer write. |
| **ADPCM** (DSP 3.xx: `0x16/17` 2-bit, `0x74/75` 4-bit, `0x76/77` 2.6-bit; auto-init `0x1F`, `0x7D`, `0x7F`) | 2:1 (4-bit), about 3:1 (2.6-bit), 4:1 (2-bit) fewer DMA bytes read from RAM. | The DS primary is mixed PCM, so compression would cost a CPU encode per byte on a machine where CPU stores are the scarce thing. Only fits pre-compressed static content, and DSOUND's mixer sees PCM. Emulator keeps a separate stereo path for ADPCM, so mono-only is unconfirmed. Not recommended. |
| **Silence** (`0x80` + 2 length bytes + IRQ; emulator calls it `Pause DAC`, sets `sb_pausetime`) | N zero samples without N bytes of DMA. | DSOUND mixes silence itself and the driver does not see it; Stop is the cheaper idle. Little use. |
| **Mixer CT1345**: `04h` voice, `22h` master, `26h` FM, `28h` CD, `2Eh` line, `0Ah` mic, `0Ch` input source and input filter, `0Eh` stereo and output filter, `00h` reset (emulator map; DDK `sbvirt.asm` names `02h 04h 06h 08h 0Ah 0Ch 0Eh`) | Primary-buffer volume (and L/R balance as pan) via `04h` or `22h`: `SetVolumePan` without touching a sample. 3-bit steps, 4 dB each in the emulator (nibble L high, R low). | Coarse: 8 steps. Mixer writes are two slow-bus I/O each. Emulator mirrors `02h`<->`22h`, `06h`<->`26h`, `08h`<->`28h` (SB Pro compatibility). Real-card behaviour unconfirmed. |
| **OPL3 FM** (`388h`/`220h+8`) | Music without PCM bytes. | A different path from DirectSound; the game must drive it. Not a DS lever. |
| **On-card memory** | None. | Manual: "Maximum DRAM N/A". Nothing to cache buffers in, so `dwTotalHwMemBytes` = 0 and every PCM byte comes from system RAM every time. |
| **Game/MIDI port, joystick** | Not audio-path levers. | Out of scope; `JP4` enable only. |

Things the sources do say that bear on bytes moved:

- 8-bit PCM, so already the smallest format the card accepts uncompressed. Halving rate or going mono are
  DSOUND-side decisions; our primary can be mono 11 or 22 kHz when the app allows it
  (`DSCAPS_PRIMARYMONO` reported, rate chosen by DSOUND through the wave driver).
- Buffer length vs IRQ: one IRQ per full ring minimises ISR work; position comes from the 8237 count, not
  the IRQ.
- A 64 KB ring at 22.05 kHz stereo 8-bit is about 1.5 s: latency is set by how far behind the mixer
  writes, not the ring length.

## D. Open questions for a real-card probe

Cheapest first. All run from DOS or a Win95 test program; none needs a build of the emulator.

1. **D1: what does DSOUND do on a primary request with CTRLVOLUME/PAN/FREQUENCY set?** Sample returns
   `DSERR_CONTROLUNAVAIL`. DDK guide: "drivers should fail to create primary buffers if volume or pan
   control is requested; in the current release this doesn't work because of conflicts with the Windows 95
   mixer controls". So the sample's behaviour is the rule; DSOUND then handles primary volume itself.
   Probe only if a title misbehaves: a test app asks for a primary with `DSBCAPS_CTRLVOLUME`.
2. **D2: `VDMAD_Get_Max_Phys_Page` on the XT/Inboard.** Does it return below-1 MB, or 0xFFF? A 30-line
   VxD (or the existing audit tool, `win9x-dma-driver-audit` skill) that calls it and `_PageAllocate` with
   `maxPhys` 0xFF, prints page frames. If it returns 0xFFF, pass 0xFF ourselves.
3. **D3: high-speed lock-out and the real max rates.** DOS program: reset DSP, `E1h` (confirm 3.0x), play
   a ramp at 22.05 kHz stereo with `1Ch` then `90h`, and at 44.1 kHz mono with `90h`; try a command mid
   play; count underruns by DMA count. Needs the card, about one trip.
4. **D4: does auto-init DMA keep going if the IRQ is never acked?** Same program: leave the IRQ line
   unacknowledged for several blocks and check the 8237 count still wraps. Decides whether the driver can
   mask IRQ 5 and skip the ISR entirely.
5. **D5: stereo order and byte rate.** Play a left-only then right-only click in stereo through `0Eh`
   bit 1 + `1Ch`; confirm L-first, and the time constant (rate x 2). Also check the output-filter bit's
   audible effect at 22 kHz.
6. **D6: the DMA position read cost and accuracy on this bus.** Time a loop of `VDMAD_Get_Phys_count`
   equivalents (two 8237 count reads with a flip-flop clear) at ring 0, and compare the count to the
   DSP-side expected byte count after 1 s. Tells whether DSOUND polling at its default rate is affordable.
7. **D7: does the primary's own volume (mixer `04h`/`22h`) behave per step?** Write each value, record the
   output level on the card's line out or a loopback; map steps to `lVolume`. Optional; only if
   `SetVolumePan` is implemented.

Not probed and not planned: ADPCM (D-less by design, see C), silence command, FM.

## Sources cited

- `references/dxddk/dx5ddk/src/0021e8d5_dscert.h` (true `dsdriver.h`); `0021f2a1_dsdriver.h` (true
  `dsound.h`); `0064b4e6_cnfgmgr.h` (true `drvobj.c`); `0064643f_messages.inc` (true `bufobj.c`);
  `0064ca6e_drvobj.c` (true `dsnd.h`); `0064de8f_dsnd.inc` (true `dsound.asm`); `0065dd6b_sbvirt.inc`
  (`MSSNDSYS_Allocate_DMA_Buffer`, DMA buffer setup); `00653e6f_api.asm` (IRQ proc); `00658101_mssndsys.asm`
  (`ssi_DsRunning`); `0065d613_sbvirt.asm` (SB port and command equates); `005164c2_ds-ddk.doc` (true
  `.cnt`, topic list); `005169de_ds-ddk.cnt` (true `.hlp`, decoded to `references/dxddk/ds_ddk_decoded/`).
- `C:\Users\lycet\OneDrive\Desktop\XT_project\SBPRO2\SBPRO2.pdf`: jumper sheet only.
- `C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk\MMEDIA\SAMPLES\MSSNDSYS\`: wave driver and VxD
  without DirectSound.
- `86box_upstream/src/sound/snd_sb_dsp.c`, `snd_sb.c`: emulator behaviour, not hardware.
- `docs/captures/2026-11-23_t4_sound/README.md`: PlaySound ~6 %, emulated DirectSound ~22 %.
