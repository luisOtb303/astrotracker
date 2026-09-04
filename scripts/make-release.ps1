# Genera los artefactos de release de AstroTracker: ZIP portable e instalador
# NSIS. Requiere que el proyecto ya esté configurado en build/ y que la versión
# y el changelog estén actualizados (ver checklist en AGENTS.md).
#
# Uso:
#   powershell -File scripts\make-release.ps1             # build + tests + empaquetado + tag
#   powershell -File scripts\make-release.ps1 -SkipTests  # sin ctest
param(
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "build"

if (-not (Test-Path (Join-Path $build "CMakeCache.txt"))) {
    Write-Error "No existe build/CMakeCache.txt. Configura primero el proyecto (cmake -S . -B build ...)."
}

# NSIS (para el instalador .exe); si no está, cpack generará solo el ZIP.
$nsisBin = "C:\Program Files (x86)\NSIS\Bin"
if (Test-Path $nsisBin) {
    $env:Path = "$nsisBin;$env:Path"
} else {
    Write-Warning "NSIS no encontrado en '$nsisBin': se generará solo el ZIP portable."
}

Write-Host "== Compilando (Release)..." -ForegroundColor Cyan
cmake --build $build --config Release
if ($LASTEXITCODE -ne 0) { Write-Error "La compilación falló." }

if (-not $SkipTests) {
    Write-Host "== Ejecutando tests (CTest)..." -ForegroundColor Cyan
    ctest --test-dir $build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { Write-Error "Los tests fallaron." }
} else {
    Write-Host "== Tests omitidos (-SkipTests)" -ForegroundColor Yellow
}

Write-Host "== Empaquetando (ZIP + instalador NSIS)..." -ForegroundColor Cyan
Push-Location $build
try {
    cpack -C Release
    if ($LASTEXITCODE -ne 0) { Write-Error "cpack falló." }
} finally {
    Pop-Location
}

Get-ChildItem (Join-Path $build "AstroTracker-*-win64.*") |
    Where-Object { $_.Extension -in ".zip", ".exe" } |
    ForEach-Object {
        Write-Host ("Artefacto: {0}  ({1:N1} MB)" -f $_.FullName, ($_.Length / 1MB)) -ForegroundColor Green
    }

# Tag anotado
$ver = (Select-String -Path (Join-Path $root "CMakeLists.txt") -Pattern 'project\(AstroTracker VERSION ([^ )]+)').Matches[0].Groups[1].Value
$tag = "v$ver"
$existingTag = git tag -l $tag 2>$null
if ($existingTag) {
    Write-Warning "El tag '$tag' ya existe; se omite."
} else {
    Write-Host "== Creando tag anotado '$tag'..." -ForegroundColor Cyan
    git tag -a $tag -m "Release $tag"
    if ($LASTEXITCODE -ne 0) { Write-Error "No se pudo crear el tag '$tag'." }
    Write-Host "Tag creado. Empuja con: git push origin $tag" -ForegroundColor Green
}
