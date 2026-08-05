# Al Williams / Intel Inboard A20 correspondence (Feb 2023) - THE ACTUAL SOURCE CODE

Source: private email thread, Mike Lycett <-> Al Williams (al.williams@awce.com), Feb 2023,
"Intel Inboard chat" (10 messages). Saved 2026-08-04 during the OSR1 Windows 95 keyboard/A20
investigation. Images extracted from the email and saved to `docs/al_williams_images/` in this
repo (permanent copies, independent of the OneDrive/email source).

Al Williams is a real, published technical author (Dr. Dobb's Journal contributor, Hackaday
writer) with direct, hands-on 1990s-era experience programming the Intel Inboard 386/PC
specifically, including real compatibility testing across multiple 386 machines.

## THE ACTUAL CODE (transcribed from the email images, `docs/al_williams_images/`)

Source: Dr. Dobb's Journal Vol. 15 (1990), "DOS + 386 = 4 Gigabytes!" by Al Williams (July 1990
issue), with a later corrected/errata version. Public scan:
https://archive.org/details/dr_dobbs_journal_vol_15/page/613/mode/2up

### Port/constant definitions (`2_keyboard_controller_defines.png`)

```c
/* Keyboard controller defines */
#define RAMPORT     0x70
#define KB_PORT     0x64
#define PCNMIPORT   0xA0
#define INBA20      0x60
#define INBA20ON    0xDF
#define INBA20OFF   0xDD
```

### Original code, as printed in the July 1990 article (`1_original_a20_code.png`)

```c
/* macro to clear keyboard port */
#define keywait() { while (inp(KB_PORT)&2); }

/*******************************************************************
 * General purpose routine to allow A20 (flag=1) or disable A20 (flag=0) *
 *******************************************************************/
void a20(int flag)
  {
  if (inboard)
    {
    outp(INBA20,flag?INBA20ON:INBA20OFF);
    }
  else
    {
    keywait();
    outp(KB_PORT,flag?0xbc:0xb4);
    keywait();
    outp(KB_PORT,flag?0xbc:0xb4);
    keywait();
    }
  }
```

### Corrected/errata version, published in a later issue (`3_errata_corrected_code.png`)

A reader letter reported the original code failed on some 386 machines (Compaq, Dell). Al's
reply supplied a fix - **only the `else` (non-Inboard) branch changed**:

```c
/* This code is the same */
void a20(int flag)
    {
    if (inboard)    {
        outp(INBA20,flag?INBA20ON:INBA20OFF);   /* NOTE: printed with a typo, see below */
        }
/* changes start here */
    else
        {
        keywait();
        outp(0x64,0xD1);
        keywait();
        outp(0x60,flag?0xDF:0XDD);
        keywait();
        outp(0x64,0xFF);
        keywait();
        }
    }
```

Al's letter: *"The code in the article works on some 386 machines, but fails on Compaq and Dell
computers. The code above works on all machines I tested, including Everex, Dell, Compaq, and
CompuAdd."*

**Known typo** (confirmed directly by Al in a follow-up email, 2023): the `if (inboard)` branch as
typeset in the errata letter dropped the "ON:" from the 3-way conditional, printing as
`outp(INBA20,flag?INBA20OFF)` instead of the correct `outp(INBA20,flag?INBA20ON:INBA20OFF)`. Al
confirmed: *"The first inboard code is correct"* - meaning **use the `if(inboard)` branch exactly
as printed in the ORIGINAL July 1990 article** (`1_original_a20_code.png`, shown above, which is
correct and complete), not the typo'd reprint. The `else` branch in the errata version is the real
fix and has no typo.

### Second errata letter (`4_errata_letter2_cs_register.png`)

From Thomas Roden (Irvine, CA), discussing CS-register reloading and far jumps when entering/
exiting protected mode, with Al's response. **Not directly relevant to A20/keyboard** - a separate
concern about protected-mode transition robustness on "yet-to-be-released 80386 compatible
processors," not about this hardware's keyboard/A20 quirks. Kept for completeness.

## WHY THIS MATTERS - direct connection to this session's own findings

This project's OSR1 Windows 95 investigation (2026-08-04, see
`memory/osr1_pivot_and_fidelity_pass_2026_08_04.md`) independently live-traced a real-mode loop at
segment `0325:0471` repeatedly executing (2067+ times):

```
OUT port=0064 val=D1
OUT port=0060 val=DF   (or DD)
OUT port=0064 val=FF
```

