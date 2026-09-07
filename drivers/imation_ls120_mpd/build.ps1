<#
    build.ps1 - assemble and link LS120MP.MPD, the parallel-port LS-120 miniport.

    PHASE 0 is a skeleton: it registers, takes its base from the Settings tab,
    reads the LPT status register once and reports no devices. It exists to
    test whether a polling miniport owning the parallel port disturbs the
    keyboard on this machine, before a transport is written. See the header of
    src/LS120MP.ASM and issue #22.

    Toolchain is XTIDEMP.MPD's, unchanged: MASM 6.11c from the Win95 DDK linked
    with the DDK's VC++ 2.0-era LINK.EXE against SCSIPORT.LIB.

    Technique 89: every build prints its commit and whether the tree was clean,
    and appends a row to build_ledger.tsv. A binary that cannot be traced back
    to a commit is not evidence.
#>

[CmdletBinding()]
param(
    # Pin the parallel-port base and ignore whatever the device node was
    # assigned. The INF is the real fix if a node is mis-assigned; this tests a
    # base without a reinstall (the lesson of 2026-09-05, technique 96).
    [int] $Base = 0,

    # 0 = the phase-0 skeleton that is staged for the keyboard test: no
    #     transport, reports no devices, ONE port instruction in the binary.
    # 2 = link the transport (LS120TR.ASM) and the SRB->ATAPI dispatch. The
    #     EPAT layer inside it is STUBBED and returns failure, so a phase-2
    #     build finds no drive either - it exists so the layers either side of
    #     the stubs can be written and reviewed before the licence question
    #     (README.md) is settled.
    [ValidateSet(0, 2)] [int] $Phase = 0,

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
if ($Base -ne 0)  { $defs += ("-DLS_FORCE_BASE=0{0:X}h" -f $Base) }
if ($Phase -eq 2) { $defs += '-DLS_PHASE2' }

# -coff is what makes ML emit objects the PE linker can use at all.
$aflags = @('-coff', '-DBLD_COFF', '-DIS_32', '-DMASM6', '-nologo', '-W2', '-Zd', '-c', '-Cx') + $defs

$objs = @()
$sources = if ($Phase -eq 2) { @('LS120TR', 'LS120MP') } else { @('LS120MP') }
foreach ($name in $sources) {
    $obj = Join-Path $out "$name.obj"
    Write-Host "ML  $name.ASM" -ForegroundColor Cyan
    & $ml @aflags "-Fo$obj" (Join-Path $src "$name.ASM")
    if ($LASTEXITCODE -ne 0) { throw "ML failed on $name.ASM (exit $LASTEXITCODE)" }
    $objs += $obj
}

$mpd = Join-Path $out 'LS120MP.MPD'
$map = Join-Path $out 'LS120MP.MAP'

# Argument-for-argument the DDK miniport sample's own link line (PC2X.LNK).
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

Write-Host "LINK LS120MP.MPD" -ForegroundColor Cyan
& $link @lflags @objs (Join-Path $lib 'scsiport.lib') (Join-Path $lib 'ntoskrnl.lib')
if ($LASTEXITCODE -ne 0) { throw "LINK failed (exit $LASTEXITCODE)" }

# ---- provenance -----------------------------------------------------------
$md5   = (Get-FileHash -Algorithm MD5 $mpd).Hash.ToLower().Substring(0, 8)
$bytes = (Get-Item $mpd).Length

# A PE's md5 is NOT a stable identity - LINK stamps build times into the COFF
# header and debug directory. The code hash neutralises those.
$py = $null
foreach ($cand in @('python', 'py', 'python3')) {
    $c = Get-Command $cand -ErrorAction SilentlyContinue
    if ($c -and $c.Source -notmatch 'WindowsApps') { $py = $c.Source; break }
}
$code = 'UNKNOWN'
if ($py) {
    $codeOut = ((& $py (Join-Path $here '..\..\tools\pe_codehash.py') $mpd) -join '').Trim()
    if ($codeOut -match '^code\s+(\S+)') { $code = $Matches[1] }
}
if ($code -eq 'UNKNOWN') { Write-Host '  (code hash unavailable - no usable python found)' -ForegroundColor Yellow }
$commit = (& git -C $here rev-parse --short HEAD 2>$null)
$dirty  = (& git -C $here status --porcelain 2>$null)
$tree   = if ($dirty) { 'DIRTY' } else { 'clean' }

Write-Host ''
Write-Host "LS120MP.MPD  code $code  md5 $md5  $bytes bytes  commit $commit  tree $tree" -ForegroundColor Green
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
