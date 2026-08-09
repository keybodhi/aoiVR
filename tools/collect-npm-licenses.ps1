param(
  [string]$PackageRoot = "D:\workplace\VR-AGENT\Aoi-Release"
)
$ErrorActionPreference = "Stop"
$nm = Join-Path $PackageRoot "agent\node_modules"
$dest = Join-Path $PackageRoot "licenses\npm"
if (-not (Test-Path $nm)) { throw "no node_modules at $nm" }
if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
New-Item -ItemType Directory -Path $dest -Force | Out-Null

# 1. Mirror every license text file from node_modules (verbatim, official per-package).
$files = Get-ChildItem $nm -Recurse -File -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -match "^(LICENSE|LICENCE|COPYING|NOTICE|COPYRIGHT|license|licence)" }
$copied = 0
foreach ($f in $files) {
  $rel = $f.FullName.Substring($nm.Length + 1)
  $target = Join-Path $dest $rel
  $tdir = Split-Path -Parent $target
  New-Item -ItemType Directory -Path $tdir -Force | Out-Null
  Copy-Item $f.FullName $target -Force
  $copied++
}

# 2. Build INDEX.md: package -> version -> license -> license file present.
function Get-LicenseField($j) {
  if (-not $j.license) { return "" }
  if ($j.license -is [string]) { return $j.license }
  $types = @()
  $item = $j.license
  if ($item -is [array]) {
    foreach ($e in $item) { if ($e.type) { $types += [string]$e.type } }
  } elseif ($item.type) { $types += [string]$item.type }
  return ($types -join " OR ")
}

$rows = New-Object System.Collections.Generic.List[string]
$rows.Add("| Package | Version | License | License file |")
$rows.Add("|---|---|---|---|")
$pkgJsons = Get-ChildItem $nm -Recurse -Filter "package.json" -File -ErrorAction SilentlyContinue
$withFile = 0; $withoutFile = 0
foreach ($pj in $pkgJsons) {
  $dir = Split-Path -Parent $pj.FullName
  $rel = $dir.Substring($nm.Length + 1)
  try { $j = Get-Content $pj.FullName -Raw | ConvertFrom-Json } catch { continue }
  $name = if ($j.name) { [string]$j.name } else { $rel }
  $ver = if ($j.version) { [string]$j.version } else { "-" }
  $lic = Get-LicenseField $j
  $hasFile = @(Get-ChildItem $dir -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match "^(LICENSE|LICENCE|COPYING|NOTICE|COPYRIGHT|license|licence)" }).Count -gt 0
  if ($hasFile) { $withFile++ } else { $withoutFile++ }
  $lf = if ($hasFile) { "yes" } else { "no (see package.json)" }
  if (-not $lic) { $lic = "(none declared)" }
  $safe = $name.Replace("|", "\|")
  $rows.Add("| $safe | $ver | $lic | $lf |")
}
$rows.Add("")
$rows.Add("$($pkgJsons.Count) packages total; $withFile with an included license file, $withoutFile without.")
[System.IO.File]::WriteAllLines((Join-Path $dest "INDEX.md"), $rows, [System.Text.UTF8Encoding]::new($false))
Write-Host "copied $copied license files; indexed $($pkgJsons.Count) packages"
