# 86Box Inboard Emulation - Device Integration Specification

**Status**: Implementation Plan  
**Date**: 2026-07-30  
**Real Hardware**: IBM 5160 + Intel Inboard 386/PC with full ISA peripherals

---

## Overview

Your 5160+Inboard system has 8 ISA cards. Current emulation status:

| Slot | Device | Status | Priority | Implementation |
|------|--------|--------|----------|-----------------|
| 1 | ATI Ultra Mach8 (Video) | ✅ Available in 86Box | High | Configure in machine table |
| 2 | Sound Blaster Pro 2 CT1600 | ✅ Available in 86Box | High | Configure in machine table |
| 3 | 3Com Etherlink III (3C509B) | 🔧 Partial | High | Port from QEMU reference |
| 4 | Trantor T130B SCSI | ✅ Available in 86Box | **CRITICAL** | Add IRQ=Disabled + zero_wait flag |
| 5 | Intel Inboard 386/PC | ✅ **IMPLEMENTED** | Core | Done (emulator/src/device/inboard386.c) |
| 6 | Sergey Floppy + Serial | ⚠️ Partial | Medium | ROM loading, serial port config |
| 7 | Intel 21 TK9901 ECP/EPP | ⚠️ Partial | Low | Standard LPT configuration |
| 8 | XT-IDE Universal BIOS | ⚠️ Partial | Low | IDE controller with CF card |

---

## Device Implementation Details

### 1. CRITICAL: Trantor T130B SCSI Controller

**Your Hardware Config** (confirmed from hardware deep-dive):
- **I/O Address**: 0x340 (SW1 bit 2 On)
- **ROM Address**: 0xCA000 (16KB)
- **IRQ**: DISABLED (All IRQ jumpers JP3 open - no IRQ 3/5/7)
- **Wait States**: Zero wait state ENABLED (JP2 jumped)
- **Operation Mode**: Polling only (no interrupts)

**86Box Status**:
- Device exists: `scsi_t130b_device` in `src/scsi/scsi_ncr53c400.c`
- ROM available: `roms/scsi/trantor_t130b_bios_v2.14.bin` ✓ (copied to repo)
- **Work needed**: Verify/implement `irq = -1` (Disabled) and `zero_wait` flag support

**Integration Steps**:
1. Check `scsi_ncr53c400.c` for `irq = -1` handling (maps to no IRQ)
2. Verify `zero_wait` flag exists and scales timing correctly
3. Add to machine table:
   ```c
   .scsicard = {
       .dev = &scsi_t130b_device,
       .irq = -1,
       .zero_wait = 1
   }
   ```
4. Test: DOS boot should detect SCSI, no IRQ conflict, faster transfers with zero_wait

**Reference**: `INBOARD_86BOX_PORT_PLAN.md` §3707-3722 (real root-cause debugging)

---

### 2. HIGH: 3Com Etherlink III (3C509B) Network Card

**Your Hardware Config** (confirmed from hardware deep-dive):
- **Type**: 3C509B-C (software-configurable Ethernet card)
- **I/O Base**: 0x300 (standard 3C509 default)
- **ID Port**: 0x110 (per 3C509 protocol)
- **IRQ**: 3 (JP... setting unknown, verify on real card)
- **MAC Address**: 00:20:AF:6F:10:5E (real hardware MAC from 2026-07-30 trace)
- **ROM**: Hosts Sergey floppy drive custom ROM

**86Box Status**:
- No native 3C509B implementation (Copilot noted this)
- Partial implementation attempted: reference in `net_3c501.c` (temp alias only)
- QEMU reference available: `references/3c509b_qemu/` (3 patch files)

**Work Needed** (HIGH PRIORITY):
1. **Port QEMU 3C509B to 86Box**:
   - Review QEMU patches in `references/3c509b_qemu/`:
     - `qemu-3c509b.patch2` - Main 3C509B implementation
     - `qemu-isapnp.patch2` - ISA Plug-and-Play handling
     - `qemu-pcnet.patch6` - PCnet adapter compatibility
   - Create `emulator/src/network/net_3c509b.c` based on QEMU code
   - Adapt I/O handlers for 86Box's `io_sethandler()` API
   - Register MAC address and configuration

2. **Register in build system**:
   - Add to `emulator/src/network/CMakeLists.txt`
   - Export device definition in header file

3. **Machine table integration**:
   ```c
   .net_device = &nic_3c509b_device,
   // Configuration: I/O 0x300, IRQ 3, MAC 00:20:AF:6F:10:5E
   ```

