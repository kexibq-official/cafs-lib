from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import numpy as np
from scipy.io import wavfile


BG = "#0d1117"
FG = "#c9d1d9"
GRID = "#30363d"
DIM = "#1f2937"
ACCENT_READ = "#58a6ff"
ACCENT_HIT = "#22c55e"
ACCENT_MISS = "#a371f7"
ACCENT_SPILL = "#f85149"
ACCENT_EMIT = "#f0883e"


def palette_color(rank: int, n: int) -> str:
    cmap = plt.get_cmap("viridis")
    return cmap(0.15 + 0.7 * rank / max(n - 1, 1))


def load(path: str) -> list[dict]:
    raw = json.load(open(path))
    return [e for e in raw if e.get("op") != "END"]


class World:
    def __init__(self, events: list[dict]) -> None:
        meta = next(e for e in events if e["op"] == "INIT")
        self.N = meta["N"]
        self.K = meta["K"]
        self.M = meta["M"]
        self.input = next(e["data"] for e in events if e["op"] == "INPUT")
        self.palette = next(e["keys"] for e in events if e["op"] == "PALETTE")
        self.rank = {v: i for i, v in enumerate(self.palette)}
        self.buckets = [[None] * 4 for _ in range(self.M)]
        self.output = [None] * self.N
        self.phase = "init"
        self.cur_input_i: int | None = None
        self.cur_bucket: int | None = None
        self.cur_slot: int | None = None
        self.cur_op: str = ""
        self.spill_count = 0
        self.sample_taken: list[int] = []
        self.K_est: int | None = None
        self.dense_pairs: list[tuple] = []
        self.cur_emit_pos: int | None = None


def step(world: World, e: dict) -> str:
    op = e["op"]
    world.cur_op = op
    if op == "SAMPLE":
        world.cur_input_i = e["i"]
        world.sample_taken.append(e["v"])
        world.phase = "sample"
    elif op == "CHAO1":
        world.K_est = e["K_est"]
        world.cur_input_i = None
    elif op == "PHASE":
        world.phase = e["name"]
        world.cur_input_i = None
        world.cur_bucket = None
        world.cur_slot = None
    elif op == "BUCKET":
        b = e["bucket"]; s = e["slot"]
        world.buckets[b][s] = (e["v"], e["new_count"])
        world.cur_input_i = e["i"]
        world.cur_bucket = b
        world.cur_slot = s
        world.cur_op = "BUCKET_HIT" if e.get("hit") == 1 else "BUCKET_MISS"
    elif op == "SPILL":
        world.spill_count += e.get("run", 1)
        world.cur_input_i = e["i"]
    elif op == "PAIR":
        world.dense_pairs.append((e["key"], e["count"]))
    elif op == "EMIT":
        world.output[e["pos"]] = e["key"]
        world.cur_emit_pos = e["pos"]
        world.cur_input_i = None
    return op


