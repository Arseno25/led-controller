param(
    [ValidateSet("esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6")]
    [string]$Target = "esp32",

    [Parameter(Mandatory = $true)]
    [string]$Port
)

$ErrorActionPreference = "Stop"

if ($env:IDF_PATH) {
    $IdfPy = Join-Path $env:IDF_PATH "tools\idf.py"
    $Python = if ($env:IDF_PYTHON_ENV_PATH) {
        Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts\python.exe"
    } else {
        "python"
    }
    & $Python $IdfPy -B "build-$Target" -D "SDKCONFIG=sdkconfig.$Target" -p $Port set-target $Target flash monitor
} else {
    & idf.py -B "build-$Target" -D "SDKCONFIG=sdkconfig.$Target" -p $Port set-target $Target flash monitor
}
