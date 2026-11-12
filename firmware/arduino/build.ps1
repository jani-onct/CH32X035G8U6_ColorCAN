param(
    [string]$ArduinoCli,
    [switch]$InstallCore
)

$ErrorActionPreference = 'Stop'
if (-not $ArduinoCli) {
    $cliCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
    $ArduinoCli = if ($cliCommand) { $cliCommand.Source } else {
        Join-Path $env:LOCALAPPDATA 'Programs/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe'
    }
}

function Invoke-Arduino([string[]]$Arguments) {
    & $ArduinoCli @Arguments
    if ($LASTEXITCODE -ne 0) { throw "arduino-cli failed ($LASTEXITCODE)" }
}


# WCH GCC on Windows needs a short path that its native tools can open.
$buildRoot = Join-Path ([IO.Path]::GetTempPath()) 'ColorCAN-Arduino'
$env:ARDUINO_DIRECTORIES_DATA = Join-Path $buildRoot 'data'
$env:ARDUINO_DIRECTORIES_DOWNLOADS = Join-Path $buildRoot 'downloads'
$env:ARDUINO_DIRECTORIES_USER = Join-Path $buildRoot 'user'
$env:ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS = 'https://raw.githubusercontent.com/openwch/board_manager_files/main/package_ch32v_index.json'
$version = '1.0.6'
$core = Join-Path $env:ARDUINO_DIRECTORIES_DATA "packages/WCH/hardware/ch32v/$version"
if ($InstallCore) {
    Invoke-Arduino @('core','update-index')
    Invoke-Arduino @('core','install',"WCH:ch32v@$version")
}
if (-not (Test-Path -LiteralPath (Join-Path $core 'platform.txt'))) {
    throw 'WCH Arduino Core is missing. Run build.ps1 -InstallCore first.'
}

$sketch = Join-Path $buildRoot 'sketch/ColorCAN_Arduino'
$output = Join-Path $buildRoot 'firmware'
$null = New-Item -ItemType Directory -Force -Path $sketch,$output
$shared = Join-Path $PSScriptRoot 'source'
$sources = @('colorcan_sensor_test.c','can_bench.h','can_pioc.c','can_pioc_program.S',
             'can_pioc_regs.h','can_wire.c','can_wire.h')
foreach ($source in $sources) {
    Copy-Item -LiteralPath (Join-Path $shared $source) -Destination $sketch
}
Copy-Item -LiteralPath (Join-Path $shared 'ColorCAN_Arduino.ino'),(Join-Path $shared 'colorcan_arduino.h') -Destination $sketch

# WCH's X035 linker script uses a fixed 20 KiB region. The PIOC maps the top 4 KiB.
$linker = Join-Path $buildRoot 'colorcan_pioc.ld'
$layout = Get-Content -Raw -LiteralPath (Join-Path $core 'system/CH32X035/SRC/Ld/Link.ld')
$ram = 'ORIGIN = 0x20000000, LENGTH = 20K'
if (-not $layout.Contains($ram)) { throw 'Unexpected CH32X035 linker RAM layout.' }
[IO.File]::WriteAllText($linker, $layout.Replace($ram, 'ORIGIN = 0x20000000, LENGTH = 16K'))
$recipe = Select-String -LiteralPath (Join-Path $core 'platform.txt') -Pattern '^recipe\.c\.combine\.pattern=' | Select-Object -ExpandProperty Line
$linkerTemplate = '{build.system.path}/{build.series}/SRC/Ld/{build.ldscript}'
if (-not $recipe.Contains($linkerTemplate)) { throw 'Unexpected WCH Arduino linker recipe.' }
$recipe = $recipe.TrimEnd().Replace($linkerTemplate, $linker.Replace('\','/'))

$fqbn = 'WCH:ch32v:CH32X035_EVT:pnum=CH32X035G8U,clock=48MHz_HSI,upload_method=ispMethod'
Invoke-Arduino @('compile','--fqbn',$fqbn,'--build-path',$output,
    '--build-property',$recipe,'--build-property','upload.maximum_data_size=16384',$sketch)

$map = Get-Content -Raw -LiteralPath (Join-Path $output 'ColorCAN_Arduino.ino.map')
if ($map -notmatch '(?m)^RAM\s+0x0*20000000\s+0x0*4000\s' -or
    $map -notmatch '(?m)^\s*0x0*20004000\s+.*_eusrstack' -or
    $map -notmatch '(?m)^\s*0x[0-9a-f]+\s+colorcan_pioc_program\s*$') {
    throw 'Linker map did not confirm the PIOC program and reserved RAM.'
}
$artifacts = Join-Path $PSScriptRoot 'build'
$null = New-Item -ItemType Directory -Force -Path $artifacts
$hex = Join-Path $artifacts 'colorcan_can_pioc_arduino.hex'
Copy-Item -LiteralPath (Join-Path $output 'ColorCAN_Arduino.ino.hex') -Destination $hex
Get-FileHash -LiteralPath $hex
