# Hardware Configuration - Intel Inboard 5160 Emulation

**Status**: ⚠️ INCOMPLETE - Devices are currently not configured in machine profile

**Real Hardware**: Intel 5160 + Inboard 386/PC with full ISA expansion

## Detailed Hardware Inventory

Your real 5160+Inboard has 8 ISA cards installed. Current machine profile is **missing 7 of them**.

### ISA Slot Configuration

#### ✅ Slot 5: Intel Inboard 386/PC (IMPLEMENTED)
- **Purpose**: CPU accelerator (not a video/sound/network card, but system device)
- **CPU**: IBM 486BL3 (Blue Lightning)
- **Crystal**: 40MHz (modified from 32MHz stock)
- **RAM**: 1MB onboard + 4MB community piggyback = 5MB total
- **Status**: ✅ Fully implemented in `emulator/src/device/inboard386.c`
- **Machine Config**: References `inboard386_xt_device`

---

#### ❌ Slot 1: ATI Ultra Mach 8 (VIDEO - NOT CONFIGURED)
- **Hardware**: ATI Ultra Mach 8 graphics card
- **Bus Mode**: 8-bit ISA (JU1 top two pins)
- **BIOS ROM**: Yes (option ROM)
- **Memory**: 512KB-1MB video RAM
- **Jumpers**: JU3 all open (2x3 pins)

**86Box Configuration Needed**:
```c
.vid_device = &ati_mach8_device  // or similar ATI Mach8 reference
```

**Current Status**: `.vid_device = NULL` - This will cause no display output!

---

#### ❌ Slot 2: Sound Blaster Pro 2 CT1600 (SOUND - NOT CONFIGURED)
- **Hardware**: Creative Sound Blaster Pro 2, model CT1600
- **Audio Features**: 16-bit stereo, MIDI, joystick port
- **IRQ**: 5 (JP20 jumped)
- **DMA**: 1 (JP6 jumped)
- **Base Address**: 220H (JP13 = 22x)
- **I/O Ports**: 220H-22FH (standard SB Pro 2 range)
- **BIOS ROM**: No
- **Connections**:
  - CD audio input from Nakamichi CD drive
  - Microphone (external)
  - Speakers (external)

**86Box Configuration Needed**:
```c
.snd_device = &sb16_pnp_device  // or &sb_pro2_device if available
// In machine_ibmxt_inboard386_init():
//   snd_card_init() with address 0x220, IRQ 5, DMA 1
```

**Current Status**: `.snd_device = NULL` - No audio output!

---

#### ❌ Slot 3: 3Com Etherlink III (3C509B) (NETWORK - NOT CONFIGURED)
- **Hardware**: 3Com Etherlink III, model 3C509B-C
- **Type**: ISA Ethernet network card
- **Configuration**: Entirely software-controlled (no jumpers)
- **BIOS ROM**: Yes (hosting Sergey floppy drive ROM)
- **ROM Location**: Loaded at user request
- **Feature**: Provides ROM boot support for Sergey floppy controller

**86Box Configuration Needed**:
```c
.net_device = &nic_3c509_device  // 3Com 3C509B
// May need to configure ROM at appropriate address
```

**Current Status**: `.net_device = NULL` - No network capability!

**Important**: This card has a dual purpose - it's both a network card AND provides ROM boot support for the Sergey controller in Slot 6. The ROM hosting is critical for floppy access!

---

#### ❌ Slot 4: T130B SCSI Controller (STORAGE - NOT CONFIGURED)
- **Hardware**: T130B SCSI controller card
- **SCSI Interface**: 8-bit SCSI, single-ended
- **Jumper Settings**:
  - **SW1 (Address Selection)**:
    - Bit 1: Off (normal)
    - Bit 2: On → **I/O Address: 0x340**
    - Bits 3-4: Off
    - Bit 5: On
    - Bit 6-8: Off
  - **ROM Address**: 0xCA000 (upper memory, 16KB ROM)
  - **Zero Wait State**: JP2 jumped (enabled)
  - **IRQ**: JP3 (all unpopulated) - **Running WITHOUT IRQ** (polling mode)

**SCSI Devices**: Not documented in your notes - would typically be:
- Hard drive(s) on SCSI bus
- Possibly CD-ROM or tape drive

**86Box Configuration Needed**:
```c
// No standard .hdc_device parameter exists for ISA SCSI
// Requires custom device implementation or ISA device in machine_init()
.hdc_device = &t130b_scsi_device  // If 86Box supports T130B
// OR manual device_add() in machine_ibmxt_inboard386_init():
//   device_add(&t130b_scsi_device);
```

**Current Status**: ❌ Not configured at all - No SCSI/hard drive access!

