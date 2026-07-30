# Build & Test Plan - Next Session

## Current Status (Session End)

✅ **Code Changes Complete**:
- Sergey ROM loading added to m_xt.c (0xC8000)
- Device configs added to machine_table.c (Mach8, SB Pro, 3C509B stub)
- 3C509B device stub created (real port needed later)
- All changes committed (3 commits)

❌ **Blocker: Need Full 86Box Repository**

Current emulator/src has only modifications, not full 86Box. To build:

### Step 1: Clone Full 86Box Repository

```bash
cd C:\Users\lycet\RiderProjects\86Box-Inboard
git clone https://github.com/86Box/86Box.git emulator_full
cd emulator_full
git checkout main  # or desired version
```

### Step 2: Apply Our Modifications

Copy our modified files into full repository:
```bash
cp ../emulator/src/device/inboard386.c emulator_full/src/device/
cp ../emulator/src/include/86box/inboard386.h emulator_full/src/include/86box/
cp ../emulator/src/machine/m_xt.c emulator_full/src/machine/
cp ../emulator/src/machine/machine_table.c emulator_full/src/machine/
cp ../emulator/src/network/net_3c509b.c emulator_full/src/network/
```

### Step 3: Verify Device References

Machine table uses:
- `.vid_device = &mach8_isa_device` - must verify exists in full repo
- `.snd_device = &sb_pro2_device` - must verify exists in full repo  
- `.net_device = &nic_3c509b_device` - our stub

If device names differ, search full 86Box repo for correct names and update machine_table.c

### Step 4: Build

```bash
cd emulator_full
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 -DQT=OFF -DSDL2=ON ..
cmake --build . --config Release -j4
```

Result: `build/x64-Release/86Box.exe`

### Step 5: Test Windows 3.11

```bash
cd ../../xt5160_test_vm  # or your test VM directory
../emulator_full/build/x64-Release/86Box.exe -P vm
```

**Expected at DOS prompt**:
- All drivers load (INBRDPC.SYS, REVTO486.SYS, SCSI drivers)
- Time/date display (SmartWatch+ from Sergey ROM)
- `C:\>` prompt

**Then boot Windows 3.11 image**:
- Type: `WIN`
- Expected: Program Manager opens successfully

## Known Issues to Debug During Boot

### Issue 1: Mach8 Self-Test Display
- **Symptom**: Real hardware doesn't show "RAM Addressing" grid during POST
- **Reference**: INBOARD_86BOX_PORT_PLAN.md §3654-3178
- **Investigation**: Check if option ROM detection or diagnostic bypass needed
- **Action**: Compare emulator POST vs real 5160 POST (use Comrade in parallel)

### Issue 2: Sound Blaster Pro DMA Hang
- **Symptom**: Program Manager hangs if digitized sound programs load
- **Root Cause**: 86Box XT-class DMA bug (not Inboard-specific)
- **Reference**: INBOARD_86BOX_PORT_PLAN.md §3741-3800
- **Workaround**: FM synthesis works; digitized sound hangs
- **Action**: Can we bypass digitized sound in Windows 3.11 boot sequence?

## Success Criteria

✅ DOS boots with all devices
✅ Windows 3.11 boots to Program Manager
✅ Program Manager stable (no hang)
✅ Win32s runs FreeCell (proves 32-bit execution works)
✅ Document any Mach8/SB Pro workarounds

## Next Steps After Boot

1. Resolve Mach8 POST display issue
2. Resolve SB Pro DMA hang
3. Boot Windows 95 with VxD patches
4. Compare emulation vs real hardware (Comrade)
5. Full port of 3C509B NIC from QEMU (lower priority)

## Repository State

All changes in git, ready to merge with full 86Box:
- 11 commits total this project
- Sergey ROM loading: emulator/src/machine/m_xt.c
- Device configs: emulator/src/machine/machine_table.c
- 3C509B stub: emulator/src/network/net_3c509b.c
- Inboard device: emulator/src/device/inboard386.c (complete, 767 lines)

**No work lost. Ready to build next session.**
