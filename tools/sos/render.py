from __future__ import annotations

import argparse
import json
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.patches as mpatches
import matplotlib.pyplot as plt
import numpy as np
from scipy.io import wavfile


BG = "#0d1117"
PANEL = "#161b22"
DIM = "#21262d"
EDGE = "#30363d"
FG = "#c9d1d9"
MUTED = "#8b949e"
ACCENT_BLUE = "#58a6ff"
ACCENT_HIT = "#22c55e"
ACCENT_MISS = "#a371f7"
ACCENT_SPILL = "#f85149"
ACCENT_EMIT = "#f0883e"
TEXT_DARK = "#0d1117"

GOLDEN = 0x9E3779B97F4A7C15
MASK64 = 0xFFFFFFFFFFFFFFFF


def palette_color(rank, n):
    return plt.get_cmap("viridis")(0.18 + 0.7 * rank / max(n - 1, 1))


def color_to_hex(c):
    return "#%02x%02x%02x" % (int(c[0] * 255), int(c[1] * 255), int(c[2] * 255))


def hash_bucket(v, M):
    shift = 64 - int(np.log2(M))
    return ((v * GOLDEN) & MASK64) >> shift & (M - 1)


def load(path):
    raw = json.load(open(path))
    return [e for e in raw if e.get("op") != "END"]


class Layout:
    HEADER_Y = 0.965
    INPUT_Y0 = 0.83
    INPUT_Y1 = 0.92
    HASH_Y = 0.755
    GRID_Y0 = 0.30
    GRID_Y1 = 0.71
    OUTPUT_Y0 = 0.11
    OUTPUT_Y1 = 0.21
    FOOTER_Y = 0.045
    LEFT = 0.06
    RIGHT = 0.94


