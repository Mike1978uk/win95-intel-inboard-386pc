# DINPROBE.EXE - DirectInput on the 5160 (#58)

Built, untested on the 5160. Opens the keyboard, the mouse and every attached joystick or wheel
through DirectInput's DirectX 5 interface (served by the DirectX 7.0a runtime on the card), reads
each every 20 ms while you use them, and reports what it saw and what each read cost.

```
DINPROBE [seconds, default 15]
```

Appends to `C:\DINPROBE.TXT`. Run it from an MS-DOS prompt in Windows 95 with DirectX installed.
While it runs: press a few keys, move the mouse and click, move each stick through its range and
press its buttons.

What it answers:
- **Keyboard through the custom VKD:** `key down` lines for the keys pressed. None, with the keyboard
  `open`, means DirectInput is not getting keys through `VKD_CUSTOM_INT09FIX`.
- **Mouse:** a non-zero total movement and the buttons pressed.
- **Each joystick or wheel:** its name, and `changes` above zero when it moved.
- **Cost:** `us/read` per device. A game-port stick is polled: DirectInput times each axis by
  reading port 201h in a loop, and each read empties the L1, so its figure is what a game pays per
  frame per stick (bus noise for #57).

Failures print the DirectInput error code (`CreateDevice`, `SetDataFormat`, `Acquire` and so on).

Build (Ubuntu `gcc-mingw-w64-i686`):

```
i686-w64-mingw32-gcc -s -Os -Wall -Wextra -march=i386 -mno-sse -ffreestanding -fno-builtin \
  -fno-tree-loop-distribute-patterns -fno-stack-protector -fno-asynchronous-unwind-tables \
  -nostdlib -o DINPROBE.EXE dinprobe.c \
  -Wl,--entry=_entry,--subsystem,console:4.0,--major-os-version,4,--minor-os-version,0,--major-image-version,1,--file-alignment,512,--section-alignment,4096,--disable-dynamicbase,--disable-nxcompat,--disable-reloc-section \
  -ldinput -ldxguid -lkernel32 -luser32 -lgcc
```

`-march=i386` matters: MinGW's default i686 code uses CMOV, which the 486BL3 does not have.
`DINPUT.DLL` is loaded at run time; the static libraries supply only the data formats and GUIDs.
