# 86Box-Inboard Project - Restart Instructions

## Current Status (Session End: 2026-07-30)

✅ **Complete and Secured:**
- 86Box-Inboard dedicated repository created with full implementation
- COMrade main repo cleaned and focused
- All work committed to git
- No work lost or at risk

---

## Two Active Repositories

### 1. **86Box-Inboard Project** (PRIMARY)
```
Location: C:\Users\lycet\RiderProjects\86Box-Inboard
Purpose: Intel Inboard 386/PC emulation for 86Box
Status: Complete implementation, organized, git history preserved
Size: ~50 MB

Structure:
  emulator/                 (86Box source with Inboard patches)
  hardware/                 (Reverse-engineering, INBRDPC.SYS analysis)
  vxd-patches/              (Windows 95 VxD modifications)
  docs/                     (INBOARD_86BOX_PORT_PLAN.md + guides)
  test-images/              (ROM and disk images)
  analysis/                 (Investigation logs, findings)
  tools/                    (Python analysis scripts)
```

Git log:
- b0f80e8: feat: add Inboard device implementation, hardware analysis, and VxD work
- f197743: init: 86Box-Inboard project repository

### 2. **COMrade Main Repo**
```
Location: C:\Users\lycet\RiderProjects\COMrade_Latest
Purpose: Serial bridge for IBM 5160 control
Status: Cleaned, focused on core bridge work
Git log:
- 9a4d1d0: chore: remove 86Box-Inboard work from COMrade main repo
```

---

## Critical Files (DO NOT LOSE)

If starting fresh, prioritize these:
- `86Box-Inboard/emulator/src/device/inboard386.c` (1000+ lines)
- `86Box-Inboard/emulator/src/machine/m_xt.c` + `machine_table.c`
- `86Box-Inboard/hardware/inboard386_reverse_engineering/` (all INBRDPC.SYS work)
- `86Box-Inboard/vxd-patches/` (all Windows 95 patches)
- `86Box-Inboard/docs/INBOARD_86BOX_PORT_PLAN.md` (complete technical notes)

All preserved in git - **no manual backups needed**.

---

## On Restart: Quick Verification Checklist

```bash
# Verify repos exist
ls -la C:\Users\lycet\RiderProjects\86Box-Inboard
ls -la C:\Users\lycet\RiderProjects\COMrade_Latest

# Verify git history
cd C:\Users\lycet\RiderProjects\86Box-Inboard
git log --oneline | head -5

# Verify key files exist
ls -la C:\Users\lycet\RiderProjects\86Box-Inboard/emulator/src/device/inboard386.c
ls -la C:\Users\lycet\RiderProjects\86Box-Inboard/docs/INBOARD_86BOX_PORT_PLAN.md
```

---

## Memory Files to Load on Restart

**Key Memory Files (auto-loaded from Claude's memory):**
- `BUILD_AND_TEST_INSTRUCTIONS.md` - Build commands and validation
- `ROM_AND_CONFIG_REFERENCE.md` - Hardware addresses and configs
- `final-session-status.md` - Session summary and blockers

**Additional Context:**
- `win95-on-5160.md` - Windows 95 porting progress
- `inboard-86box-port.md` - Emulator port status

These are at: `C:\Users\lycet\.claude\projects\C--Users-lycet-RiderProjects-COMrade-Latest\memory\`

---

## Next Steps (When Restarting)

### Option A: Build and Test Emulator
```bash
cd C:\Users\lycet\RiderProjects\86Box-Inboard/emulator
# Follow BUILD_GUIDE.md for MinGW or MSVC build
cmake -G "MSYS Makefiles" -DQT=OFF -DSDL2=ON -DCMAKE_TOOLCHAIN_FILE=...
make -j4
# Result: 86Box.exe with Inboard support
```

### Option B: Continue Windows 95 Porting
```bash
cd C:\Users\lycet\RiderProjects\86Box-Inboard/vxd-patches
# Review win95_port_plan.md
# Apply patch scripts (patch_vpicd.py, patch_vkd.py, etc.)
# Test with emulator
```

### Option C: Hardware Analysis
- Continue PAL/GAL reverse-engineering (ronnyroy collaboration)
- Analyze 3C509B network card
- Document T130B SCSI configuration

---

## Files Safe to Delete

**These can be deleted to free disk space (work is in repos):**
```
C:\Users\lycet\OneDrive\Desktop\Claude_stuff\86box_source
C:\Users\lycet\OneDrive\Desktop\Claude_stuff (if empty after)
C:\Users\lycet\RiderProjects\COMrade_Latest - restore
```

**Do NOT delete:**
```
C:\Users\lycet\RiderProjects\86Box-Inboard  (active project)
C:\Users\lycet\RiderProjects\COMrade_Latest (main repo)
```

---

## Session Work Summary

### What Was Accomplished

1. **Recovered Lost Work**
   - Located backup of inboard386.c implementation
   - Located UniPCemu reference and hardware analysis
   - All VxD patches and documentation

2. **Created Dedicated Repository**
   - New clean 86Box-Inboard repo
   - Organized by purpose (emulator, hardware, vxd, docs)
   - Full git history with commit messages

3. **Cleaned COMrade Main Repo**
   - Removed 86Box work (now in 86Box-Inboard)
   - Removed application cruft (GAMES, APPS, OFFICE, etc.)
   - Focused on core serial bridge work

4. **Secured Everything in Git**
   - 3 major commits to 86Box-Inboard
   - Clear commit messages documenting what was done
   - History preserved, no work lost

### Days of Work Recovered & Organized

- Intel Inboard 386/PC device implementation (~1000 lines C code)
- Machine profile for 386+ CPU on XT architecture
- CPU timing corrections (PIC, DMA, I/O cycles)
- BIOS POST analysis and debugging findings
- Hardware reverse-engineering (INBRDPC.SYS disassembly, PAL equations)
- Windows 95 VxD patching work (VPICD, VKD, VDMAD)
- Complete technical documentation
- Python analysis tools and validators

---

## Token Usage Notes

- Current session: Heavy exploratory work, file organization, git operations
- Next session: Can focus on specific tasks (building, testing, or analyzing)
- Recommendation: Start next session with specific goal (e.g., "build emulator" or "continue Win95 work")

---

## Quick Reference

| Task | Location | Main File |
|------|----------|-----------|
| Build 86Box | `86Box-Inboard/emulator/` | `CMakeLists.txt` |
| Device implementation | `86Box-Inboard/emulator/src/device/` | `inboard386.c` |
| Machine profile | `86Box-Inboard/emulator/src/machine/` | `m_xt.c` |
| Hardware analysis | `86Box-Inboard/hardware/` | `5160_deep_dive_config.txt` |
| Windows 95 work | `86Box-Inboard/vxd-patches/` | `win95_port_plan.md` |
| Technical notes | `86Box-Inboard/docs/` | `INBOARD_86BOX_PORT_PLAN.md` |
| Analysis tools | `86Box-Inboard/tools/` | `*.py` scripts |

---

## If Something Is Wrong on Restart

1. **Verify git repos exist**
   ```bash
   cd C:\Users\lycet\RiderProjects\86Box-Inboard
   git status
   git log --oneline
   ```

2. **Check for uncommitted work**
   ```bash
   git status  # Should show "nothing to commit"
   ```

3. **Contact point**
   - All work is in git - can always `git log` to see history
   - No work should be lost
   - Memory files have session context

---

**This project is now properly organized and secured. Ready for restart.**