class Painter:
    def __init__(self, fig, palette, rank_map, K, N, M):
        self.fig = fig
        self.palette = palette
        self.rank = rank_map
        self.K = K; self.N = N; self.M = M
        self._overlay_ax = None

    def color(self, key):
        return palette_color(self.rank.get(key, 0), max(self.K, 1))

    def input_x(self, i):
        L = Layout.RIGHT - Layout.LEFT
        return Layout.LEFT + (i + 0.5) / self.N * L

    def output_x(self, i):
        L = Layout.RIGHT - Layout.LEFT
        return Layout.LEFT + (i + 0.5) / self.N * L

    def grid_cell_box(self, bucket_idx, lane):
        L = Layout.RIGHT - Layout.LEFT
        col_w = L / self.M
        x = Layout.LEFT + bucket_idx * col_w
        H = Layout.GRID_Y1 - Layout.GRID_Y0
        row_h = H / 4
        y = Layout.GRID_Y1 - (lane + 1) * row_h
        gap_x = col_w * 0.06
        gap_y = row_h * 0.12
        return (x + gap_x, y + gap_y, col_w - 2 * gap_x, row_h - 2 * gap_y)

    def grid_col_box(self, bucket_idx):
        L = Layout.RIGHT - Layout.LEFT
        col_w = L / self.M
        x = Layout.LEFT + bucket_idx * col_w
        return (x, Layout.GRID_Y0, col_w, Layout.GRID_Y1 - Layout.GRID_Y0)

    def clear_figure(self):
        self.fig.clf()
        self.fig.patch.set_facecolor(BG)
        ax = self.fig.add_axes([0, 0, 1, 1])
        ax.set_facecolor(BG); ax.patch.set_alpha(0)
        ax.set_xlim(0, 1); ax.set_ylim(0, 1)
        ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values(): s.set_visible(False)
        self._overlay_ax = ax
        return ax

    def draw_header(self, stage_text=""):
        self.fig.text(0.04, Layout.HEADER_Y, "CAFS",
                      color=FG, fontsize=18, fontweight="bold",
                      family="monospace", va="center")
        self.fig.text(0.115, Layout.HEADER_Y,
                      f"N = {self.N}     K = {self.K}     M = {self.M}",
                      color=MUTED, fontsize=11, family="monospace",
                      va="center")
        if stage_text:
            self.fig.text(0.96, Layout.HEADER_Y, stage_text,
                          color=ACCENT_HIT, fontsize=12, family="monospace",
                          va="center", ha="right")

    def draw_caption(self, text, color=MUTED):
        self.fig.text(0.5, Layout.FOOTER_Y, text, color=color, fontsize=11,
                      family="monospace", va="center", ha="center")

    def draw_input(self, input_data, highlight_range=None,
                   highlight_color=ACCENT_BLUE, label="input"):
        ax = self.fig.add_axes([Layout.LEFT, Layout.INPUT_Y0,
                                Layout.RIGHT - Layout.LEFT,
                                Layout.INPUT_Y1 - Layout.INPUT_Y0])
        ax.set_facecolor(BG)
        ax.set_xlim(-0.5, self.N - 0.5)
        ax.set_ylim(0, max(self.palette) * 1.10)
        ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values(): s.set_visible(False)
        colors = [DIM] * self.N
        if highlight_range:
            for j in highlight_range:
                if 0 <= j < self.N:
                    colors[j] = highlight_color
        ax.bar(range(self.N), input_data, width=0.95, color=colors,
               edgecolor="none", linewidth=0)
        if label:
            self.fig.text(Layout.RIGHT, Layout.INPUT_Y0 - 0.018, label,
                          color=MUTED, fontsize=10, family="monospace",
                          va="top", ha="right")

    def draw_output(self, output_data, highlight_range=None,
                    highlight_color=ACCENT_EMIT, label="output"):
        ax = self.fig.add_axes([Layout.LEFT, Layout.OUTPUT_Y0,
                                Layout.RIGHT - Layout.LEFT,
                                Layout.OUTPUT_Y1 - Layout.OUTPUT_Y0])
        ax.set_facecolor(BG)
        ax.set_xlim(-0.5, self.N - 0.5)
        ax.set_ylim(0, max(self.palette) * 1.10)
        ax.set_xticks([]); ax.set_yticks([])
        for s in ax.spines.values(): s.set_visible(False)
        out_vals = []
        out_colors = []
        for v in output_data:
            if v is None:
                out_colors.append(DIM); out_vals.append(0)
            else:
                out_colors.append(self.color(v)); out_vals.append(v)
        ax.bar(range(self.N), out_vals, width=0.95, color=out_colors,
               edgecolor="none", linewidth=0)
        if highlight_range:
            for j in highlight_range:
                if 0 <= j < self.N and output_data[j] is not None:
                    ax.bar([j], [output_data[j]], width=0.95,
                           color=highlight_color, edgecolor="none")
        if label:
            self.fig.text(Layout.RIGHT, Layout.OUTPUT_Y1 + 0.005, label,
                          color=MUTED, fontsize=10, family="monospace",
                          va="bottom", ha="right")

    def draw_hash_formula(self, ax, value, bucket_idx, highlight=True):
        cx = (Layout.LEFT + Layout.RIGHT) / 2
        text = (f"hash(v) = (v * 0x9E3779B97F4A7C15) >> 60 & 0xF      "
                f"v = {value}   ->   bucket {bucket_idx}")
        color = ACCENT_BLUE if highlight else MUTED
        self.fig.text(cx, Layout.HASH_Y, text,
                      color=color, fontsize=11, family="monospace",
                      ha="center", va="center")

    def draw_bucket_grid(self, ax, buckets, active=None,
                         drained=None, dim=False):
        for b in range(self.M):
            col_x, col_y, col_w, col_h = self.grid_col_box(b)
            base = mpatches.Rectangle(
                (col_x + col_w * 0.02, col_y), col_w * 0.96, col_h,
                facecolor=PANEL, edgecolor=EDGE, linewidth=0.4,
                alpha=0.35 if dim else 0.55)
            ax.add_patch(base)

        for b in range(self.M):
            for lane in range(4):
                x, y, w, h = self.grid_cell_box(b, lane)
                ent = buckets[b][lane]
                is_active = (active is not None
                             and active[0] == b and active[1] == lane)
                if ent is None:
                    face = DIM
                else:
                    face = color_to_hex(self.color(ent[0]))
                alpha = 0.35 if (dim and not is_active) else 1.0
                if drained and (b, lane) in drained:
                    face = DIM
                rect = mpatches.FancyBboxPatch(
                    (x, y), w, h,
                    boxstyle="round,pad=0.0005,rounding_size=0.005",
                    facecolor=face, edgecolor=EDGE, linewidth=0.5,
                    alpha=alpha)
                ax.add_patch(rect)
                if ent is not None and not (drained and (b, lane) in drained):
                    key, cnt = ent
                    self.fig.text(x + w / 2, y + h * 0.62, f"{key}",
                                  color=TEXT_DARK, fontsize=7,
                                  family="monospace", ha="center",
                                  va="center", alpha=alpha)
                    self.fig.text(x + w / 2, y + h * 0.28, f"x{cnt}",
                                  color=TEXT_DARK, fontsize=10,
                                  family="monospace", ha="center",
                                  va="center", fontweight="bold",
                                  alpha=alpha)

        for b in range(self.M):
            col_x, col_y, col_w, col_h = self.grid_col_box(b)
            self.fig.text(col_x + col_w / 2, col_y - 0.018, f"b{b}",
                          color=MUTED, fontsize=8, family="monospace",
                          ha="center", va="top")

        for lane in range(4):
            self.fig.text(Layout.LEFT - 0.012,
                          self.grid_cell_box(0, lane)[1]
                          + self.grid_cell_box(0, lane)[3] / 2,
                          f"l{lane}", color=MUTED, fontsize=9,
                          family="monospace", ha="right", va="center")

        if active is not None:
            b = active[0]; lane = active[1]
            cx, cy, cw, ch = self.grid_cell_box(b, lane)
            kind = active[2] if len(active) > 2 else "BUCKET_HIT"
            halo_color = (ACCENT_HIT if kind == "BUCKET_HIT"
                          else ACCENT_MISS)
            halo = mpatches.FancyBboxPatch(
                (cx - 0.004, cy - 0.006), cw + 0.008, ch + 0.012,
                boxstyle="round,pad=0.0005,rounding_size=0.006",
                facecolor="none", edgecolor=halo_color, linewidth=2.2)
            ax.add_patch(halo)
            col_x, col_y, col_w, col_h = self.grid_col_box(b)
            col_outline = mpatches.Rectangle(
                (col_x + col_w * 0.01, col_y - 0.006),
                col_w * 0.98, col_h + 0.012,
                facecolor="none", edgecolor=ACCENT_BLUE, linewidth=1.3,
                alpha=0.55)
            ax.add_patch(col_outline)

    def draw_arrow_input_to_cell(self, ax, input_idx, bucket_idx, lane,
                                 alpha=1.0, color=ACCENT_BLUE):
        sx = self.input_x(input_idx); sy = Layout.INPUT_Y0 - 0.005
        x, y, w, h = self.grid_cell_box(bucket_idx, lane)
        tx = x + w / 2; ty = y + h + 0.005
        arrow = mpatches.FancyArrowPatch(
            (sx, sy), (tx, ty),
            arrowstyle="-|>", mutation_scale=14,
            color=color, linewidth=1.8, alpha=alpha,
            connectionstyle="arc3,rad=0.10", zorder=5)
        ax.add_patch(arrow)

    def draw_cell_to_tape_arrow(self, ax, bucket_idx, lane,
                                tape_x, tape_y, alpha=1.0):
        x, y, w, h = self.grid_cell_box(bucket_idx, lane)
        sx = x + w / 2; sy = y - 0.002
        arrow = mpatches.FancyArrowPatch(
            (sx, sy), (tape_x, tape_y),
            arrowstyle="-|>", mutation_scale=12,
            color=ACCENT_HIT, linewidth=1.5, alpha=alpha,
            connectionstyle="arc3,rad=0.12", zorder=5)
        ax.add_patch(arrow)

    def draw_freqset_panel(self, ax, samples_dict, u, f1, f2,
                           k_est=None, formula_step=0):
        cx = (Layout.LEFT + Layout.RIGHT) / 2
        cy = (Layout.GRID_Y0 + Layout.GRID_Y1) / 2

        panel = mpatches.FancyBboxPatch(
            (Layout.LEFT + 0.04, Layout.GRID_Y0 + 0.02),
            (Layout.RIGHT - Layout.LEFT) - 0.08,
            (Layout.GRID_Y1 - Layout.GRID_Y0) - 0.04,
            boxstyle="round,pad=0.001,rounding_size=0.012",
            facecolor=PANEL, edgecolor=EDGE, linewidth=1.0)
        ax.add_patch(panel)

        self.fig.text(cx, Layout.GRID_Y1 - 0.025,
                      "FreqSet  /  4096 slots, 16-bit counts",
                      color=FG, fontsize=12, family="monospace",
                      ha="center", va="center")

        keys = sorted(samples_dict.keys(),
                      key=lambda k: self.rank.get(k, 0))
        if keys:
            bar_y0 = Layout.GRID_Y1 - 0.20
            bar_y1 = Layout.GRID_Y1 - 0.075
            bar_left = Layout.LEFT + 0.10
            bar_right = Layout.RIGHT - 0.10
            bw = (bar_right - bar_left) / max(len(keys), 1)
            max_cnt = max(samples_dict.values()) if samples_dict else 1
            for i, k in enumerate(keys):
                cnt = samples_dict[k]
                bx = bar_left + i * bw + bw * 0.10
                bw_actual = bw * 0.80
                bh = (bar_y1 - bar_y0) * (cnt / max(max_cnt, 1))
                color = color_to_hex(self.color(k))
                rect = mpatches.Rectangle(
                    (bx, bar_y0), bw_actual, bh,
                    facecolor=color, edgecolor=EDGE, linewidth=0.4)
                ax.add_patch(rect)
                if cnt == 1:
                    self.fig.text(bx + bw_actual / 2, bar_y1 + 0.012,
                                  "f1", color=ACCENT_BLUE, fontsize=8,
                                  family="monospace",
                                  ha="center", va="center")
                elif cnt == 2:
                    self.fig.text(bx + bw_actual / 2, bar_y1 + 0.012,
                                  "f2", color=ACCENT_HIT, fontsize=8,
                                  family="monospace",
                                  ha="center", va="center")
                self.fig.text(bx + bw_actual / 2, bar_y0 - 0.012,
                              f"{cnt}",
                              color=MUTED, fontsize=8,
                              family="monospace",
                              ha="center", va="top")

        stats_y = Layout.GRID_Y0 + 0.090
        self.fig.text(cx, stats_y,
                      f"u = {u}    f1 = {f1}    f2 = {f2}",
                      color=FG, fontsize=12, family="monospace",
                      ha="center", va="center")

        formula_y = Layout.GRID_Y0 + 0.045
        if formula_step == 0:
            txt = "K_hat  =  u  +  f1^2  /  (2 * (f2 + 1))"
            color = MUTED
        elif formula_step == 1:
            txt = (f"K_hat  =  {u}  +  {f1}^2  /  (2 * ({f2} + 1))")
            color = ACCENT_BLUE
        elif formula_step == 2:
            v = f1 * f1
            d = 2 * (f2 + 1)
            txt = f"K_hat  =  {u}  +  {v}  /  {d}  =  {k_est}"
            color = ACCENT_HIT
        else:
            txt = f"K_hat  =  {k_est}"
            color = ACCENT_HIT
        self.fig.text(cx, formula_y, txt, color=color, fontsize=12,
                      family="monospace", ha="center", va="center")

    def draw_dense_pairs_tape(self, ax, pairs, current_idx=None,
                              y_top_offset=0.0, slots=None):
        cx = (Layout.LEFT + Layout.RIGHT) / 2
        n = len(pairs)
        if n == 0:
            return None, None
        full_w = (Layout.RIGHT - Layout.LEFT) * 0.78
        slot_count = max(slots or n, 1)
        cell_w = full_w / slot_count * 0.92
        gap = full_w / slot_count * 0.08
        cell_h = 0.055
        cell_y0 = Layout.OUTPUT_Y1 + 0.008 + y_top_offset
        x0 = cx - (n * (cell_w + gap) - gap) / 2
        positions = []
        for i, (k, c) in enumerate(pairs):
            x = x0 + i * (cell_w + gap)
            color = color_to_hex(self.color(k))
            face_alpha = 1.0 if current_idx is None or i == current_idx else 0.55
            rect = mpatches.FancyBboxPatch(
                (x, cell_y0), cell_w, cell_h,
                boxstyle="round,pad=0.001,rounding_size=0.008",
                facecolor=color, edgecolor=EDGE, linewidth=0.8,
                alpha=face_alpha)
            ax.add_patch(rect)
            self.fig.text(x + cell_w / 2, cell_y0 + cell_h * 0.68, f"{k}",
                          color=TEXT_DARK, fontsize=8, family="monospace",
                          ha="center", va="center", alpha=face_alpha)
            self.fig.text(x + cell_w / 2, cell_y0 + cell_h * 0.28, f"x{c}",
                          color=TEXT_DARK, fontsize=9, family="monospace",
                          ha="center", va="center", fontweight="bold",
                          alpha=face_alpha)
            if current_idx is not None and i == current_idx:
                halo = mpatches.FancyBboxPatch(
                    (x - 0.003, cell_y0 - 0.005), cell_w + 0.006,
                    cell_h + 0.010,
                    boxstyle="round,pad=0.0005,rounding_size=0.008",
                    facecolor="none", edgecolor=ACCENT_EMIT, linewidth=2.0)
                ax.add_patch(halo)
            positions.append((x + cell_w / 2, cell_y0))
        return positions, cell_y0

    def draw_pair_to_output_arrow(self, ax, pair_x, pair_y_bottom,
                                  output_pos, output_count,
                                  color, alpha=1.0):
        L = Layout.RIGHT - Layout.LEFT
        x_left = Layout.LEFT + (output_pos / self.N) * L
        x_right = Layout.LEFT + ((output_pos + output_count) / self.N) * L
        x_mid = (x_left + x_right) / 2
        ty = Layout.OUTPUT_Y1 + 0.005
        arrow = mpatches.FancyArrowPatch(
            (pair_x, pair_y_bottom), (x_mid, ty),
            arrowstyle="-|>", mutation_scale=18,
            color=color, linewidth=2.0, alpha=alpha,
            connectionstyle="arc3,rad=0.06", zorder=5)
        ax.add_patch(arrow)


