# Build Setup for 86Box-Inboard

## Current Status (2026-07-30)

✅ **Working Binary Available**: `86box_full/build/phase1_working/x64-Release/86Box.exe` (74 MB)
- Built: 2026-07-26
- Compiler: MinGW-w64 GCC (via MSYS2)
- Contains all timing fixes (validated, tested)
- Use this for Phase 1 testing without rebuilding

⚠️ **Build Environment Issue**: Current MSVC setup is broken
- Missing C runtime headers (stddef.h, stdint.h, string.h, etc.)
- Rebuild from the MinGW setup documented below

## Building from Source (MinGW/MSYS2)

### Why MinGW Instead of MSVC?

86Box's build genuinely depends on GNU-libc-specific symbols:
- `ftello64`, `usleep`, `ssize_t` (not available in MSVC)
- Only MinGW-w64 GCC provides these

### Setup: MSYS2 with MinGW-w64

**One-time environment setup:**

```bash
# 1. Install MSYS2 (if not already installed)
#    Download from https://www.msys2.org/
#    Install to C:\msys64 (or similar)

# 2. From MSYS2 shell (MSYS2 MinGW 64-bit):
pacman -S mingw-w64-x86_64-cmake
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-ninja
pacman -S mingw-w64-x86_64-pkg-config

# 3. Set MSYS2 in your environment:
export PATH="/mingw64/bin:$PATH"
export CC=gcc
export CXX=g++
```

**Configure and build:**

```bash
cd C:\Users\lycet\RiderProjects\86Box-Inboard\86box_full

# Clean previous MSVC build artifacts
rm -rf build/phase1

# Create build directory
mkdir -p build/phase1_mingw
cd build/phase1_mingw

# Configure with Ninja (faster parallel builds)
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DQT=OFF \
  -DSDL2=ON \
  -DCMAKE_C_COMPILER=gcc \
  -DCMAKE_CXX_COMPILER=g++

# Build (4 parallel jobs)
cmake --build . --config Release -j4

# Result: ./86Box.exe
```

**If vcpkg fails to auto-bootstrap**, manually set up:

```bash
# In 86box_full directory:
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
cmake .. -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake
```

## Alternative: Pre-Built Binary

For Phase 1 testing, use the already-built binary:

```bash
./86box_full/build/phase1_working/x64-Release/86Box.exe
```

This is the "known good" version. Test with:
- `vm_1986mach8` for Mach8 video testing
- `vm_win311` for Windows 3.11 testing
- Verify all timing fixes working (no 3-beep errors, successful POST sequences)

## When to Rebuild

Rebuild only when:
1. You've modified source code in `86box_full/src/device/inboard386.c` or related timing files
2. Adding new features or fixes beyond Phase 1
3. Validating code changes before committing

**Do NOT rebuild** if just testing or running existing configurations.

## Build Artifacts to Archive

After a successful MinGW build, preserve:
- `build/phase1_mingw/86Box.exe` (the new binary)
- Update `build/phase1_working/x64-Release/86Box.exe` with it
- Commit to git (or update the archive location)

## Known Build Issues & Workarounds

### "Cannot find Freetype" (MSVC)
- Status: **FIXED for MinGW** - dependencies auto-resolve
- Workaround if MSVC: Made optional in CMakeLists.txt (src/CMakeLists.txt line ~180)

### "Missing C runtime headers" (MSVC)
- Status: **Fundamental MSVC limitation** for this codebase
- Fix: Use MinGW-w64 GCC instead (see setup above)

### Build parallelism hangs
- Symptom: cmake --build hangs or crashes on -j8
- Fix: Reduce to `-j4` or `-j2`

## CI/CD Integration (Future)

When setting up automated builds:
1. Use GitHub Actions with MinGW-w64 GCC container
2. Cache vcpkg artifacts between builds
3. Run tests on both reference binary and newly-built binary for validation
4. Archive successful builds with commit hash

## References

- Full investigation: `INBOARD_86BOX_PORT_PLAN.md` (section "2026-07-24")
- Timing fixes documentation: `TIMING_FIXES_APPLIED.md`
- Build configuration: `86box_full/CMakeLists.txt`, `86box_full/CMakePresets.json`

## Testing the Build

After building `86Box.exe`:

```bash
# Quick smoke test: Check it runs
./build/phase1_mingw/86Box.exe --help

# Full test: Boot DOS in test VM
./build/phase1_mingw/86Box.exe -P ../../../XT_5160_rework_claude/vm/

# Expected: DOS boots, no 3-beep errors, "640 KB OK" message
```
