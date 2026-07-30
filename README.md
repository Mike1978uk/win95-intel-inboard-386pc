# 86Box-Inboard: Intel Inboard 386/PC Emulation

Complete emulation implementation with hardware analysis, Windows 95 porting work, and full documentation.

## Structure
- **emulator/** - 86Box source with Inboard device, machine profile, timing fixes
- **hardware/** - INBRDPC.SYS reverse-engineering, PAL/GAL analysis
- **vxd-patches/** - Windows 95 VPICD, VKD, VDMAD modifications
- **docs/** - Project documentation and guides
- **test-images/** - ROM and disk images
- **analysis/** - Investigation logs and technical findings
- **tools/** - Python analysis utilities

## Key Implementation
- Device: A20 gating, speed control, ROM shadow (src/device/inboard386.c)
- Machine: ibmxt_inboard386 profile with CPU_IBM486BL support
- Timing: I/O waitstates, ROM prefetch, memory timing, ISA speed corrections
- Fixes: PIC IMR timing, DMA refresh, register-op delays

See docs/ARCHITECTURE.md for full details.
