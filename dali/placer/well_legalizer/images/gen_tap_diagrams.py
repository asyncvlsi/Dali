#!/usr/bin/env python3
"""Generate the well-tap pattern schematics used by the well_legalizer README.

Run with no arguments to regenerate every PNG next to this script:

    python3 gen_tap_diagrams.py

Only the Python stdlib is used (zlib + struct), so there is no matplotlib/PIL
dependency. A small 5x7 bitmap font is included so each diagram can carry its own
legend.

What the diagrams depict
------------------------
Gridded rows do not share one height. Gridded cells are quantized in height
rather than width, every cell is aligned on its row's P/N boundary the way
abutting power rails require, and a row is exactly tall enough for the tallest
cell clustered into it -- so row heights and P/N boundary offsets are uneven.
Adjacent rows are flipped so like wells abut.

A well-tap cell is short: it cannot span a row, so it straddles the P/N boundary
to tie both wells, with its N+ well tie on the N-well side and its P+ substrate
tie on the P-well side. Because the tap is short, the rest of that tap column --
above it, below it, and across any untapped row -- is filled with the lighter
P+/N+ select tones. That fill is what WellGeometryBuilder generates, and it is
required: the select layer must stay continuous or the layout takes implant-area
and well-spacing DRC violations.
"""
import os
import struct
import zlib

# (p_well_units, n_well_units) per row; uneven on purpose.
ROW_SPECS = [(4, 6), (5, 5), (3, 6), (6, 4), (4, 6)]
UNIT = 11
MARGIN = 22
W = 900
TAP_W = 30
# A tap is short: it reaches this many units either side of the P/N boundary.
TAP_UP, TAP_DOWN = 2, 2
CELL_W = 40
CELL_GAP = 9
CELL_SHAPES = [(0, 0), (0, 2), (1, 0), (2, 1), (0, 1),
               (1, 3), (0, 0), (2, 0), (1, 1), (0, 3)]

WHITE = (255, 255, 255)
BG_BAD = (255, 244, 244)
NWELL = (205, 227, 247)
PWELL = (250, 236, 190)
CELL_EDGE = (108, 118, 134)
PPLUS = (206, 110, 74)
NPLUS = (58, 150, 138)
PPLUS_FILL = (240, 192, 172)
NPLUS_FILL = (178, 217, 212)
TAP_EDGE = (58, 46, 40)
ROW_EDGE = (176, 180, 190)
TEXT = (48, 52, 60)

GLYPH_W, GLYPH_H = 5, 7

