# Build the Windows 95 IOS port driver for the 8-bit XT-IDE card (issue #21).
#
# Phase 0 builds Microsoft's DDK sample UNCHANGED, to prove the toolchain and that IOS will
# load what it produces. Later phases replace the sample's transport with XT-IDE taskfile I/O.
#
# Toolchain: genuine MASM 6.11c + the VC++ 2.0-era LINK.EXE (modern link.exe dropped /VXD).
# Both run natively on Windows 11 via WOW64 - no DOSBox. Same pair custom_vkd/build.ps1 uses.
#
# The DDK is NOT in this repo (not redistributable). Sources come from:
#   $DDK\BLOCK\SAMPLES\PORT\SAMPLE\   PORT.ASM PORTAER.ASM PORTREQ.ASM PORTISR.ASM
#   $DDK\INC32  $DDK\INC16  $DDK\BLOCK\INC

param(
    [string]$DDK = "C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk",
    [string]$OutDir = "$PSScriptRoot\build",
    # Phase 0 = stock sample. Point this at .\src once we start modifying it.
    [string]$SrcDir = ""
)

$ML   = "$DDK\MASM611C\ML.EXE"
$LINK = "$DDK\MSVC20\LINK.EXE"
if ($SrcDir -eq "") { $SrcDir = "$DDK\BLOCK\SAMPLES\PORT\SAMPLE" }

foreach ($t in @($ML, $LINK, $SrcDir)) {
    if (-not (Test-Path $t)) { Write-Output "MISSING: $t"; exit 1 }
}

New-Item -ItemType Directory -Force $OutDir | Out-Null
Copy-Item "$SrcDir\*.ASM" $OutDir
Copy-Item "$SrcDir\*.INC" $OutDir
Set-Location $OutDir

# INC16 carries CMACROS.INC/PIF.INC only; BLOCK\INC carries the storage-stack headers
# (DDB, DCB, SCSIPORT, IODEBUG...). INC32 has the rest. All three are required.
$env:INCLUDE = "$DDK\INC32;$DDK\INC16;$DDK\BLOCK\INC"

# Flags taken verbatim from the sample's own MAKEFILE, with MASTER_MAKE resolved by hand.
$aflags = @("-coff","-DBLD_COFF","-DDEBUG_TRACE=1","-DIS_32","-nologo","-W3","-Zd","-c","-Cx",
            "-DMASM6","-DINITLOG","-DDEBLEVEL=0")
$objs = @("port","portaer","portreq","portisr")

$failed = $false
foreach ($f in $objs) {
    & $ML $aflags "-Fo$f.obj" "$f.asm"
    if ($LASTEXITCODE -ne 0) { $failed = $true; Write-Output "FAILED: $f.asm" }
}
if ($failed) { Write-Output "Build aborted - assembly errors above."; exit 1 }

# No /DEF: the DDB is declared in PORT.ASM and /VXD sets the segment attributes.
& $LINK /VXD /NOD /OUT:PORT.pdr /MAP:PORT.map ($objs | ForEach-Object { "$_.obj" })

if (Test-Path PORT.pdr) {
    $h = (Get-FileHash PORT.pdr -Algorithm MD5).Hash.ToLower()
    Write-Output "SUCCESS: $OutDir\PORT.pdr  $((Get-Item PORT.pdr).Length) bytes  md5 $h"
} else {
    Write-Output "FAILED: link did not produce PORT.pdr"; exit 1
}
