# CTCHIP34 — the CPU register set for the Inboard's Blue Lightning (issue #9)

Held here because `CPUSET.BAT` is useless without the tool it calls, and the
tool lived only in one Downloads folder. Restoring an older card image now
costs a file copy, not the CPU configuration.

| File | |
|---|---|
| `CTCHIP34.EXE` | the register editor, 61,762 bytes |
| `IBM486.CFG` | its register map for this CPU — `486DLC3 (oder 486SLC2) IBMs Blue Lightning`, dated 1993-12-07 |
| `CPUSET.BAT` | this project's applied set, captured off the live card 2026-09-19 |

## Provenance

`CPU_Register_Control_Apps.rar`, attached by **feipoa** to
*"Register settings for various CPUs"*,
<https://www.vogons.org/viewtopic.php?t=45756>. Not this project's work.

## Why this and not `REVTO486.SYS`

The Inboard's accelerator is an **IBM Blue Lightning BL3**, so Cyrix tools do
not apply — `CXID.EXE` returns errorlevel 255 on anything that is not a
Cx486DRx/SRx. The obvious alternative, `REVTO486.SYS`, is a `CONFIG.SYS`
resident that halts under Windows 95.

`CTCHIP34` is a real-mode EXE that runs from `AUTOEXEC.BAT` and exits, so it
sets the same silicon without a resident driver. That is what closed #9 on
2026-08-25, confirmed on the real 5160.

## The applied set

```
1000h:0 = 92    1000h:1 = 9C    1001h:0 = FF    1001h:1 = 03    1002h:3 = 03
```

Read off this machine from CTCHIP34's own screens and REVTO486's MSR dump on
2026-08-24 — measured, not guessed. **Windows 95 runs at 1:1 with the cache
off and nothing cacheable until this executes**, i.e. half clock speed.

## Two traps, both paid for

- **`CTCHIP34` looks for its `.CFG` in the CURRENT directory**, not beside the
  EXE. `CPUSET.BAT` does the `CD` for this reason; without it you get
  "file not found" with the `.CFG` sitting next to the executable.
- **Never put a bare angle bracket in a `.BAT`, even inside a `REM`.**
  `COMMAND.COM` honours redirection inside a remark: an arrow in a comment
  wrote a junk file named `ONLY` and printed "File not found" on every boot
  from 2026-08-24 until it was found on 2026-09-07. The register writes were
  always fine; only the comments were wrong.

Run it from real-mode DOS, not a Windows DOS box — ports `22h`/`23h` are
V86-trapped there.