# 5x7 glyphs, written a row at a time so widths cannot drift.
_GLYPH_ROWS = {
    " ": [".....", ".....", ".....", ".....", ".....", ".....", "....."],
    "-": [".....", ".....", ".....", "#####", ".....", ".....", "....."],
    "+": [".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."],
    "/": ["....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...."],
    "N": ["#...#", "##..#", "#.#.#", "#.#.#", "#..##", "#...#", "#...#"],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "a": [".....", ".....", ".###.", "....#", ".####", "#...#", ".####"],
    "c": [".....", ".....", ".###.", "#....", "#....", "#....", ".###."],
    "d": ["....#", "....#", ".####", "#...#", "#...#", "#...#", ".####"],
    "e": [".....", ".....", ".###.", "#...#", "#####", "#....", ".###."],
    "f": ["..##.", ".#..#", ".#...", "###..", ".#...", ".#...", ".#..."],
    "g": [".....", ".....", ".####", "#...#", ".####", "....#", ".###."],
    "i": ["..#..", ".....", ".##..", "..#..", "..#..", "..#..", ".###."],
    "l": [".##..", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    "m": [".....", ".....", "##.#.", "#.#.#", "#.#.#", "#.#.#", "#.#.#"],
    "n": [".....", ".....", "#.##.", "##..#", "#...#", "#...#", "#...#"],
    "p": [".....", ".....", "####.", "#...#", "####.", "#....", "#...."],
    "r": [".....", ".....", "#.##.", "##..#", "#....", "#....", "#...."],
    "s": [".....", ".....", ".####", "#....", ".###.", "....#", "####."],
    "t": ["..#..", "..#..", ".###.", "..#..", "..#..", "..#.#", "...#."],
    "w": [".....", ".....", "#...#", "#...#", "#.#.#", "#.#.#", ".#.#."],
}
FONT = {ch: "".join(rows) for ch, rows in _GLYPH_ROWS.items()}
assert all(len(v) == GLYPH_W * GLYPH_H for v in FONT.values())

LEGEND_LABELS = ["N-well", "P-well", "gridded cell", "well tap cell",
                 "N+ implant fill", "P+ implant fill"]
_missing = {c for label in LEGEND_LABELS for c in label} - set(FONT)
assert not _missing, f"legend needs glyphs for {sorted(_missing)}"


class Canvas:
    def __init__(self, w, h, bg):
        self.w, self.h = w, h
        self.buf = bytearray(bg * (w * h))

    def px(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h:
            i = (y * self.w + x) * 3
            self.buf[i:i + 3] = bytes(c)

    def rect(self, x0, y0, x1, y1, c):
        for y in range(int(round(y0)), int(round(y1))):
            for x in range(int(round(x0)), int(round(x1))):
                self.px(x, y, c)

    def border(self, x0, y0, x1, y1, c, t=1):
        x0, y0, x1, y1 = (int(round(v)) for v in (x0, y0, x1, y1))
        for k in range(t):
            for x in range(x0, x1):
                self.px(x, y0 + k, c); self.px(x, y1 - 1 - k, c)
            for y in range(y0, y1):
                self.px(x0 + k, y, c); self.px(x1 - 1 - k, y, c)

    def text(self, x, y, s, c=TEXT, scale=2):
        """Draw `s` with the built-in 5x7 font; unknown chars render blank."""
        cx = x
        for ch in s:
            bits = FONT.get(ch, FONT[" "])
            for row in range(GLYPH_H):
                for col in range(GLYPH_W):
                    idx = row * GLYPH_W + col
                    if idx < len(bits) and bits[idx] == "#":
                        self.rect(cx + col * scale, y + row * scale,
                                  cx + (col + 1) * scale,
                                  y + (row + 1) * scale, c)
            cx += (GLYPH_W + 1) * scale
        return cx

    def write(self, path):
        raw = bytearray()
        stride = self.w * 3
        for y in range(self.h):
            raw.append(0)
            raw += self.buf[y * stride:(y + 1) * stride]

        def chunk(tag, data):
            return (struct.pack(">I", len(data)) + tag + data +
                    struct.pack(">I", zlib.crc32(tag + data) & 0xffffffff))

        ihdr = struct.pack(">IIBBBBB", self.w, self.h, 8, 2, 0, 0, 0)
        png = (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
               chunk(b"IDAT", zlib.compress(bytes(raw), 6)) +
               chunk(b"IEND", b""))
        with open(path, "wb") as f:
            f.write(png)
        return os.path.getsize(path)


def row_layout():
    """Return [(y0, y1, bands, spec)]; bands is [(ylo, yhi, is_pwell)]."""
    rows = []
    y = MARGIN
    for r, (p_units, n_units) in enumerate(ROW_SPECS):
        top_units, top_is_p = ((n_units, False) if r % 2 == 0
                               else (p_units, True))
        bot_units, bot_is_p = ((p_units, True) if r % 2 == 0
                               else (n_units, False))
        top_h, bot_h = top_units * UNIT, bot_units * UNIT
        bands = [(y, y + top_h, top_is_p),
                 (y + top_h, y + top_h + bot_h, bot_is_p)]
        rows.append((y, y + top_h + bot_h, bands, (p_units, n_units)))
        y += top_h + bot_h
    return rows


LEGEND_H = 78
CANVAS_H = row_layout()[-1][1] + MARGIN + LEGEND_H


def draw_cells(c, x0, x1, r, bands, spec):
    """Gridded cells: one width, varying height, aligned on the P/N boundary."""
    p_row, n_row = spec
    y_pn = bands[0][1]
    x, i = x0 + 6, 0
    while x + CELL_W <= x1 - 6:
        dp, dn = CELL_SHAPES[i % len(CELL_SHAPES)]
        p_cell, n_cell = max(1, p_row - dp), max(1, n_row - dn)
        up, down = ((n_cell, p_cell) if r % 2 == 0 else (p_cell, n_cell))
        c.border(x, y_pn - up * UNIT, x + CELL_W, y_pn + down * UNIT, CELL_EDGE)
        x += CELL_W + CELL_GAP
        i += 1


def draw_tap_column(c, x0, x1, bands, tapped):
    """Fill a tap column with select, then stamp the short tap cell if present.

    The select fill covers the whole column so the P+/N+ layer stays continuous;
    the tap, being short, only occupies a band around the P/N boundary.
    """
    for (blo, bhi, is_p) in bands:
        c.rect(x0, blo, x1, bhi, PPLUS_FILL if is_p else NPLUS_FILL)
    if not tapped:
        return
    y_pn = bands[0][1]
    top_is_p = bands[0][2]
    c.rect(x0, y_pn - TAP_UP * UNIT, x1, y_pn, PPLUS if top_is_p else NPLUS)
    c.rect(x0, y_pn, x1, y_pn + TAP_DOWN * UNIT, NPLUS if top_is_p else PPLUS)
    c.border(x0, y_pn - TAP_UP * UNIT, x1, y_pn + TAP_DOWN * UNIT,
             TAP_EDGE, t=2)


def draw_legend(c, y):
    """Two rows of swatch + label pairs describing every colour used."""
    sw, sh = 26, 14
    col_x = [MARGIN, MARGIN + 300, MARGIN + 600]

    def chip(x, yy, kind):
        if kind == "nwell":
            c.rect(x, yy, x + sw, yy + sh, NWELL)
        elif kind == "pwell":
            c.rect(x, yy, x + sw, yy + sh, PWELL)
        elif kind == "cell":
            c.border(x, yy, x + sw, yy + sh, CELL_EDGE)
        elif kind == "tap":
            c.rect(x, yy, x + sw, yy + sh // 2, NPLUS)
            c.rect(x, yy + sh // 2, x + sw, yy + sh, PPLUS)
            c.border(x, yy, x + sw, yy + sh, TAP_EDGE, t=2)
        elif kind == "nfill":
            c.rect(x, yy, x + sw, yy + sh, NPLUS_FILL)
        elif kind == "pfill":
            c.rect(x, yy, x + sw, yy + sh, PPLUS_FILL)

    kinds = ["nwell", "pwell", "cell", "tap", "nfill", "pfill"]
    entries = list(zip(kinds, LEGEND_LABELS))
    for i, (kind, label) in enumerate(entries):
        x = col_x[i % 3]
        yy = y + (i // 3) * 30
        chip(x, yy, kind)
        c.text(x + sw + 10, yy + 1, label)


def make(path, columns_fn, is_tapped_fn, bad=False):
    c = Canvas(W, CANVAS_H, BG_BAD if bad else WHITE)
    left, right = MARGIN, W - MARGIN
    rows = row_layout()
    for r, (y0, y1, bands, spec) in enumerate(rows):
        for (blo, bhi, is_p) in bands:
            c.rect(left, blo, right, bhi, PWELL if is_p else NWELL)
        c.border(left, y0, right, y1, ROW_EDGE)

        cols = columns_fn(r, left, right)
        bounds = [left] + [v for iv in cols for v in iv] + [right]
        for i in range(0, len(bounds), 2):
            draw_cells(c, bounds[i], bounds[i + 1], r, bands, spec)
        for (cx0, cx1) in cols:
            draw_tap_column(c, cx0, cx1, bands, is_tapped_fn(r))

    draw_legend(c, rows[-1][1] + 20)
    return c.write(path)


def ends(r, left, right):
    return [(left, left + TAP_W), (right - TAP_W, right)]


def mid(r, left, right):
    cx = (left + right) / 2
    return [(cx - TAP_W / 2, cx + TAP_W / 2)]


def checker(r, left, right):
    span = right - left - TAP_W
    cx = left + (span * 0.26 if r % 2 == 0 else span * 0.74)
    return [(cx, cx + TAP_W)]


ALWAYS = lambda r: True
ALTERNATE = lambda r: r % 2 == 0

here = os.path.dirname(os.path.abspath(__file__))
outputs = {
    "row-end_every-row.png": (ends, ALWAYS, False),
    "row-end_every-other-row.png": (ends, ALTERNATE, False),
    "row-mid_every-row_unsupported.png": (mid, ALWAYS, True),
    "row-mid_every-other-row_unsupported.png": (mid, ALTERNATE, True),
    "checkerboard_unsupported.png": (checker, ALWAYS, True),
}

if __name__ == "__main__":
    for name, (cols_fn, tapped_fn, bad) in sorted(outputs.items()):
        size = make(os.path.join(here, name), cols_fn, tapped_fn, bad)
        print(f"{size:7d} B  {name}")