**Critical Gap**: The T130B is essential for persistent storage. Without it, your machine can only use:
- Floppy drives (via Sergey controller in Slot 6)
- RAM disk if supported
- No hard drive access

---

#### ❌ Slot 6: Sergey Floppy/Serial Controller (FLOPPY + SERIAL - PARTIALLY CONFIGURED)
- **Hardware**: ISA Floppy Disk and Serial Controller, Sergey Kisalev 2012
- **Two Functions**:
  1. **Floppy Controller** - Hosts custom ROM
  2. **Serial Port (COM1)** - RS-232 interface

**Floppy Configuration**:
- **ROM Enable**: SW2 pins 1-2 (On) - EEPROM write enabled
- **ROM Address**: 0xC8000 (64KB - 8KB region)
- **ROM Contents**: SmartWatch+ by dJos
- **Purpose**: Custom boot ROM for non-standard floppy formats

**Serial Configuration**:
- **IRQ**: 4 (SW1 bit 2 On) - **COM1 IRQ**
- **Port Address**: 0x3F8 (standard COM1)
- **Configuration**: SW1-7 Off (standard, not weird addresses)

**86Box Configuration Needed**:
```c
// Floppy:
.fdc_device = &fdc_xt_device  // Standard XT floppy (uses IRQ 6, DMA 2)
// BUT: This is the *base* controller. The Sergey card is an *add-on*.
// May require custom integration or separate device.

// Serial:
// Standard serial port, usually auto-configured in machine_xt_common_init()
// May need manual configuration if non-standard settings required
```

**Current Status**: 
- `.fdc_device = NULL` - No floppy drives!
- Serial port: Unknown if configured in `machine_xt_common_init()`

---

#### ❌ Slot 7: Intel 21 TK9901 ECP/EPP Parallel Port (PARALLEL - NOT CONFIGURED)
- **Hardware**: Intel 21 TK9901 ECP/EPP controller (IEEE 1284)
- **Speed**: Up to 2 MB/s (high-speed parallel)
- **Modes**: ECP (Extended Capabilities Port) and EPP (Enhanced Parallel Port)
- **Base Address**: 0x378 (JP6, pins 2-3) - **LPT1**
- **IRQ**: 7 (JP4, pins 2-3)
- **DMA**: 3 (JP5 and JP3, pins 2-3)
- **Configuration**: All jumpers JP1-JP6, pins 2-3 jumped (ECP/EPP mode enabled)

**Typical Use**: Printer or scanner connection via high-speed parallel

**86Box Configuration Needed**:
```c
// 86Box has standard LPT device support
// May need device_add() in machine init if not auto-configured:
//   device_add(&lpt1_device);  // or lpt_port_device with config
```

**Current Status**: Unknown - likely missing since not in machine table

---

#### ❌ Slot 8: XT-IDE Universal BIOS (IDE/CF CONTROLLER - NOT CONFIGURED)
- **Hardware**: LoTech XT-CF v20 IDE controller with XT-IDE Universal BIOS
- **Interface**: IDE (Parallel ATA)
- **Storage Media**: CompactFlash (CF) cards acting as hard drive
- **BIOS ROM**: XT-IDE Universal BIOS
- **BIOS Address**: 0xD800 (JP2 open - default address, 16KB)
- **ROM Enable**: JP1 jumped (ROM enable)
- **Slot Enable**: JP3 jumped (slot 8 enable)

**Storage Devices**: Not documented - typical XT-IDE setup:
- Primary IDE: CF card as C: drive
- Secondary IDE: Optional CD-ROM emulation or second CF card

**86Box Configuration Needed**:
```c
// May require custom XT-IDE device or standard IDE configuration
// Typically accessed via custom ROM that patches INT 13h
// Would need:
// .hdc_device = &xtide_device  // If 86Box has XT-IDE support
// OR manual device_add() with appropriate IDE controller
```

**Current Status**: ❌ Not configured - No IDE/CF card access!

---

### Storage Summary

Your real hardware has **2 independent storage subsystems**:

1. **Floppy**: Sergey controller in Slot 6
   - Standard 3.5" and/or 5.25" floppy drives
   - Custom ROM for non-standard formats
   - **Emulation Status**: ❌ Not configured

2. **Hard Disk**: Multiple options
   - **T130B SCSI** (Slot 4): Primary option (not configured)
   - **XT-IDE CF** (Slot 8): Secondary option (not configured)
   - **Current Emulation**: Neither configured - only RAM available!

---

## What's Actually Implemented

### In Current Machine Profile
```c
// From machine_table.c (ibmxt_inboard386 entry):
.vid_device    = NULL,           // ❌ Missing
.snd_device    = NULL,           // ❌ Missing
.net_device    = NULL,           // ❌ Missing
.fdc_device    = NULL,           // ❌ Missing
.hdc_device    = Not shown        // ❌ Missing (no SCSI/IDE)
.device        = &ibmxt_inboard386_device  // ✅ Just the Inboard card itself
```

