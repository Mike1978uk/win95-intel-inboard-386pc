# Assemble and link SWAPCNT.VXD with the Win95 DDK toolchain (MASM 6.11c and
# the VC++ 2.0-era LINK.EXE), as custom_vkd/build.ps1 does.
$DDK = "C:\Users\lycet\OneDrive\Desktop\XT_project\Windows95_ddk"
$SRC = Join-Path $PSScriptRoot 'src'
$OUT = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force $OUT | Out-Null
$env:INCLUDE = "$DDK\INC32;$DDK\INC16"
Write-Output ("commit {0}{1}" -f (git -C $PSScriptRoot log -1 --format=%h), $(if (git -C $PSScriptRoot status --porcelain -- .) { ' (tree dirty)' } else { '' }))
& "$DDK\MASM611C\ML.EXE" -coff -DBLD_COFF -DIS_32 -nologo -W2 -Zd -c -Cx -DMASM6 -DDEBLEVEL=0 "-Fo$OUT\SWAPCNT.obj" "$SRC\SWAPCNT.ASM"
if ($LASTEXITCODE -ne 0) { Write-Output 'FAILED: assembly'; exit 1 }
# LINK's .DEF parser fails ("ParseDefDescription") on absolute paths: link in place.
Copy-Item "$SRC\SWAPCNT.DEF" $OUT
Push-Location $OUT
& "$DDK\MSVC20\LINK.EXE" /VXD /NOD /OUT:SWAPCNT.VXD /MAP:SWAPCNT.map /DEF:SWAPCNT.DEF SWAPCNT.obj
Pop-Location
if (-not (Test-Path "$OUT\SWAPCNT.VXD")) { Write-Output 'FAILED: link'; exit 1 }
Get-Item "$OUT\SWAPCNT.VXD" | Select-Object Name, Length, LastWriteTime
