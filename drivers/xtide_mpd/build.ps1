<#
    build.ps1 - assemble and link XTIDEMP.MPD, the XT-CF SCSI miniport.

    A .MPD is a PE file, not an LE VxD, so none of the .DEF / LE-page /
    dynamic-load-flag problems that cost sessions on the .PDR apply here.
    Toolchain is the one already in use: MASM 6.11c from the Win95 DDK, linked
    with the DDK's own VC++ 2.0-era LINK.EXE against SCSIPORT.LIB.

    Technique 89: every build prints its commit and whether the tree was clean,
    and appends a row to build_ledger.tsv. A binary that cannot be traced back
    to a commit is not evidence, and this project has already lost a day to
    exactly that.
#>

[CmdletBinding()]
param(
    # Pin the register stride instead of autodetecting. 2 = Lo-tech XT-CF.
    [ValidateSet(0, 1, 2)] [int] $Stride = 0,

    # Set PORT_CONFIGURATION_INFORMATION.RealModeInitialized. The open question
    # in docs/scsi_miniport_costing.md - flip this first if the miniport loads
    # but the boot volume is not taken over from the real-mode mapper.
    [switch] $RealModeInit,

    # Allow the phase-2b write self-test to look for a slave. OFF by default:
    # on a single-disk machine the unit that answers is the boot volume.
    [switch] $WriteTest,

    # Pin the I/O base and ignore whatever the device node was assigned.
    # CONFIGMG allocates from the INF's LogConfig before the driver ever runs,
    # so a mis-assigned node cannot be corrected from inside the driver at
    # install time - on 2026-09-05 it handed this node 0320-033F. The INF is
    # the real fix; this tests a base without a reinstall.
    [int] $Base = 0,

    [string] $DdkRoot = 'C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk'
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$src  = Join-Path $here 'src'
$out  = Join-Path $here 'build'
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

$ml   = Join-Path $DdkRoot 'MASM611C\ML.EXE'
$link = Join-Path $DdkRoot 'MSVC20\LINK.EXE'
$lib  = Join-Path $DdkRoot 'BLOCK\LIB'
$inc  = Join-Path $DdkRoot 'BLOCK\INC'

foreach ($p in @($ml, $link, $lib, $inc)) {
    if (-not (Test-Path $p)) { throw "missing from the DDK: $p" }
}

# ML resolves INCLUDE from the environment, not from a switch.
$env:INCLUDE = "$inc;$(Join-Path $DdkRoot 'INC32')"

$defs = @()
if ($Stride -ne 0)  { $defs += "-DXT_STRIDE=$Stride" }
if ($RealModeInit)  { $defs += '-DXT_REAL_MODE_INIT' }
if (-not $WriteTest) { $defs += '-DXT_NO_WRITETEST' }
if ($Base -ne 0)    { $defs += ("-DXT_FORCE_BASE=0{0:X}h" -f $Base) }

# -coff is what makes ML emit objects the PE linker can use at all.
$aflags = @('-coff', '-DBLD_COFF', '-DIS_32', '-DMASM6', '-nologo', '-W2', '-Zd', '-c', '-Cx') + $defs

$objs = @()
foreach ($name in @('XTIDETR', 'XTIDEMP')) {
    $obj = Join-Path $out "$name.obj"
    Write-Host "ML  $name.ASM" -ForegroundColor Cyan
    & $ml @aflags "-Fo$obj" (Join-Path $src "$name.ASM")
    if ($LASTEXITCODE -ne 0) { throw "ML failed on $name.ASM (exit $LASTEXITCODE)" }
    $objs += $obj
}

$mpd = Join-Path $out 'XTIDEMP.MPD'
$map = Join-Path $out 'XTIDEMP.MAP'

# Argument-for-argument the DDK miniport sample's own link line (PC2X.LNK),
# with our two objects in place of its one. -entry names the decorated stdcall
# symbol; see the convention note at the top of XTIDEMP.ASM.
$lflags = @(
    '-machine:i386'
    '-align:0x200'
    '-subsystem:native'
    '-debug:partial'
    '-base:0x10000'
    "-entry:DriverEntry@8"
    "-out:$mpd"
    "-map:$map"
)

Write-Host "LINK XTIDEMP.MPD" -ForegroundColor Cyan
& $link @lflags @objs (Join-Path $lib 'scsiport.lib') (Join-Path $lib 'ntoskrnl.lib')
if ($LASTEXITCODE -ne 0) { throw "LINK failed (exit $LASTEXITCODE)" }

# ---- provenance -----------------------------------------------------------
$md5   = (Get-FileHash -Algorithm MD5 $mpd).Hash.ToLower().Substring(0, 8)
$bytes = (Get-Item $mpd).Length

# A PE's md5 is NOT a stable identity: LINK stamps the build time into the COFF
# header, the debug directory and the trailing debug data, so two builds of one
# source differ (measured 2026-09-05: nine bytes, all timestamps). The code hash
# neutralises exactly those and is what identifies the binary. Record both -
# the file md5 is what you can compute on a card without tooling.
# `python3` on this box resolves to the Windows Store stub, which silently does
# nothing; `python` is the real interpreter. Resolve it rather than assume.
$py = $null
foreach ($cand in @('python', 'py', 'python3')) {
    $c = Get-Command $cand -ErrorAction SilentlyContinue
    if ($c -and $c.Source -notmatch 'WindowsApps') { $py = $c.Source; break }
}
$code = 'UNKNOWN'
if ($py) {
    $codeOut = ((& $py (Join-Path $here '..\..	ools\pe_codehash.py') $mpd) -join '').Trim()
    if ($codeOut -match '^code\s+(\S+)') { $code = $Matches[1] }
}
if ($code -eq 'UNKNOWN') { Write-Host '  (code hash unavailable - no usable python found)' -ForegroundColor Yellow }
$commit = (& git -C $here rev-parse --short HEAD 2>$null)
$dirty  = (& git -C $here status --porcelain 2>$null)
$tree   = if ($dirty) { 'DIRTY' } else { 'clean' }

Write-Host ''
Write-Host "XTIDEMP.MPD  code $code  md5 $md5  $bytes bytes  commit $commit  tree $tree" -ForegroundColor Green
if ($tree -eq 'DIRTY') {
    Write-Host ''
    Write-Host '  *** TREE IS DIRTY - this binary cannot be rebuilt from a commit. ***' -ForegroundColor Yellow
    Write-Host '  Commit before deploying anywhere a conclusion will be drawn from.' -ForegroundColor Yellow
    ($dirty -split "`n" | Where-Object { $_ }) | ForEach-Object { Write-Host "    $_" -ForegroundColor Yellow }
    Write-Host ''
}

$ledger = Join-Path $here 'build_ledger.tsv'
if (-not (Test-Path $ledger)) {
    "utc`tcode`tmd5`tbytes`tcommit`ttree`tflags" | Out-File -FilePath $ledger -Encoding ascii
}
$flags = if ($defs) { $defs -join ' ' } else { '(none)' }
"{0}`t{1}`t{2}`t{3}`t{4}`t{5}`t{6}" -f (Get-Date -Format 'yyyy-MM-ddTHH:mm:ssZ'), $code, $md5, $bytes, $commit, $tree, $flags |
    Out-File -FilePath $ledger -Encoding ascii -Append
