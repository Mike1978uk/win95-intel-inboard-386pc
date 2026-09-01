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
    [string]$SrcDir = "",
    # Register stride. 2 = this project's Lo-tech XT-CF rev 3 (A0 undecoded, MEASURED).
    # 1 = a classic XTIDE, which is also what 86Box's hdc_xtide.c emulates, so the
    # emulator load/probe loop must build with -Stride 1. Getting this wrong writes
    # the IDENTIFY opcode to DEVICE CONTROL and leaves SRST asserted on the drive.
    [int]$Stride = 2,
    # Which units the driver is willing to claim, one bit each:
    #   1 = master, 2 = slave, 3 = both.
    # The emulator loop builds with 2 so the first request-path run is aimed at
    # a scratch slave disk rather than the boot volume.
    [int]$ClaimMask = 3,
    # Bisect switch: claim the unit but write nothing into the DCB. Splits
    # "our DCB writes are wrong" from "the fault is downstream of inquiry"
    # in one run, which is cheaper than another guess (Technique 80).
    [switch]$NoDcb,
    [switch]$NoCalldown,
    [switch]$NoIo,
    # Prove an IOS request actually reached XTIDE_StartRequest. Writes a marker
    # sector to the claimed unit at LBA 16000 on the first request and every
    # 64th after; tools/pdr_reqmarker.py reads it back out of the image.
    # Diagnostic only - never in a shipped driver.
    [switch]$ReqMarker
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

    # 3. AEP_DEVICE_INQUIRY: the sample tests an UNINITIALISED eax, because its
    #    sniff_for_drive call is commented out, and answers AEP_FAILURE on the
    #    coin flip. IOS then logs "Init Failure" and drops the driver - which is
    #    what cost us three boots. Phase 1 claims no device at all, so answer
    #    AEP_NO_MORE_DEVICES deterministically. Its own comment block agrees
    #    that is the correct code for "nothing here".
    $inq = "`tor`teax, eax"
    if ($aer -notmatch [regex]::Escape($inq)) {
        Write-Output "FAILED: device-inquiry test not found in PORTAER.ASM"; exit 1
    }
    $aer = $aer.Replace($inq, "`tjmp`tPort_di_no_more_devices`t; phase 1 claims nothing`r`n`tor`teax, eax")
    Set-Content "$OutDir\PORTAER.ASM" -Value $aer -NoNewline
    # 4. DECLARE OURSELVES DYNAMICALLY LOADABLE.
    #    PORTINFO.INC ships PORTFeature EQU 00H, but the sample dispatches
    #    SYS_DYNAMIC_DEVICE_INIT - it IS dynamically loaded. Registering with no
    #    DRP_FC_DYNALOAD makes IOS_Register return something that is neither
    #    DRP_REMAIN_RESIDENT nor DRP_MINIMIZE, so PORT_Device_Init sets carry and
    #    the VxD reports "Device not initialized" - the Init Failure line in
    #    BOOTLOG.TXT. IOS never dispatches the AER, so no probe ever runs.
    #    zikolas/cfu1-win9x sets this flag for the same reason.
    $pi = Get-Content "$OutDir\PORTINFO.INC" -Raw
    $feat = "PORTFeature`tEQU`t00H"
    if ($pi -notmatch [regex]::Escape($feat)) {
        Write-Output "FAILED: PORTFeature equate not found in PORTINFO.INC"; exit 1
    }
    $pi = $pi.Replace($feat, "PORTFeature`tEQU`t10000H`t; DRP_FC_DYNALOAD - see build.ps1")
    Set-Content "$OutDir\PORTINFO.INC" -Value $pi -NoNewline
    Write-Output "Applied DRP_FC_DYNALOAD (PORTFeature 00H -> 10000H)."

    # 5. MEASURE DRP_reg_result. IOS_Register's return code is the only thing
    #    that explains an Init Failure at this layer, and nothing else escapes
    #    PORT_Device_Init - it runs before any AEP. Report it as a delay the
    #    boot log timestamps: (result+1)*2 half-second units, clamped.
    #      result 0 -> 1.0s (~30 ticks)   1 -> 2.0s (~60)   2 -> 3.0s (~90)
    #      3 -> 4.0s (~120)  4 -> 5.0s (~150) ...
    $pa = Get-Content "$OutDir\PORT.ASM" -Raw
    $reg = "        VxDCall`tIOS_Register`t`t;call registration"
    if ($pa -notmatch [regex]::Escape($reg)) {
        Write-Output "FAILED: IOS_Register call site not found in PORT.ASM"; exit 1
    }
    # 5a. ARRIVAL MARKER, before IOS_Register. A delay placed only after the
    #     call cannot distinguish "the control message never arrived" from
    #     "IOS_Register faulted or never returned" - both give zero delay.
    #     This one fires the moment PORT_Device_Init is entered.
    $entry = "BeginProc PORT_Device_Init"
    if ($pa -notmatch [regex]::Escape($entry)) {
        Write-Output "FAILED: PORT_Device_Init entry not found"; exit 1
    }
    $pa = $pa.Replace($entry, $entry + "`r`n" + @"
	mov	eax, 2			; ARRIVAL MARKER (build.ps1): ~1s
pdi_outer:				; proves this routine was entered
	mov	ecx, 725000
pdi_inner:
	dec	ecx
	jnz	pdi_inner
	dec	eax
	jnz	pdi_outer
"@)

    $diag = $reg + "`r`n" + @"
	add	esp,04			; DIAGNOSTIC (build.ps1): report the
	push	OFFSET32 Drv_Reg_Pkt	; registration result as a delay
	movzx	eax, Drv_Reg_Pkt.DRP_reg_result
	cmp	eax, 10
	jbe	prd_ok
	mov	eax, 10
prd_ok:
	inc	eax
	add	eax, eax
prd_outer:
	mov	ecx, 725000
prd_inner:
	dec	ecx
	jnz	prd_inner
	dec	eax
	jnz	prd_outer
"@
    $pa = $pa.Replace($reg, $diag)
    Set-Content "$OutDir\PORT.ASM" -Value $pa -NoNewline
    Write-Output "Applied reg_result diagnostic delay."

    # Phase 2c: claim a unit and service its requests. These two edits need
    # regexes over the sample's mixed tabs and spaces, so they live in Python;
    # it asserts every anchor and fails the build rather than no-op quietly.
    $psArgs = @("$PSScriptRoot/tools/patch_sample.py", $OutDir)
    if ($NoCalldown) { $psArgs += "--nocalldown" }
    if ($ReqMarker)  { $psArgs += "--reqmarker" }
    python $psArgs
    if ($LASTEXITCODE -ne 0) { Write-Output "FAILED: patch_sample.py"; exit 1 }

    $objs_extra = @("xtidetr")
    Write-Output "Applied phase1 hook (XTIDE_Probe wired into Port_initialize)."
} else {
    $objs_extra = @()
}