4. **Test**: Boot DOS with 3C509B driver, verify network access

**References**:
- QEMU patches: `references/3c509b_qemu/`
- Port plan: `INBOARD_86BOX_PORT_PLAN.md` §3108-3129 (Copilot summary)
- Real hardware config: `HARDWARE_CONFIGURATION.md` Slot 3

---

### 3. HIGH: ATI Ultra Mach8 Graphics Card

**Your Hardware Config**:
- **Card**: ATI Ultra Mach8 ISA (8-bit mode)
- **ROM**: Real ATI Mach8 ROM dump `roms/video/ATI_MACH8.bin` ✓ (copied)
- **Bus Mode**: 8-bit ISA (JU1 top two pins jumped)
- **BIOS**: Requires option ROM loading

**86Box Status**:
- ✅ Native Mach8 support exists: `vid_mach8_isa_device` (or similar)
- ✅ Used in working config: `gfxcard = mach8`

**Machine Table Entry**:
```c
.vid_device = &mach8_isa_device,
// Ensure JU1 set to 8-bit mode in config
```

**Caveats** (from previous sessions):
- CGA mode works cleanly; VGA reintroduces POST timing bugs
- Real self-test behavior doesn't match emulation (RAM addressing grid)
- Register divergence on accelerator engine (0x92E8/0xC2E8/0xDAE8 bits)

**Status**: Ready to integrate, monitor for timing issues

**References**: `INBOARD_86BOX_PORT_PLAN.md` §3654-3178 (Mach8 debugging details)

---

### 4. MEDIUM: Sound Blaster Pro 2 CT1600

**Your Hardware Config**:
- **Card**: Creative Sound Blaster Pro 2, model CT1600
- **I/O Base**: 0x220 (JP13 = 22x)
- **IRQ**: 5 (JP20 jumped)
- **DMA**: 1 (JP6 jumped)
- **Features**: 16-bit stereo, MIDI, joystick port

**86Box Status**:
- ✅ Multiple SoundBlaster variants available: `sb_pro2_device` or `sb16_device`
- ⚠️ **Known Issue**: Digitized sound DMA completion hangs on XT configs (general 86Box bug, not Inboard-specific)

**Machine Table Entry**:
```c
.snd_device = &sb_pro2_device,  // or sb16_device if closer match exists
// IRQ 5, DMA 1, I/O 0x220
```

**Known Workaround** (from `INBOARD_86BOX_PORT_PLAN.md` §3741-3800):
- Digitized sound hangs confirmed on real XT hardware (FM synthesis works)
- Bug exists in 86Box's `dma_xt8237` implementation (`dma.c`)
- Not Inboard-specific; affects any true XT-class machine
- Current fix: Don't use digitized sound; FM synthesis only

**Status**: Integrate but expect known DMA limitation

---

### 5. MEDIUM: Sergey Floppy/Serial Controller (Slot 6)

**Your Hardware Config**:
- **Floppy ROM**: Custom ROM at 0xC8000 (SmartWatch+ by dJos)
- **ROM File**: `roms/network/Sergey_FDD.bin` ✓ (copied, 8KB)
- **Serial Port**: COM1 at 0x3F8, IRQ 4
- **ROM Configuration**: SW2 pins 1-2 On (enable ROM), EEPROM write on
- **Operation**: Supports non-standard floppy formats via custom ROM

**86Box Status**:
- ✅ Standard XT floppy controller exists: `fdc_xt_device`
- ✅ Serial port auto-configured in `machine_xt_common_init()`
- ⚠️ Custom Sergey ROM loading needs special handling

**Work Needed**:
1. Ensure ROM loads at 0xC8000 in machine init
2. Verify serial port IRQ 4 (standard COM1) configured correctly
3. Test floppy access with custom ROM

**Machine Table Integration**:
```c
// Standard XT floppy already added via machine_xt_common_init()
// Serial port: standard, no special config needed
// ROM loading: add in machine_ibmxt_inboard386_init():
//   bios_load_aux_linear(<rom_path>, 0xc8000, 8192, 0);
```

**Status**: Mostly ready; just needs ROM loading verification

---

### 6. MEDIUM: Intel 21 TK9901 ECP/EPP Parallel Port

**Your Hardware Config**:
- **Card**: Intel 21 TK9901 ISA parallel port
- **Address**: 0x378 (LPT1 standard)
- **IRQ**: 7 (JP4 pins 2-3)
- **DMA**: 3 (JP5, JP3 pins 2-3)
- **Modes**: ECP/EPP enabled (all jumpers 2-3 jumped)
- **Speed**: Up to 2 MB/s high-speed parallel