def draw(world: World, fig, ax_in, ax_buckets, ax_out, ax_label) -> None:
    fig.patch.set_facecolor(BG)

    ax_in.clear()
    ax_in.set_facecolor(BG)
    ax_in.set_xlim(-0.5, world.N - 0.5)
    ax_in.set_ylim(0, max(world.palette) * 1.1)
    ax_in.set_xticks([]); ax_in.set_yticks([])
    for spine in ax_in.spines.values(): spine.set_visible(False)
    bar_colors = [DIM] * world.N
    if world.cur_input_i is not None and world.cur_input_i < world.N:
        bar_colors[world.cur_input_i] = ACCENT_READ
    ax_in.bar(range(world.N), world.input, width=0.9, color=bar_colors,
              edgecolor="none")
    if world.cur_input_i is not None:
        i = world.cur_input_i
        ax_in.bar([i], [world.input[i]], width=0.9, color=ACCENT_READ,
                  edgecolor="none")
    ax_in.text(0.01, 0.95, "input", transform=ax_in.transAxes,
               color=FG, fontsize=10, va="top", family="monospace")

    ax_buckets.clear()
    ax_buckets.set_facecolor(BG)
    ax_buckets.set_xlim(-0.1, world.M + 0.1)
    ax_buckets.set_ylim(-0.3, 4.4)
    ax_buckets.set_xticks([]); ax_buckets.set_yticks([])
    ax_buckets.invert_yaxis()
    for spine in ax_buckets.spines.values(): spine.set_visible(False)
    for b in range(world.M):
        for s in range(4):
            ent = world.buckets[b][s]
            x, y = b, s
            if ent is None:
                rect = mpatches.Rectangle((x + 0.08, y + 0.08), 0.84, 0.84,
                                          facecolor=DIM, edgecolor=GRID,
                                          linewidth=0.6)
                ax_buckets.add_patch(rect)
            else:
                key, cnt = ent
                rank = world.rank.get(key, 0)
                color = palette_color(rank, max(world.K, 1))
                rect = mpatches.Rectangle((x + 0.08, y + 0.08), 0.84, 0.84,
                                          facecolor=color, edgecolor=GRID,
                                          linewidth=0.6, alpha=0.85)
                ax_buckets.add_patch(rect)
                ax_buckets.text(x + 0.5, y + 0.5, f"{cnt}",
                                ha="center", va="center", color="#0d1117",
                                fontsize=9, fontweight="bold",
                                family="monospace")
            if (world.cur_bucket == b and world.cur_slot == s
                    and world.phase == "hot_loop"):
                halo = mpatches.Rectangle((x + 0.04, y + 0.04), 0.92, 0.92,
                                          facecolor="none",
                                          edgecolor=ACCENT_HIT
                                          if world.cur_op == "BUCKET_HIT"
                                          else ACCENT_MISS,
                                          linewidth=2.2)
                ax_buckets.add_patch(halo)
    ax_buckets.text(0.01, 1.04, f"buckets ({world.M} x 4)",
                    transform=ax_buckets.transAxes,
                    color=FG, fontsize=10, va="bottom", family="monospace")

    ax_out.clear()
    ax_out.set_facecolor(BG)
    ax_out.set_xlim(-0.5, world.N - 0.5)
    ax_out.set_ylim(0, max(world.palette) * 1.1)
    ax_out.set_xticks([]); ax_out.set_yticks([])
    for spine in ax_out.spines.values(): spine.set_visible(False)
    out_vals = [v if v is not None else 0 for v in world.output]
    out_colors = []
    for v in world.output:
        if v is None:
            out_colors.append(DIM)
        else:
            r = world.rank.get(v, 0)
            out_colors.append(palette_color(r, max(world.K, 1)))
    ax_out.bar(range(world.N), out_vals, width=0.9, color=out_colors,
               edgecolor="none")
    if world.cur_emit_pos is not None:
        i = world.cur_emit_pos
        if 0 <= i < world.N and world.output[i] is not None:
            ax_out.bar([i], [world.output[i]], width=0.9, color=ACCENT_EMIT,
                       edgecolor="none")
    ax_out.text(0.01, 0.95, "output", transform=ax_out.transAxes,
                color=FG, fontsize=10, va="top", family="monospace")

    ax_label.clear()
    ax_label.set_facecolor(BG)
    ax_label.set_xticks([]); ax_label.set_yticks([])
    for spine in ax_label.spines.values(): spine.set_visible(False)
    stage_titles = {
        "init":        "init",
        "sample":      "stage 1: Chao1 sample (1024 strided picks, K_est)",
        "hot_loop":    "stage 2: hot loop (one AVX2 cmpeq per element)",
        "reconstruct": "stage 3: reconstruct (collect pairs, sort by key)",
        "emit":        "stage 4: emit (fill_n: monotonic write of output)",
    }
    title = stage_titles.get(world.phase, world.phase)
    ax_label.text(0.01, 0.55,
                  f"CAFS  N={world.N}  K={world.K}",
                  color=FG, fontsize=14, fontweight="bold",
                  family="monospace", transform=ax_label.transAxes)
    ax_label.text(0.01, 0.18, title,
                  color=ACCENT_HIT, fontsize=11,
                  family="monospace", transform=ax_label.transAxes)
    info = []
    if world.K_est is not None:
        info.append(f"K_est = {world.K_est}")
    if world.spill_count:
        info.append(f"spill = {world.spill_count}")
    if info:
        ax_label.text(0.99, 0.55, "  |  ".join(info),
                      color=FG, fontsize=10, ha="right",
                      family="monospace", transform=ax_label.transAxes)


def should_emit_frame(op: str, world: World, sample_idx: int,
                      emit_idx: int) -> bool:
    if op in ("INIT", "INPUT", "PALETTE", "COLLECT"):
        return False
    if op == "SAMPLE":
        return sample_idx % 3 == 0
    if op == "EMIT":
        return emit_idx % 2 == 0
    if op == "PAIR":
        return False
    return True


