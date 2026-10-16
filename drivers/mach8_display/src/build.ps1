# Build M8DISP.DRV, the Mach8 display driver, with Open Watcom. No DDK needed:
# the DIB Engine import library is built from ddk\dibeng.lbc.
#
#   .\build.ps1 [-Watcom C:\Users\lycet\tools\openwatcom] [-Out build]

param(
  [string]$Watcom = 'C:\Users\lycet\tools\openwatcom',
  [string]$Out = (Join-Path $PSScriptRoot 'build'),
  [switch]$Trace      # bring-up step markers to SCRATCH_PAD_1 (M8_STEP)
)
$ErrorActionPreference = 'Stop'
$src = $PSScriptRoot
$env:WATCOM = $Watcom
$env:PATH = "$Watcom\binnt64;$Watcom\binnt;$env:PATH"
$env:INCLUDE = "$Watcom\h;$Watcom\h\win"
New-Item -ItemType Directory -Force $Out | Out-Null

function Run($exe, [string[]]$a) {
  & $exe @a
  if ($LASTEXITCODE -ne 0) { throw "$exe failed ($LASTEXITCODE): $($a -join ' ')" }
}

$inc = @("-i=$src", "-i=$src\ddk", "-i=$Watcom\h", "-i=$Watcom\h\win")

# Resources: each .c is data linked raw into a .bin.
foreach ($r in 'config', 'colortab', 'fonts', 'fonts120') {
  Run wcc (@('-q', '-zu', "-fo=$Out\$r.obj") + $inc + "$src\res\$r.c")
  Run wlink @('op', 'quiet', 'disable', '1014,1023', 'name', "$Out\$r.bin", 'sys', 'dos',
              'output', 'raw', 'file', "$Out\$r.obj")
}
Push-Location $Out
try {
  Run wrc @('-q', '-r', '-ad', '-bt=windows', "-fo=$Out\m8disp.res", "-i=$Out",
            "-i=$Watcom\h\win", "$src\res\m8disp.rc")
} finally { Pop-Location }

Run wlib @('-b', '-q', '-n', '-fo', '-ii', "@$src\ddk\dibeng.lbc", "$Out\dibeng.lib")

# 486 code, Windows prologues (-zW). Compact model: GDI calls us on its own stack
# (-zu, SS is not DGROUP), so a pointer to a local is far. W303 (unused parameter) is off: DDI entry
# points have fixed signatures.
$cflags = @('-q', '-wx', '-we', '-s', '-zu', '-zls', '-zW', '-4', '-mc', '-wcd=303')
if ($Trace) { $cflags += '-dM8_TRACE' }
foreach ($c in 'enable', 'screen', 'm8hw') {
  Run wcc ($cflags + $inc + @("-fo=$Out\$c.obj", "$src\$c.c"))
}
Run wasm @('-q', "-fo=$Out\thunks.obj", "$src\thunks.asm")
Run wasm @('-q', "-fo=$Out\switch.obj", "$src\switch.asm")

$exports = @(
  'BitBlt.1', 'ColorInfo.2', 'Control.3', 'Disable.4', 'Enable.5', 'EnumDFonts.6',
  'EnumObj.7', 'Output.8', 'Pixel.9', 'RealizeObject.10', 'StrBlt.11', 'ScanLR.12',
  'DeviceMode.13', 'ExtTextOut.14', 'GetCharWidth.15', 'DeviceBitmap.16', 'FastBorder.17',
  'SetAttribute.18', 'DibBlt.19', 'CreateDIBitmap.20', 'DibToDevice.21', 'SetPalette.22',
  'GetPalette.23', 'SetPaletteTranslate.24', 'GetPaletteTranslate.25', 'UpdateColors.26',
  'StretchBlt.27', 'StretchDIBits.28', 'SelectBitmap.29', 'BitmapBits.30', 'ReEnable.31',
  'Inquire.101', 'SetCursor.102', 'MoveCursor.103', 'CheckCursor.104',
  'SaveScreenBitmap.92', 'GetDriverResourceID.450', 'UserRepaintDisable.500', 'ValidateMode.700')
$lnk = @(
  'system windows dll initglobal',
  "name $Out\M8DISP.DRV",
  "option map=$Out\m8disp.map",
  'option quiet',
  'option start=DriverInit_',
  'option modname=DISPLAY',
  "option description 'DISPLAY : 100, 96, 96 : ATI Graphics Ultra (Mach8) display driver.'",
  'option oneautodata',
  'option alignment=16',
  'segment class ''DATA'' preload fixed',
  "segment '_TEXT' preload fixed shared",
  'disable 2055'
) + ('enable', 'screen', 'm8hw', 'thunks', 'switch' | ForEach-Object { "file $Out\$_.obj" }) + @(
  "library $Out\dibeng.lib",
  "library $Watcom\lib286\win\clibc.lib",
  'import GlobalSmartPageLock KERNEL.230'
) + ($exports | ForEach-Object { "export $_" })
Set-Content -Path "$Out\m8disp.lnk" -Value $lnk -Encoding Ascii
Run wlink @("@$Out\m8disp.lnk")

Run wrc @('-q', "$Out\m8disp.res", "$Out\M8DISP.DRV")

# Windows 95 refuses a display driver that expects an older Windows: set the
# NE header's expected version (offset 3Eh) to 4.00.
$drv = "$Out\M8DISP.DRV"
$b = [IO.File]::ReadAllBytes($drv)
$ne = [BitConverter]::ToInt32($b, 0x3C)
if ($b[$ne] -ne 0x4E -or $b[$ne + 1] -ne 0x45) { throw 'no NE header' }
$b[$ne + 0x3E] = 0x00; $b[$ne + 0x3F] = 0x04
[IO.File]::WriteAllBytes($drv, $b)

# wlink links a fixup to a segment the file does not have without complaint;
# Windows then refuses the driver (BOOTLOG: LoadFail, failure code 0).
$bad = & wdump -a $drv 2>$null | Select-String 'invalid segment'
if ($bad) { throw "fixup to a missing segment:`n$($bad -join "`n")" }
Get-Item $drv | Format-Table Name, Length, LastWriteTime
