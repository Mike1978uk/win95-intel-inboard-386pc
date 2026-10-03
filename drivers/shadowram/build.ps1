# Assemble and link SHADRAM.VXD with the Win95 DDK toolchain (MASM 6.11c and the
# VC++ 2.0-era LINK.EXE), as drivers/swapcomp/build.ps1 does.
$DDK = "C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk"
$SRC = Join-Path $PSScriptRoot 'src'
$OUT = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $OUT | Out-Null
$env:INCLUDE = "$DDK\INC32;$DDK\INC16"
Write-Output ("commit {0}{1}" -f (git -C $PSScriptRoot log -1 --format=%h), $(if (git -C $PSScriptRoot status --porcelain -- .) { ' (tree dirty)' } else { '' }))

function Build($name, $asm, $def, $extra) {
    & "$DDK\MASM611C\ML.EXE" -coff -DBLD_COFF -DIS_32 -nologo -W2 -Zd -c -Cx -DMASM6 -DDEBLEVEL=0 @extra "-Fo$OUT\$name.obj" "$SRC\$asm"
    if ($LASTEXITCODE -ne 0) { Write-Output "FAILED: assembling $name"; exit 1 }
    # LINK's .DEF parser fails ("ParseDefDescription") on absolute paths: link in place.
    # The DEF keeps the device's own name; only the output file is renamed.
    Copy-Item "$SRC\$def" "$OUT\$name.DEF"
    Push-Location $OUT
    & "$DDK\MSVC20\LINK.EXE" /VXD /NOD "/OUT:$name.VXD" "/MAP:$name.map" "/DEF:$name.DEF" "$name.obj" | Where-Object { $_ -notmatch 'LNK4078' }
    Pop-Location
    if (-not (Test-Path "$OUT\$name.VXD")) { Write-Output "FAILED: linking $name"; exit 1 }
    Get-Item "$OUT\$name.VXD" | Select-Object Name, Length, LastWriteTime
}

Build 'SHADRAM' 'SHADRAM.ASM' 'SHADRAM.DEF' @()
Build 'SHADRAMD' 'SHADRAM.ASM' 'SHADRAM.DEF' @('-DSR_DIAG')
