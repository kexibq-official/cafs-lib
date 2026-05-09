param(
    [int]$N = 256,
    [int]$K = 8,
    [int]$M = 16,
    [int]$Seed = 7,
    [int]$Fps = 30
)

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
Set-Location $Root

$gpp = "C:\ProgramData\mingw64\mingw64\bin\g++.exe"
if (-not (Test-Path $gpp)) {
    $gpp = (Get-Command g++ -ErrorAction SilentlyContinue).Source
}
if (-not $gpp) {
    Write-Error "g++ not found. Install MinGW-w64 or set the path manually."
    exit 1
}

Write-Host "[1/4] compile sos_trace ($gpp)"
& $gpp -O2 -std=c++20 sos_trace.cpp -o sos_trace.exe
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[2/4] run tracer (N=$N K=$K M=$M seed=$Seed)"
& .\sos_trace.exe --N $N --K $K --M $M --seed $Seed --out events.json
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[3/4] render frames + audio (fps=$Fps)"
Remove-Item -Recurse -Force render -ErrorAction SilentlyContinue
python render.py --events events.json --out-dir render --fps $Fps
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[4/4] ffmpeg encode -> examples\cafs_demo.{mp4,gif}"
New-Item -ItemType Directory -Force -Path examples | Out-Null
& ffmpeg -y -loglevel error -framerate $Fps `
    -i render\frames\f%05d.png -i render\audio.wav `
    -c:v libx264 -pix_fmt yuv420p -crf 23 `
    -c:a aac -b:a 128k -shortest `
    examples\cafs_demo.mp4
if ($LASTEXITCODE -ne 0) { exit 1 }

& ffmpeg -y -loglevel error -framerate $Fps `
    -i render\frames\f%05d.png `
    -vf "fps=20,scale=900:-2:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse=dither=bayer:bayer_scale=4" `
    examples\cafs_demo.gif
if ($LASTEXITCODE -ne 0) { exit 1 }

Remove-Item -Recurse -Force render -ErrorAction SilentlyContinue
Remove-Item -Force sos_trace.exe -ErrorAction SilentlyContinue

$mp4 = Get-Item examples\cafs_demo.mp4
$gif = Get-Item examples\cafs_demo.gif
Write-Host "done."
Write-Host ("  mp4: {0} ({1:N1} KB)" -f $mp4.FullName, ($mp4.Length / 1KB))
Write-Host ("  gif: {0} ({1:N1} KB)" -f $gif.FullName, ($gif.Length / 1KB))