def synth_audio(audio_events, duration_s, n_keys, wav_path, sr=44100):
    n_samples = int(duration_s * sr) + sr
    track = np.zeros(n_samples, dtype=np.float32)

    def add_tone(t0, freq, dur, amp, env="bell"):
        i0 = int(t0 * sr)
        i1 = min(i0 + int(dur * sr), n_samples)
        if i1 <= i0: return
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
        rank = ev["rank"]; kind = ev["kind"]
        f = base * (2 ** (rank / max(n_keys - 1, 1)))
        if kind == "BUCKET_HIT":
            add_tone(ev["t"], f, 0.10, 0.20, env="bell")
        elif kind == "BUCKET_MISS":
            add_tone(ev["t"], f * 2, 0.12, 0.30, env="click")
        elif kind == "SAMPLE":
            add_tone(ev["t"], f * 1.5, 0.06, 0.10, env="bell")
        elif kind == "EMIT":
            add_tone(ev["t"], f, 0.05, 0.16, env="bell")
        elif kind == "COLLECT":
            add_tone(ev["t"], f, 0.06, 0.10, env="bell")

    peak = float(np.max(np.abs(track)))
    if peak > 0:
        track *= 0.85 / peak
    pcm = (track * 32767).astype(np.int16)
    wavfile.write(wav_path, sr, pcm)