**This is an exact, literal match to Al's `else` (non-Inboard/generic-AT) branch of `a20()`** from
his own corrected 1990 code. This is not a coincidence - it's the same routine, the same era, the
same hardware-compatibility problem.

**What this proves**: some real-mode component in this project's OSR1 boot chain (segment `0325`,
identity not yet confirmed - candidates: `HIMEM.SYS`, or a similarly-generic A20 handler; NOT
`INBRDPC.SYS` itself, which is independently confirmed throughout this project's history to
correctly use the direct `INBA20`/port-0x60 method) is **failing its own `if (inboard)` check** and
falling through to the generic AT-compatible fallback path instead of the correct, direct
Inboard-native path.

**What this does NOT necessarily mean**: the actual A20 *value* written to port 0x60 is identical
either way (`0xDF`/`0xDD`) - so A20 itself is very likely ending up in the correct state regardless
of which branch runs. The `else` branch's `keywait()` (`while (inp(KB_PORT)&2);`, no timeout) is
harmless on this project's own emulator (confirmed: `kbc_xt.c` doesn't implement port 0x64 at all,
so it always reads back `0x00`, and `0x00 & 2 == 0`, so `keywait()` returns instantly every time -
it is NOT capable of hanging on this hardware, faithfully or otherwise). The practical effect of
taking the "wrong" branch is likely just wasted redundant I/O (three port writes instead of one,
repeated far more than necessary), not a hang or corruption by itself.

**Why it might still matter for the actual blocker** (the Windows 95 GUI dialog not responding to
keyboard input - see the main memory file's full history): if this same misdetection pattern
extends to OTHER components beyond simple A20 toggling - i.e., if whatever decides "is this an
Inboard" for A20 purposes is the SAME detection logic (or lack thereof) used elsewhere for
keyboard-specific decisions - that would directly explain why patching `VKD.VXD` and
`KEYBOARD.DRV`'s *known* port-64h checks individually was not sufficient: there could be
additional, not-yet-found call sites making the same wrong assumption, each needing the same
"treat this as an Inboard" correction, exactly mirroring the A20 case just found. Also worth
checking directly: **how does `a20()` (or whatever inherited/ported version of it exists in this
project's real boot chain) actually implement `if (inboard)` - what's the real detection method?**
If it's checking something (a BIOS signature, an I/O port probe, an environment variable, a
config setting) that this project's build/environment doesn't correctly satisfy, fixing *that*
detection could be the actual, root-level fix - more fundamental than patching individual port-64h
call sites one at a time.

## Separately useful context from the thread

- Al, when asked later about the A20 gate specifically: *"Unfortunately, that doesn't sound
  familiar. I only remember the a20 thing because I got it wrong and had to correct it lol."* -
  confirms the A20 handling was genuinely tricky/error-prone even for the original expert author.
- Mike separately tracked down (or attempted to track down) several other people potentially
  relevant to this investigation, for reference/future follow-up if useful:
  - **"Bob"** - reportedly the original author of Intel's own Inboard software/drivers (identified
    via a Washington Post article, "There's still room for the lone programmer" - contacted via
    email, unclear if he ever replied based on this thread alone).
  - **Superfury** (UniPCemu emulator author) - independently attempted emulation of this exact
    hardware, bitbucket.org/superfury/unipcemu, itch.io page superfury.itch.io - already
    cross-referenced elsewhere in this project's own research (the "UniPCemu/SuperFury technical
    analysis" mentioned in earlier memory files, which already cross-validated the port 0x60
    hijack/0xDD/0xDF findings).
  - **Jeff Parsons** (jeffpar.com, "pcjs.org") - worked on Win 3/Win 95 era tooling, wrote up the
    Windows 95 startup routine and processor checks: https://www.pcjs.org/blog/2015/10/27/
  - **Raymond Chen** (Microsoft, devblogs.microsoft.com) - not yet contacted per this thread.
  - A Czech retrocomputing contact who reworked Windows 3.1 to run on an NEC V20 (i8088/i8086)
    CPU - potentially relevant prior art for "getting a Microsoft OS to behave on non-standard/
    non-AT-compliant hardware," not yet followed up per this thread.

## Status / next step

1. Identify segment `0325`'s actual owning file for certain (live segment dump + string search,
   same technique already proven this session for segments `0EAF`/`FF03`/`650B`).
2. Once identified, find how it decides `if (inboard)` (or whatever its equivalent check is) and
   confirm whether that detection is succeeding or failing in this project's OSR1 environment.
3. If it's failing, that's a new, more fundamental fix target than any individual port-64h call
   site - worth prioritizing over continuing to hunt for more individual patch sites one at a time.
