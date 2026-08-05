# Build script for the custom Inboard-aware VKD.VXD, using the real Windows 95 DDK toolchain
# (Microsoft MASM 6.11c + the VC++ 2.0-era LINK.EXE, both confirmed to run natively on modern
# Windows via WOW64 - no DOSBox/emulation needed). See docs/custom_vkd_notes.md for background.
#
# DDK location (not in this repo): C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk\
# Source (in this repo, adapted from KEYB\SAMPLES\VKD\): custom_vkd\src\

$DDK = "C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk"
$ML = "$DDK\MASM611C\ML.EXE"
$LINK = "$DDK\MSVC20\LINK.EXE"
$SRC = "C:\Users\lycet\RiderProjects\86Box-Inboard\custom_vkd\src"

$env:INCLUDE = "$DDK\INC32;$DDK\INC16"
Set-Location $SRC

$aflags = "-coff","-DBLD_COFF","-DMINIVDD","-DIS_32","-nologo","-W2","-Zd","-c","-Cx","-DMASM6","-DDEBLEVEL=0","-DSupport_Reboot"
$files = @("vkd","vkdhk","vkdio","vkdmsg","vkdphys","vad")

$failed = $false
foreach ($f in $files) {
    Write-Output "=== Assembling $f.asm ==="
    & $ML $aflags "-Fo$f.obj" "$f.asm"
    if ($LASTEXITCODE -ne 0) { $failed = $true; Write-Output "FAILED: $f.asm" }
}

if ($failed) {
    Write-Output "Build aborted - assembly errors above."
    exit 1
}

Write-Output "=== Linking VKD.vxd ==="
& $LINK /VXD /NOD /OUT:VKD.vxd /MAP:VKD.map /DEF:VKD.DEF vkd.obj vkdhk.obj vkdio.obj vkdmsg.obj vkdphys.obj vad.obj

if (Test-Path VKD.vxd) {
    Write-Output "SUCCESS: $SRC\VKD.vxd built."
} else {
    Write-Output "FAILED: link did not produce VKD.vxd"
    exit 1
}