def render(events_path: str, out_dir: str, fps: int = 30) -> dict:
    events = load(events_path)
    world = World(events)
    out = Path(out_dir)
    frames_dir = out / "frames"
    frames_dir.mkdir(parents=True, exist_ok=True)

    fig = plt.figure(figsize=(12.8, 7.2), dpi=100)
    gs = fig.add_gridspec(4, 1, height_ratios=[1.6, 0.5, 3.5, 1.6], hspace=0.25)
    ax_in = fig.add_subplot(gs[0])
    ax_label = fig.add_subplot(gs[1])
    ax_buckets = fig.add_subplot(gs[2])
    ax_out = fig.add_subplot(gs[3])

    sample_idx = 0
    emit_idx = 0
    frame_count = 0
    audio_events: list[dict] = []
    seconds_per_frame = 1.0 / fps

    for e in events:
        op = step(world, e)
        if op == "SAMPLE":
            sample_idx += 1
        elif op == "EMIT":
            emit_idx += 1

        if should_emit_frame(op, world, sample_idx, emit_idx):
            t = frame_count * seconds_per_frame
            if op in ("BUCKET", "SAMPLE", "EMIT", "SPILL"):
                key = e.get("v") or e.get("key")
                if key is not None:
                    rank = world.rank.get(key, 0)
                    audio_events.append({
                        "t": t,
                        "rank": rank,
                        "kind": world.cur_op,
                    })
            draw(world, fig, ax_in, ax_buckets, ax_out, ax_label)
            fig.savefig(frames_dir / f"f{frame_count:05d}.png",
                        facecolor=BG, dpi=100)
            frame_count += 1

    for _ in range(fps):
        draw(world, fig, ax_in, ax_buckets, ax_out, ax_label)
        fig.savefig(frames_dir / f"f{frame_count:05d}.png",
                    facecolor=BG, dpi=100)
        frame_count += 1

    plt.close(fig)
    return {
        "frame_count": frame_count,
        "audio_events": audio_events,
        "duration_s": frame_count / fps,
        "K": world.K,
        "N": world.N,
    }


def synth_audio(audio_events: list[dict], duration_s: float, n_keys: int,
                wav_path: str, sr: int = 44100) -> None:
    n_samples = int(duration_s * sr) + sr
    track = np.zeros(n_samples, dtype=np.float32)

    def add_tone(t0, freq, dur, amp, env="bell"):
        i0 = int(t0 * sr)
        i1 = min(i0 + int(dur * sr), n_samples)
        if i1 <= i0:
            return
        n = i1 - i0
        x = np.arange(n) / sr
        wave = np.sin(2 * np.pi * freq * x).astype(np.float32)
        if env == "bell":
            envelope = np.exp(-x * 14.0)
        elif env == "click":
            envelope = np.exp(-x * 30.0)
        elif env == "thump":
            envelope = np.exp(-x * 6.0)
        else:
            envelope = np.ones_like(x)
        track[i0:i1] += (amp * wave * envelope).astype(np.float32)

    base = 220.0
    for ev in audio_events:
        rank = ev["rank"]
        kind = ev["kind"]
        f = base * (2 ** (rank / max(n_keys - 1, 1)))
        if kind == "BUCKET_HIT":
            add_tone(ev["t"], f, 0.10, 0.20, env="bell")
        elif kind == "BUCKET_MISS":
            add_tone(ev["t"], f * 2, 0.12, 0.30, env="click")
        elif kind == "SAMPLE":
            add_tone(ev["t"], f * 1.5, 0.06, 0.10, env="bell")
        elif kind == "SPILL":
            add_tone(ev["t"], 90.0, 0.20, 0.40, env="thump")
        elif kind == "EMIT":
            add_tone(ev["t"], f, 0.05, 0.12, env="bell")
        else:
            add_tone(ev["t"], f, 0.05, 0.10, env="bell")

    peak = float(np.max(np.abs(track)))
    if peak > 0:
        track *= 0.85 / peak
    pcm = (track * 32767).astype(np.int16)
    wavfile.write(wav_path, sr, pcm)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--events", default="events.json")
    p.add_argument("--out-dir", default="render")
    p.add_argument("--fps", type=int, default=30)
    args = p.parse_args()

    info = render(args.events, args.out_dir, fps=args.fps)
    print(f"frames: {info['frame_count']}")
    print(f"duration: {info['duration_s']:.2f}s")

    wav = Path(args.out_dir) / "audio.wav"
    synth_audio(info["audio_events"], info["duration_s"], info["K"], str(wav))
    print(f"wav: {wav}")