# Flags taken verbatim from the sample's own MAKEFILE, with MASTER_MAKE resolved by hand.
$aflags = @("-coff","-DBLD_COFF","-DDEBUG_TRACE=1","-DIS_32","-nologo","-W3","-Zd","-c","-Cx",
            "-DMASM6","-DINITLOG","-DDEBLEVEL=0","-DXT_STRIDE=$Stride","-DXT_CLAIM_MASK=$ClaimMask")
if ($NoDcb) { $aflags += "-DXT_NO_DCB=1"; Write-Output "BISECT: DCB fill disabled" }
if ($NoIo)  { $aflags += "-DXT_NO_IO=1";  Write-Output "BISECT: request handler is a stub" }
if ($ReqMarker) { $aflags += "-DXT_REQ_MARKER=1"; Write-Output "DIAGNOSTIC: on-disk request marker enabled" }
Write-Output "Register stride: $Stride   claim mask: $ClaimMask"
$objs = @("port","portaer","portreq","portisr") + $objs_extra

$failed = $false
foreach ($f in $objs) {
    & $ML $aflags "-Fo$f.obj" "$f.asm"
    if ($LASTEXITCODE -ne 0) { $failed = $true; Write-Output "FAILED: $f.asm" }
}
if ($failed) { Write-Output "Build aborted - assembly errors above."; exit 1 }

# /DEF is REQUIRED: it carries "VXD PORT DYNAMIC", which sets the LE module flag
# that marks the image dynamically loadable, and exports the DDB. Without it the
# link produces module flags 0x00028000; every .PDR IOS actually loads carries
# 0x00038000. See PORT.DEF.
Copy-Item "$PSScriptRoot\PORT.DEF" $OutDir -Force
& $LINK /VXD /NOD /ALIGN:4096 /DEF:PORT.DEF /OUT:PORT.pdr /MAP:PORT.map ($objs | ForEach-Object { "$_.obj" })

if (Test-Path PORT.pdr) {
    $h = (Get-FileHash PORT.pdr -Algorithm MD5).Hash.ToLower()
    Write-Output "SUCCESS: $OutDir\PORT.pdr  $((Get-Item PORT.pdr).Length) bytes  md5 $h"
} else {
    Write-Output "FAILED: link did not produce PORT.pdr"; exit 1
}
