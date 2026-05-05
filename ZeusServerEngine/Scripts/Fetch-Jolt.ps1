# Obtém o código-fonte do JoltPhysics (tag alinhada com CMakeLists.txt do ZeusServerEngine).
# Executar a partir da raiz do repositório Server ou com o caminho correcto.
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Dest = Join-Path $Root "ThirdParty\JoltPhysics"
$Tag = "v5.5.0"
if (Test-Path (Join-Path $Dest "Jolt\Jolt.cmake")) {
    Write-Host "JoltPhysics já presente em $Dest"
    exit 0
}
Write-Host "A clonar JoltPhysics $Tag para $Dest ..."
New-Item -ItemType Directory -Force -Path (Split-Path $Dest) | Out-Null
git clone --depth 1 --branch $Tag https://github.com/jrouwe/JoltPhysics.git $Dest
Write-Host "Concluído."
