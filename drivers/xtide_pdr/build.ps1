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

# Phase 0 safety change - see phase0-no-irq.patch for why. Registering the devnode's
# DDB_irq_number (zero, on a devnode declaring no IRQ) with VPICD would virtualise the
# system timer. Asserts it actually changed something rather than silently no-opping.
if ($SrcDir -eq "$DDK\BLOCK\SAMPLES\PORT\SAMPLE") {
    $aer = Get-Content "$OutDir\PORTAER.ASM" -Raw
    $needle = "`tcall`tPort_set_irq_handler"
    if ($aer -notmatch [regex]::Escape($needle)) {
        Write-Output "FAILED: IRQ call site not found in PORTAER.ASM - inspect before building"; exit 1
    }
    $aer = $aer.Replace($needle,
        "; PHASE 0, this project: NOT the DDK original. See phase0-no-irq.patch.`r`n;`tcall`tPort_set_irq_handler")
    Set-Content "$OutDir\PORTAER.ASM" -Value $aer -NoNewline
    Write-Output "Applied phase0-no-irq (IRQ registration removed)."
}

# Phase 1: hook our transport into the sample's AEP_INITIALIZE. Every edit asserts
# it matched, so a moved call site fails the build instead of silently no-opping.
if (Test-Path "$PSScriptRoot\src\XTIDETR.ASM") {
    Copy-Item "$PSScriptRoot\src\*.ASM" $OutDir
    $aer = Get-Content "$OutDir\PORTAER.ASM" -Raw

    # 1. declare it, alongside the sample's own cross-module externs
    $anchor = "`textrn`tPort_request:near"
    if ($aer -notmatch [regex]::Escape($anchor)) {
        Write-Output "FAILED: extern anchor not found in PORTAER.ASM"; exit 1
    }
    $aer = $aer.Replace($anchor, "`textrn`tXTIDE_Probe:near`t; XT-IDE transport (phase 1)`r`n$anchor")

    # 2. call it where the sample calls its own (commented out) adapter probe.
    #    The existing "jc Port_i_failure" two lines below then does the right thing.
    $probe = ";`tcall`tPort_initialize_adapter"
    if ($aer -notmatch [regex]::Escape($probe)) {
        Write-Output "FAILED: adapter-probe call site not found in PORTAER.ASM"; exit 1
    }
    $aer = $aer.Replace($probe, "`tcall`tXTIDE_Probe`t`t; IDENTIFY + transport autodetect")

    Set-Content "$OutDir\PORTAER.ASM" -Value $aer -NoNewline
    $objs_extra = @("xtidetr")
    Write-Output "Applied phase1 hook (XTIDE_Probe wired into Port_initialize)."
} else {
    $objs_extra = @()
}

# Flags taken verbatim from the sample's own MAKEFILE, with MASTER_MAKE resolved by hand.
$aflags = @("-coff","-DBLD_COFF","-DDEBUG_TRACE=1","-DIS_32","-nologo","-W3","-Zd","-c","-Cx",
            "-DMASM6","-DINITLOG","-DDEBLEVEL=0")
$objs = @("port","portaer","portreq","portisr") + $objs_extra

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
