#!/usr/bin/env python3
"""Render Dali placement rectangle tables without MATLAB.

The input format is the existing Dali visualization table:

  x0 x1 x2 x3 y0 y1 y2 y3 [r g b]

Each row describes one rectangle using four patch vertices. Optional RGB values
are used as face colors. The renderer draws directly into a raster image, which
keeps large standard-cell designs practical to plot.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
from typing import Optional, TYPE_CHECKING

if TYPE_CHECKING:
    import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render a Dali MATLAB-style rectangle table to an image."
    )
    parser.add_argument("input", type=Path, help="Dali rectangle table to render")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("placement.png"),
        help="Output image path",
    )
    parser.add_argument(
        "--pixels",
        type=int,
        default=2400,
        help="Longest output image dimension in pixels, default: 2400",
    )
    parser.add_argument(
        "--margin", type=int, default=20, help="Image margin in pixels"
    )
    parser.add_argument(
        "--edge",
        action="store_true",
        help="Draw rectangle outlines. This is slower for large designs.",
    )
    parser.add_argument(
        "--background",
        default="white",
        help="Background color name or #RRGGBB value, default: white",
    )
    parser.add_argument(
        "--max-rects",
        type=int,
        default=0,
        help="Draw at most this many rectangles using deterministic stride sampling.",
    )
    return parser.parse_args()


def load_rect_table(path: Path, max_rects: int) -> np.ndarray:
    import numpy as np

    if not path.is_file():
        raise FileNotFoundError(f"Input file does not exist: {path}")

    table = np.loadtxt(path, dtype=np.float32, ndmin=2)
    if table.size == 0:
        return np.empty((0, 0), dtype=np.float32)
    if table.shape[1] < 8:
        raise ValueError(
            f"{path} has {table.shape[1]} columns; expected at least 8"
        )

    if max_rects > 0 and table.shape[0] > max_rects:
        stride = int(np.ceil(table.shape[0] / max_rects))
        table = table[::stride]
    return table


def build_vertices(table: np.ndarray) -> np.ndarray:
    import numpy as np

    vertices = np.empty((table.shape[0], 4, 2), dtype=np.float32)
    vertices[:, :, 0] = table[:, 0:4]
    vertices[:, :, 1] = table[:, 4:8]
    return vertices


def build_colors(table: np.ndarray) -> Optional[np.ndarray]:
    import numpy as np

    if table.shape[1] < 11:
        return None
    return (np.clip(table[:, 8:11], 0.0, 1.0) * 255).astype(np.uint8)


def render(table: np.ndarray, args: argparse.Namespace) -> None:
    import numpy as np
    from PIL import Image, ImageColor, ImageDraw

    background = ImageColor.getrgb(args.background)
    if table.shape[0] == 0:
        image = Image.new("RGB", (args.pixels, args.pixels), background)
        image.save(args.output)
        return

    min_x = float(np.min(table[:, 0:4]))
    max_x = float(np.max(table[:, 0:4]))
    min_y = float(np.min(table[:, 4:8]))
    max_y = float(np.max(table[:, 4:8]))
    design_width = max(max_x - min_x, 1.0)
    design_height = max(max_y - min_y, 1.0)

    drawable_pixels = max(args.pixels - 2 * args.margin, 1)
    scale = drawable_pixels / max(design_width, design_height)
    width = int(np.ceil(design_width * scale)) + 2 * args.margin
    height = int(np.ceil(design_height * scale)) + 2 * args.margin

    x0 = np.floor((np.min(table[:, 0:4], axis=1) - min_x) * scale).astype(int)
    x1 = np.ceil((np.max(table[:, 0:4], axis=1) - min_x) * scale).astype(int)
    y0 = np.floor((max_y - np.max(table[:, 4:8], axis=1)) * scale).astype(int)
    y1 = np.ceil((max_y - np.min(table[:, 4:8], axis=1)) * scale).astype(int)

    x0 += args.margin
    x1 += args.margin
    y0 += args.margin
    y1 += args.margin
    colors = build_colors(table)

    image = Image.new("RGB", (width, height), background)
    draw = ImageDraw.Draw(image)
    outline = (0, 0, 0) if args.edge else None
    default_color = (0, 255, 255)
    for i in range(table.shape[0]):
        fill = tuple(int(v) for v in colors[i]) if colors is not None else default_color
        draw.rectangle((int(x0[i]), int(y0[i]), int(x1[i]), int(y1[i])),
                       fill=fill, outline=outline)

    image.save(args.output)


def main() -> int:
    args = parse_args()
    try:
        table = load_rect_table(args.input, args.max_rects)
        render(table, args)
    except ImportError as exc:
        print(
            "Missing Python plotting dependency. Install numpy and pillow.",
            file=sys.stderr,
        )
        print(exc, file=sys.stderr)
        return 2
    except (OSError, ValueError) as exc:
        print(exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