class Movie:
    def __init__(self, events, fps=30):
        self.events = events
        self.fps = fps
        meta = next(e for e in events if e["op"] == "INIT")
        self.N = meta["N"]; self.K = meta["K"]; self.M = meta["M"]
        self.input = next(e["data"] for e in events if e["op"] == "INPUT")
        self.palette = next(e["keys"] for e in events if e["op"] == "PALETTE")
        self.rank = {v: i for i, v in enumerate(self.palette)}
        self.fig = plt.figure(figsize=(12.8, 7.2), dpi=100)
        self.fig.patch.set_facecolor(BG)
        self.painter = Painter(self.fig, self.palette, self.rank,
                               self.K, self.N, self.M)
        self.buckets = [[None] * 4 for _ in range(self.M)]
        self.output = [None] * self.N
        self.dense_pairs = []
        self.frame_count = 0
        self.audio_events = []

    def save(self, frames_dir):
        path = frames_dir / f"f{self.frame_count:05d}.png"
        self.fig.savefig(path, facecolor=BG, dpi=100)
        self.frame_count += 1

    def _t(self):
        return self.frame_count / self.fps

    def title_screen(self, frames_dir):
        for k in range(45):
            self.painter.clear_figure()
            alpha = min(1.0, k / 12.0) if k < 36 else max(0.0,
                                                          1.0 - (k - 36) / 9.0)
            self.fig.text(0.5, 0.62, "CAFS sort",
                          color=FG, fontsize=42, fontweight="bold",
                          family="monospace", ha="center", va="center",
                          alpha=alpha)
            self.fig.text(0.5, 0.50, f"N = {self.N}    K = {self.K}",
                          color=ACCENT_BLUE, fontsize=18, family="monospace",
                          ha="center", va="center", alpha=alpha)
            self.fig.text(0.5, 0.42,
                          "low-cardinality integer sort  /  in 4 stages",
                          color=MUTED, fontsize=12, family="monospace",
                          ha="center", va="center", alpha=alpha)
            self.save(frames_dir)

    def setup_screen(self, frames_dir):
        for _ in range(50):
            self.painter.clear_figure()
            self.painter.draw_header("setup")
            self.painter.draw_input(self.input)
            self.painter.draw_output(self.output)
            self.painter.draw_caption(
                f"K = {self.K} distinct values among N = {self.N} elements"
                f"   ->   count, then emit", color=FG)
            self.save(frames_dir)

    def sample_stage(self, frames_dir):
        sample_events = [e for e in self.events if e["op"] == "SAMPLE"]
        chao1 = next((e for e in self.events if e["op"] == "CHAO1"), None)

        seen = {}
        for idx, e in enumerate(sample_events):
            v = e["v"]
            seen[v] = seen.get(v, 0) + 1
            if idx % 2 != 0 and idx != len(sample_events) - 1:
                continue
            u = len(seen)
            f1 = sum(1 for c in seen.values() if c == 1)
            f2 = sum(1 for c in seen.values() if c == 2)
            for hold in range(2):
                ax = self.painter.clear_figure()
                self.painter.draw_header("stage 1/4: Chao1 sample")
                self.painter.draw_input(self.input,
                                        highlight_range=[e["i"]],
                                        highlight_color=ACCENT_BLUE)
                self.painter.draw_freqset_panel(ax, seen, u, f1, f2,
                                                k_est=None,
                                                formula_step=0)
                self.painter.draw_output(self.output)
                sx = self.painter.input_x(e["i"])
                sy = Layout.INPUT_Y0 - 0.005
                tx = (Layout.LEFT + Layout.RIGHT) / 2
                ty = Layout.GRID_Y1 - 0.010
                arrow = mpatches.FancyArrowPatch(
                    (sx, sy), (tx, ty),
                    arrowstyle="-|>", mutation_scale=12,
                    color=ACCENT_BLUE, linewidth=1.2, alpha=0.55,
                    connectionstyle="arc3,rad=0.18", zorder=5)
                ax.add_patch(arrow)
                self.painter.draw_caption(
                    f"sample  {idx + 1}/64    "
                    f"FreqSet[hash(v)] += 1")
                self.audio_events.append({
                    "t": self._t(),
                    "rank": self.rank.get(v, 0),
                    "kind": "SAMPLE"})
                self.save(frames_dir)

        if chao1:
            u = chao1["u"]; f1 = chao1["f1"]; f2 = chao1["f2"]
            k_est = chao1["K_est"]
            for step, holds in [(0, 12), (1, 24), (2, 24), (3, 18)]:
                for _ in range(holds):
                    ax = self.painter.clear_figure()
                    self.painter.draw_header("stage 1/4: Chao1 sample")
                    self.painter.draw_input(self.input)
                    self.painter.draw_freqset_panel(ax, seen, u, f1, f2,
                                                    k_est=k_est,
                                                    formula_step=step)
                    self.painter.draw_output(self.output)
                    cap_text = ("scan FreqSet for f1 (singletons) and f2 "
                                "(doubletons)" if step == 0 else
                                "substitute  u, f1, f2  into Chao1" if step == 1 else
                                f"K_hat = {k_est}   ->   M = {self.M} buckets"
                                if step == 2 else
                                f"K_hat = {k_est}   pick M = {self.M} buckets")
                    self.painter.draw_caption(cap_text,
                                              color=(ACCENT_HIT if step >= 2
                                                     else MUTED))
                    self.save(frames_dir)

    def hot_loop_stage(self, frames_dir):
        bucket_events = [e for e in self.events if e["op"] == "BUCKET"]
        animated = min(14, len(bucket_events))

        for idx, e in enumerate(bucket_events):
            self.buckets[e["bucket"]][e["slot"]] = (e["v"], e["new_count"])
            kind = "BUCKET_HIT" if e.get("hit") == 1 else "BUCKET_MISS"
            self.audio_events.append({"t": self._t(),
                                      "rank": self.rank.get(e["v"], 0),
                                      "kind": kind})
            run_len = e.get("run", 1)
            hl = list(range(e["i"], min(self.N, e["i"] + run_len)))

            if idx < animated:
                hold_frames = 7
                for k in range(hold_frames):
                    ax = self.painter.clear_figure()
                    self.painter.draw_header(
                        "stage 2/4: hot loop  /  one AVX2 cmpeq per element")
                    self.painter.draw_input(self.input, highlight_range=hl)
                    self.painter.draw_hash_formula(ax, e["v"], e["bucket"])
                    self.painter.draw_bucket_grid(
                        ax, self.buckets,
                        active=(e["bucket"], e["slot"], kind))
                    arrow_alpha = min(1.0, (k + 1) / 4.0)
                    self.painter.draw_arrow_input_to_cell(
                        ax, e["i"], e["bucket"], e["slot"],
                        alpha=arrow_alpha,
                        color=(ACCENT_HIT if kind == "BUCKET_HIT"
                               else ACCENT_MISS))
                    self.painter.draw_output(self.output)
                    cap = (f"_mm256_cmpeq_epi64  ->  "
                           f"{'HIT at lane ' + str(e['slot']) if kind == 'BUCKET_HIT' else 'claim slot ' + str(e['slot'])}"
                           f"   counts[{e['slot']}] += {run_len}")
                    self.painter.draw_caption(
                        cap, color=(ACCENT_HIT if kind == "BUCKET_HIT"
                                    else ACCENT_MISS))
                    self.save(frames_dir)
            else:
                if idx % 4 != 0 and idx != len(bucket_events) - 1:
                    continue
                ax = self.painter.clear_figure()
                self.painter.draw_header(
                    "stage 2/4: hot loop  /  one AVX2 cmpeq per element")
                self.painter.draw_input(self.input, highlight_range=hl)
                self.painter.draw_hash_formula(ax, e["v"], e["bucket"],
                                               highlight=False)
                self.painter.draw_bucket_grid(
                    ax, self.buckets,
                    active=(e["bucket"], e["slot"], kind))
                self.painter.draw_output(self.output)
                self.painter.draw_caption(
                    f"i = {e['i']}   v = {e['v']}   "
                    f"->   bucket {e['bucket']}, lane {e['slot']}")
                self.save(frames_dir)

    def reconstruct_stage(self, frames_dir):
        collect_events = [e for e in self.events if e["op"] == "COLLECT"]
        pair_events = [e for e in self.events if e["op"] == "PAIR"]
        drained = set()
        live_pairs = []
        for e in collect_events:
            b = e["bucket"]; lane = e["slot"]
            live_pairs.append((e["key"], e["count"]))
            self.audio_events.append({"t": self._t(),
                                      "rank": self.rank.get(e["key"], 0),
                                      "kind": "COLLECT"})
            for k in range(5):
                ax = self.painter.clear_figure()
                self.painter.draw_header(
                    "stage 3/4: reconstruct  /  scan buckets, build dense list")
                self.painter.draw_input(self.input)
                self.painter.draw_bucket_grid(ax, self.buckets,
                                              active=(b, lane, "BUCKET_HIT"),
                                              drained=drained, dim=True)
                tape_positions, tape_y0 = self.painter.draw_dense_pairs_tape(
                    ax, live_pairs, current_idx=len(live_pairs) - 1,
                    slots=self.K)
                if tape_positions:
                    tx, ty = tape_positions[-1]
                    arrow_alpha = min(1.0, (k + 1) / 3.0)
                    self.painter.draw_cell_to_tape_arrow(
                        ax, b, lane, tx, ty + 0.13, alpha=arrow_alpha)
                self.painter.draw_output(self.output)
                self.painter.draw_caption(
                    f"collect  bucket {b}, lane {lane}  ->  "
                    f"pair (key {e['key']}, x{e['count']})",
                    color=ACCENT_HIT)
                self.save(frames_dir)
            drained.add((b, lane))

        sorted_pairs = [(e["key"], e["count"]) for e in pair_events]
        for _ in range(20):
            ax = self.painter.clear_figure()
            self.painter.draw_header(
                "stage 3/4: reconstruct  /  sort pairs by key")
            self.painter.draw_input(self.input)
            self.painter.draw_dense_pairs_tape(ax, sorted_pairs,
                                                slots=self.K)
            self.painter.draw_output(self.output)
            self.painter.draw_caption(
                f"K' = {len(sorted_pairs)} pairs sorted by key   "
                f"->   ready for fill_n", color=ACCENT_HIT)
            self.save(frames_dir)
        self.dense_pairs = sorted_pairs

    def emit_stage(self, frames_dir):
        sweep = 0
        for idx, (k, c) in enumerate(self.dense_pairs):
            for j in range(c):
                self.output[sweep + j] = k
            self.audio_events.append({"t": self._t(),
                                      "rank": self.rank.get(k, 0),
                                      "kind": "EMIT"})
            for f in range(8):
                ax = self.painter.clear_figure()
                self.painter.draw_header(
                    "stage 4/4: emit  /  fill_n monotonic write")
                self.painter.draw_input(self.input)
                positions, _ = self.painter.draw_dense_pairs_tape(
                    ax, self.dense_pairs, current_idx=idx, slots=self.K)
                hl = list(range(sweep, sweep + c))
                self.painter.draw_output(self.output, highlight_range=hl)
                if positions:
                    px, py = positions[idx]
                    color = color_to_hex(self.painter.color(k))
                    arrow_alpha = min(1.0, (f + 1) / 5.0)
                    self.painter.draw_pair_to_output_arrow(
                        ax, px, py, sweep, c, color, alpha=arrow_alpha)
                self.painter.draw_caption(
                    f"fill_n(out + {sweep}, {c}, {k})", color=ACCENT_EMIT)
                self.save(frames_dir)
            sweep += c

    def final_screen(self, frames_dir):
        for k in range(45):
            self.painter.clear_figure()
            self.painter.draw_header("done")
            self.painter.draw_input(self.input, label="input  (random)")
            self.painter.draw_output(self.output, label="output  (sorted)")
            alpha = min(1.0, k / 12.0)
            self.fig.text(0.5, 0.51, "256 elements sorted",
                          color=FG, fontsize=20, fontweight="bold",
                          family="monospace", ha="center", va="center",
                          alpha=alpha)
            self.fig.text(0.5, 0.45,
                          "O(N + K log K)   /   one SIMD cmpeq per element",
                          color=ACCENT_HIT, fontsize=12, family="monospace",
                          ha="center", va="center", alpha=alpha)
            self.fig.text(0.5, 0.40,
                          "github.com/kexibq-official/cafs-lib",
                          color=MUTED, fontsize=10, family="monospace",
                          ha="center", va="center", alpha=alpha)
            self.save(frames_dir)

    def render(self, out_dir):
        out = Path(out_dir)
        frames_dir = out / "frames"
        frames_dir.mkdir(parents=True, exist_ok=True)

        self.title_screen(frames_dir)
        self.setup_screen(frames_dir)
        self.sample_stage(frames_dir)
        self.hot_loop_stage(frames_dir)
        self.reconstruct_stage(frames_dir)
        self.emit_stage(frames_dir)
        self.final_screen(frames_dir)

        plt.close(self.fig)
        return {
            "frame_count": self.frame_count,
            "duration_s": self.frame_count / self.fps,
            "audio_events": self.audio_events,
        }


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--events", default="events.json")
    p.add_argument("--out-dir", default="render")
    p.add_argument("--fps", type=int, default=30)
    args = p.parse_args()

    events = load(args.events)
    movie = Movie(events, fps=args.fps)
    info = movie.render(args.out_dir)

    print(f"frames:   {info['frame_count']}")
    print(f"duration: {info['duration_s']:.2f}s")

    wav = Path(args.out_dir) / "audio.wav"
    synth_audio(info["audio_events"], info["duration_s"],
                movie.K, str(wav))
    print(f"wav:      {wav}")
