# 86Box Inboard Emulation - Implementation Roadmap

**Objective**: Match real 5160+Inboard hardware exactly for Windows 3.11 & Windows 95 testing  
**Real Hardware Reference**: IBM 5160 + Comrade program running live alongside emulation  
**Test Target**: Windows 3.11 (Program Manager) → Win32s (FreeCell) → Windows 95 (XT-patched)

---

## Phase 1: Hardware Configuration Exactness

### Priority 1A: ROM Loading & Boot Configuration

**Sergey FDD ROM (0xC8000)**
- File: `roms/network/Sergey_FDD.bin` (8KB) ✓ Already in repo
- Location: Upper memory 0xC8000 (SmartWatch+ by dJos)
- Loading: Add to `machine_ibmxt_inboard386_init()` in `m_xt.c`
- Implementation:
  ```c
  bios_load_aux_linear("Sergey_FDD.bin", 0xc8000, 8192, 0);
  ```

**SmartWatch+ DS1315 RTC**
- Purpose: Phantom clock read via ROM-socket bit pattern (not IRQ/memory mapped)
- Datasheet: https://www.analog.com/media/en/technical-documentation/data-sheets/DS1315.pdf
- Reference Files: `references/ds1315_rtc/` (SMWCLOCK.COM utility)
- Implementation Type: ROM-socket phantom device (different from standard I/O mapped RTC)
- Status: Reference files gathered, needs device implementation

**IBM 5160 Board Switch Configuration**
- From your deep dive: `5160_deep_dive_config.txt`
- Switch Groups (SW1): 
  - Bit 1: Off (normal)
  - Bit 4: On (enables motherboard RAM banks 0/3)
  - Bit 5: On 
  - Bit 8: On (two floppy drives)
  - Others: Off
- Motherboard RAM: 256KB (1 bank in this config, rest is Inboard + piggyback)
- Floppy: Two drives configured

**Machine Table Entry Verification**:
- CPU: IBM 486BL3 (Blue Lightning, 83.5MHz)
- RAM: 5120KB (1MB onboard + 4MB piggyback)
- Base system matches real 5160 board switches

**Deliverable**: Boot to DOS with all ROMs loaded, time/date working

---

### Priority 1B: Device Configuration Exactness

**T130B SCSI** (Storage)
- I/O: 0x340 (real hardware confirmed)
- ROM: 0xCA000 (real hardware confirmed)
- **IRQ: NONE** (polling mode, all JP3 jumpers open)
- Wait States: Zero-wait enabled (JP2 jumped)
- Status: Device exists, needs polling-mode verification
- Machine table: `scsicard_1 = t130b` (polling, zero_wait if available)

**3C509B Ethernet** (Network)
- I/O: 0x320 (real hardware measured, NOT standard 0x300)
- IRQ: 3 (real hardware confirmed)
- ID Port: 0x110 (real hardware confirmed)
- MAC: 00:20:AF:6F:10:5E (real hardware measured)
- Status: QEMU reference patches available, needs porting
- Machine table: `.net_device = &nic_3c509b_device` with 0x320 I/O config