### What Gets Added Implicitly
In `machine_ibmxt_inboard386_init()`:
```c
device_add(&kbc_xt_device);           // ✅ Keyboard
machine_xt_common_init(model, 0);    // ✅ Adds standard XT base
if (enable_5161)
    device_add(&ibm_5161_device);    // ✅ Optional: 5161 receiver
device_add(&inboard386_xt_device);   // ✅ Inboard hardware itself
```

---

## Emulation Gaps (Critical)

| Device | Real Hardware | Emulated | Impact |
|--------|---------------|----------|--------|
| Video Card | ATI Mach8 | ❌ None | **No display output** |
| Sound Card | Sound Blaster Pro 2 | ❌ None | **No audio** |
| Network | 3Com 3C509B | ❌ None | **No networking** |
| SCSI Controller | T130B | ❌ None | **No hard drive access** |
| Floppy Controller | Sergey | ❌ None | **No floppy access** |
| IDE Controller | XT-IDE | ❌ None | **No CF card access** |
| Parallel Port | Intel 21 TK9901 | ❌ None | **No parallel device** |
| **CPU Accelerator** | Inboard 386/PC | ✅ Full | **CPU and timing OK** |
| **Base XT System** | IBM 5160 | ✅ Partial | **POST partially works** |

---

## Why This Matters

The current configuration has only the Inboard device but **none of the ISA peripherals**. This means:

1. **No Display Output** - CRT monitor will be black
2. **No Audio** - CD audio cannot play, MIDI cannot work
3. **No Persistent Storage** - Can't save files to disk (except floppy in theory)
4. **No Networking** - Cannot access network shares or external devices
5. **No Printer/Scanner** - Parallel port devices cannot be used
6. **Windows 95 Installation**: Cannot complete without hard drive, cannot verify video works

The machine will boot to a DOS prompt in emulation, but you won't see it or be able to interact with it meaningfully.

---

## What Needs to Be Done

### Tier 1 (Required for Basic Operation)

1. **Video Device Configuration**
   - Add ATI Mach8 or compatible video device to machine table
   - Ensure BIOS ROM loading for video card ROM
   - Test basic display output

2. **Floppy/Serial Controller**
   - Ensure Sergey controller ROM is loaded at 0xC8000
   - Verify serial port (COM1 on IRQ 4) works
   - Test floppy drive access

3. **Storage Selection**
   - Choose either T130B SCSI **or** XT-IDE for primary storage
   - Implement device in machine_init() if no standard table entry exists
   - Create hard drive image(s) for storage

### Tier 2 (Important for Full Compatibility)

4. **Sound Card (Sound Blaster Pro 2)**
   - Add to machine table with IRQ 5, DMA 1, address 0x220
   - Verify joystick port configuration
   - Test audio input/output

5. **Network Card (3C509B)**
   - Add to machine table
   - Ensure ROM is loaded for Sergey controller boot support
   - Configure network settings

### Tier 3 (Nice-to-Have)

6. **Parallel Port**
   - Configure Intel 21 TK9901 or standard LPT1
   - Enable for future printer/scanner emulation

---

## 86Box Device Availability Check

**ACTION NEEDED**: Research 86Box source tree to determine:

1. **Does 86Box have ATI Mach8 support?**
   - Search: `ati_mach8*`, `mach8*`, `ati_video*`
   - If not: May need generic VGA/VBE fallback

2. **Does 86Box have Sound Blaster Pro 2 support?**
   - Search: `sb_pro2*`, `sb16*`, `sb_vibra*`
   - If not: Use closest compatible Sound Blaster variant

3. **Does 86Box have 3Com 3C509B support?**
   - Search: `nic_3c509*`, `ethernet_3c509*`
   - If not: Use NE2000 or other ISA Ethernet card

4. **Does 86Box have T130B SCSI support?**
   - Search: `t130b*`, `scsi*`, `aha*`
   - If not: Use generic SCSI adapter if available

5. **Does 86Box have XT-IDE support?**
   - Search: `xtide*`, `cf_ide*`, `ide_xt*`
   - If not: Use standard IDE controller with CF card emulation

---

## Next Steps

1. **Review full 86Box source tree** to identify available devices
2. **Update machine table entry** with appropriate device references
3. **Enhance machine_ibmxt_inboard386_init()** to add any non-table devices
4. **Test each device** to verify proper integration
5. **Create hardware configuration documentation** for users

**Current implementation**: Inboard CPU emulation is excellent, but peripheral support is currently missing.

---

**Priority**: HIGH - The machine cannot be used for Windows 95 testing or any practical work without at least video and storage.