**86Box Status**:
- ✅ Standard LPT support exists: `lpt_port_device` or `lpt1_device`
- Typically auto-configured; may need explicit registration

**Machine Table Entry**:
```c
// Check if auto-configured in machine_xt_common_init()
// If not, add: device_add(&lpt_port_device);
// Standard configuration: 0x378, IRQ 7
```

**Status**: Low priority for now; standard config should work

---

### 7. LOW: XT-IDE Universal BIOS (Slot 8)

**Your Hardware Config**:
- **Card**: LoTech XT-CF v20 with XT-IDE Universal BIOS
- **Interface**: IDE/Parallel ATA
- **Media**: CompactFlash (CF) card as hard drive
- **ROM**: XT-IDE Universal BIOS at 0xD800 (JP2 open)
- **ROM Enable**: JP1 jumped

**86Box Status**:
- ⚠️ XT-IDE specific support unclear
- Generic IDE controller available: may work as fallback
- Alternative: Use T130B SCSI (Slot 4) for primary storage

**Work Needed**:
1. Verify 86Box can load XT-IDE BIOS at 0xD800
2. Create CF card image for storage
3. Test boot from XT-IDE

**Lower Priority**: T130B SCSI is primary storage solution; XT-IDE is secondary option

---

## Implementation Priorities

### Phase 1 (Critical - Core Emulation):
- ✅ Inboard 386/PC device (DONE)
- 🔧 T130B SCSI with IRQ=Disabled + zero_wait
- 🔧 3C509B NIC (port from QEMU)

### Phase 2 (Important - Full Compatibility):
- 🔧 ATI Mach8 integration (verify timing)
- 🔧 Sound Blaster Pro 2 integration (understand DMA bug)
- 🔧 Sergey ROM loading

### Phase 3 (Nice-to-Have):
- Parallel port (LPT1)
- XT-IDE controller

---

## Files Reference

**Source Files**:
- Device implementation: `emulator/src/device/inboard386.c` ✓
- Machine profile: `emulator/src/machine/m_xt.c` ✓
- Machine table: `emulator/src/machine/machine_table.c` ✓

**ROM Files** (copied to repo):
- Video: `roms/video/ATI_MACH8.bin` (32KB)
- SCSI: `roms/scsi/trantor_t130b_bios_v2.14.bin` (8KB)
- Network: `roms/network/Sergey_FDD.bin` (8KB) [Floppy ROM]

**Reference Materials** (copied to repo):
- 3C509B QEMU: `references/3c509b_qemu/` (3 patch files)
- DS1315 RTC: `references/ds1315_rtc/` (SmartWatch+ files)

**Documentation**:
- Hardware config: `HARDWARE_CONFIGURATION.md`
- Build status: From COMrade_Latest (to be migrated)
- Port plan: From COMrade_Latest (extensive debugging notes)

---

## Build Commands (Once Devices Implemented)

```bash
cd C:\Users\lycet\RiderProjects\86Box-Inboard\emulator
cmake -B build -G "Visual Studio 17 2022" -A x64 -DQT=OFF -DSDL2=ON
cmake --build build -j4
# Verify devices appear:
./build/x64-Release/86Box.exe -l | findstr "t130b 3c509 Mach8"
```

---

## Real Hardware Configuration Summary

**Your Machine**:
- CPU: IBM 486BL3 (Blue Lightning) at ~83.5MHz
- RAM: 1MB onboard + 4MB piggyback = 5MB total
- Video: ATI Ultra Mach8 (8-bit ISA)
- Audio: Sound Blaster Pro 2 CT1600 (IRQ 5, DMA 1)
- Network: 3Com Etherlink III 3C509B (IRQ 3)
- Storage: Trantor T130B SCSI (I/O 0x340, no IRQ, zero wait)
- Floppy: Sergey controller with custom ROM
- Parallel: Intel 21 TK9901 ECP/EPP (IRQ 7, DMA 3)
- IDE: XT-IDE with CF card (ROM D800H)

**Expected Behavior**:
- Cold boot: BIOS POST with all hardware detection
- DOS boot: All drivers load cleanly
- Windows 95: Should load and run with proper timing calibration
- Test: Each subsystem functional per BIOS/driver diagnostics

---

**Next Actions**:
1. Implement T130B IRQ=Disabled + zero_wait support
2. Port 3C509B from QEMU to 86Box
3. Configure ATI Mach8, Sound Blaster in machine table
4. Test each device subsystem
5. Validate Windows 95 installation/boot