**ATI Mach8 Video** (Display)
- Mode: 8-bit ISA (JU1 top 2 pins, real hardware config)
- ROM: `roms/video/ATI_MACH8.bin` (32KB, real hardware dump) ✓
- **CRITICAL**: Use CGA mode (not VGA) to avoid POST timing issues
- Status: Device available, needs configuration + diagnostic bypass
- Known Issue: Self-test displays "RAM Addressing" grid (real hardware doesn't)
  - Workaround: May need option ROM detection fix or diagnostic bypass
  - Reference: INBOARD_86BOX_PORT_PLAN.md §3654-3178

**Sound Blaster Pro 2 CT1600** (Audio)
- I/O: 0x220 (real hardware)
- IRQ: 5 (JP20 jumped)
- DMA: 1 (JP6 jumped)
- Status: Device available in 86Box
- **KNOWN ISSUE**: Digitized sound DMA hangs on XT configs (general 86Box bug)
  - Cause: Bug in `dma_xt8237` implementation
  - Workaround: FM synthesis works; avoid digitized sound (test programs)
  - Reference: INBOARD_86BOX_PORT_PLAN.md §3741-3800
- Machine table: `.snd_device = &sb_pro2_device` (IRQ 5, DMA 1, 0x220)

**Sergey Floppy/Serial** (I/O)
- Floppy ROM: 0xC8000 (SmartWatch+)
- Serial: COM1 (0x3F8, IRQ 4)
- Status: ROM loading needed, serial auto-configured
- Implementation: Add ROM load in machine init (see 1A above)

**Parallel Port (Intel 21 TK9901)**
- I/O: 0x378 (standard LPT1)
- IRQ: 7 (JP4 pins 2-3)
- DMA: 3 (JP5, JP3 pins 2-3)
- Status: Standard LPT device should work
- Machine table: Auto-configured or `device_add(&lpt_port_device)`

**Deliverable**: All devices configured with exact real hardware I/O addresses, IRQs, DMA

---

## Phase 2: Boot & Test Windows 3.11

### Test Sequence

**Step 1: DOS Boot**
```
Expected: 
- BIOS POST completes
- Inboard drivers load (INBRDPC.SYS, REVTO486.SYS)
- DOS prompt: C:\>
- All memory detected: 4096K extended RAM
- SCSI driver chain loads silently (T130B in polling mode)
- Floppy/serial working
- Time displays correctly (SmartWatch+ if implemented)
```

**Step 2: Windows 3.11 Boot**
```
Expected:
- WIN command boots Windows
- BIOS/driver initialization completes
- Program Manager opens (FILE/WINDOW/HELP menus visible)
- CRITICAL TEST: Program Manager stable without hanging
  - Previous failure: SB Pro digitized sound hang preventing reach
  - Fix: Remove digitized-sound test from boot sequence OR fix DMA
```

**Step 3: Verify 32-bit Execution (Win32s + FreeCell)**
```
Expected:
- Win32s subsystem loads
- Launch FreeCell (pure 32-bit game)
- Gameplay smooth, no crashes
- Proves: 32-bit code execution on Inboard works identically to real hardware
```

**Known Issues to Manage**:
1. Mach8 diagnostic on startup - Real hardware shows no "RAM Addressing" grid
2. SB Pro digitized sound hang - Avoid digitized-sound programs
3. Check for spurious IRQs from misconfigured devices

**Deliverable**: Windows 3.11 reaches Program Manager, Win32s executes FreeCell

---

## Phase 3: Windows 95 XT Port

### Prerequisites (from VxD patches already done)
- VPICD patch applied (your VPICD_INBOARD.VXD) ✓
- VDMAD patch applied (your VDMAD_INBOARD.VXD) ✓
- VKD patch applied (your VKD_INBOARD.VXD) ✓
- VMM32 build prepared ✓
- All patches in `vxd-patches/` directory ✓

### Windows 95 Testing Plan
1. Boot Windows 95 install media on Inboard emulation
2. Run setup with all emulated hardware present
3. Track failures vs. real hardware (via Comrade real-time parallel session)
4. Adjust VxD patches as needed based on differences
5. Target: Full Windows 95 desktop boot

**Deliverable**: Windows 95 installs and boots on XT+Inboard configuration

---

## Implementation Priority Queue

### PHASE 1 - CRITICAL (Boot to DOS)
1. ✅ **Inboard CPU device** - Complete (already in code)
2. 🔧 **ROM loading (Sergey at 0xC8000)** - Add 1 line to machine_init()
3. 🔧 **T130B SCSI config** - Verify polling mode, add to machine table
4. 🔧 **3C509B NIC port** - Port from QEMU (1-2 hours)
5. 🔧 **Mach8 video config** - Add to machine table, CGA mode only
6. 🔧 **Sound Blaster config** - Add to machine table (note DMA limitation)

**Target**: Reach DOS prompt with all devices functional

### PHASE 2 - HIGH (Windows 3.11 Boot)
1. 🔧 **SmartWatch+ RTC** - Implement DS1315 phantom clock
2. 🔧 **Mach8 diagnostic bypass** - Handle ROM self-test display issue
3. 🔧 **Test Windows 3.11 boot** - Reach Program Manager
4. 🔧 **Test Win32s** - Run FreeCell
5. ✓ **Document working config** - Save machine profile

**Target**: Windows 3.11 Program Manager stable

### PHASE 3 - MEDIUM (Windows 95)
1. 🔧 **Windows 95 boot test** - Use existing VxD patches
2. 🔧 **Compare real vs. emulation** - Run Comrade in parallel
3. 🔧 **Adjust VxD patches** - Based on real hardware trace
4. ✓ **Full Windows 95 on XT+Inboard** - Final validation

**Target**: Windows 95 desktop boot

---

## Files & References

### Implementation Files
- `emulator/src/device/inboard386.c` - Inboard device ✓
- `emulator/src/machine/m_xt.c` - Machine init (needs ROM loading)
- `emulator/src/machine/machine_table.c` - Machine table (needs device configs)

### ROM Files Ready
- `roms/video/ATI_MACH8.bin` (32KB) ✓
- `roms/scsi/trantor_t130b_bios_v2.14.bin` (8KB) ✓
- `roms/network/Sergey_FDD.bin` (8KB) ✓

### Reference Materials
- `references/3c509b_qemu/` - QEMU patches for NIC porting ✓
- `references/ds1315_rtc/` - SmartWatch+ reference files ✓
- `hardware/5160_deep_dive_config.txt` - Real hardware specs ✓
- `vxd-patches/` - All Windows 95 VxD patches ✓

### Documentation
- `DEVICE_INTEGRATION_SPEC.md` - All device specs
- `HARDWARE_CONFIGURATION.md` - Real hardware inventory
- `INBOARD_86BOX_PORT_PLAN.md` - Detailed debugging notes
- `INBOARD_86BOX_BUILD_AND_RELEASE.md` (in COMrade_Latest) - Build guide

---

## Success Criteria

✅ **DOS Boot**: All drivers load, SCSI detected, time working  
✅ **Windows 3.11**: Program Manager opens and stays stable  
✅ **Win32s**: FreeCell runs without crashes  
✅ **Windows 95**: Boots to desktop with Inboard fully functional  
✅ **Real Hardware Parity**: Behavior matches Comrade/real 5160 exactly

---

## Token-Efficient Next Steps

**Recommend starting with:**
1. Add Sergey ROM loading (5 min) - 1 line of code
2. Verify T130B polling mode (10 min) - Configuration check
3. Port 3C509B from QEMU (45 min) - Main work
4. Add device configs to machine table (15 min) - Wiring
5. Build and boot test (30 min) - Verification

**Estimated time for Phase 1 completion**: 2 hours focused work

Ready to proceed?
