#!/usr/bin/env bash
set -e

N=${N:-256}
K=${K:-8}
M=${M:-16}
SEED=${SEED:-7}
FPS=${FPS:-30}

cd "$(dirname "$0")"

echo "[1/4] compile sos_trace"
g++ -O2 -std=c++20 sos_trace.cpp -o sos_trace

echo "[2/4] run tracer (N=$N K=$K M=$M seed=$SEED)"
./sos_trace --N "$N" --K "$K" --M "$M" --seed "$SEED" --out events.json

echo "[3/4] render frames + audio (fps=$FPS)"
rm -rf render
python render.py --events events.json --out-dir render --fps "$FPS"

echo "[4/4] ffmpeg encode -> examples/cafs_demo.{mp4,gif}"
mkdir -p examples
ffmpeg -y -loglevel error -framerate "$FPS" \
    -i render/frames/f%05d.png -i render/audio.wav \
    -c:v libx264 -pix_fmt yuv420p -crf 23 \
    -c:a aac -b:a 128k -shortest \
    examples/cafs_demo.mp4

ffmpeg -y -loglevel error -framerate "$FPS" \
    -i render/frames/f%05d.png \
    -vf "fps=20,scale=900:-2:flags=lanczos,split[s0][s1];[s0]palettegen=max_colors=128[p];[s1][p]paletteuse=dither=bayer:bayer_scale=4" \
    examples/cafs_demo.gif

rm -rf render sos_trace
echo "done."
ls -la examples/cafs_demo.mp4 examples/cafs_demo.gif
