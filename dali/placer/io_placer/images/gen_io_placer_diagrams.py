#!/usr/bin/env python3
"""Generate the SVG schematics used by the I/O placer README.

Run this script with no arguments to regenerate both SVG files next to it:

    python3 gen_io_placer_diagrams.py

The diagrams deliberately use SVG primitives and the Python standard library
only. They document concepts and supported workflows; they are not placement
results or claims of foundry DRC compliance.
"""

from pathlib import Path
from xml.sax.saxutils import escape


WIDTH = 1200
HEIGHT = 600
INK = "#24313a"
MUTED = "#61717d"
GRID = "#dce4e8"
PANEL = "#f7f9fa"
BLUE = "#247ba0"
GREEN = "#2a9d6f"
MAGENTA = "#bd3f78"
AMBER = "#d88918"
RED = "#c54b4b"


class Svg:
    """Small SVG writer for deterministic documentation diagrams."""

    def __init__(self, title):
        self.items = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{WIDTH}" '
            f'height="{HEIGHT}" viewBox="0 0 {WIDTH} {HEIGHT}">',
            f"<title>{escape(title)}</title>",
            "<style>"
            "text{font-family:Arial,Helvetica,sans-serif;fill:#24313a}"
            ".title{font-size:28px;font-weight:700}"
            ".heading{font-size:20px;font-weight:700}"
            ".label{font-size:16px}"
            ".small{font-size:14px;fill:#61717d}"
            "</style>",
            f'<rect width="{WIDTH}" height="{HEIGHT}" fill="#ffffff"/>',
        ]

    def rect(self, x, y, width, height, fill, stroke=INK, radius=0, dash=None):
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        self.items.append(
            f'<rect x="{x}" y="{y}" width="{width}" height="{height}" '
            f'rx="{radius}" fill="{fill}" stroke="{stroke}" '
            f'stroke-width="2"{dash_attr}/>'
        )

    def line(self, x1, y1, x2, y2, stroke=INK, width=2, dash=None):
        dash_attr = f' stroke-dasharray="{dash}"' if dash else ""
        self.items.append(
            f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" '
            f'stroke="{stroke}" stroke-width="{width}"{dash_attr}/>'
        )

    def circle(self, x, y, radius, fill, stroke=INK):
        self.items.append(
            f'<circle cx="{x}" cy="{y}" r="{radius}" fill="{fill}" '
            f'stroke="{stroke}" stroke-width="1.5"/>'
        )

    def polygon(self, points, fill):
        value = " ".join(f"{x},{y}" for x, y in points)
        self.items.append(f'<polygon points="{value}" fill="{fill}"/>')

    def text(self, x, y, value, css_class="label", anchor="start"):
        self.items.append(
            f'<text x="{x}" y="{y}" class="{css_class}" '
            f'text-anchor="{anchor}">{escape(value)}</text>'
        )

    def arrow(self, x1, y1, x2, y2, color=INK):
        self.line(x1, y1, x2, y2, color, 3)
        self.polygon([(x2, y2), (x2 - 12, y2 - 7), (x2 - 12, y2 + 7)], color)

    def write(self, path):
        self.items.append("</svg>")
        path.write_text("\n".join(self.items) + "\n", encoding="utf-8")


def draw_pin(svg, x, y, color, vertical=True):
    """Draw one pin rectangle centered at x/y."""
    width, height = (10, 28) if vertical else (28, 10)
    svg.rect(x - width / 2, y - height / 2, width, height, color, color, 1)


def placement_modes(path):
    """Show exact, perimeter, grouped, mirrored, and area-array placement."""
    svg = Svg("Dali I/O placement modes")
    svg.text(50, 50, "I/O placement modes", "title")
    svg.text(
        50,
        80,
        "Manual coordinates are authoritative; automatic modes provide starting points.",
        "small",
    )

    die_x, die_y, die_w, die_h = 80, 125, 710, 390
    svg.rect(die_x, die_y, die_w, die_h, "#fbfcfc", INK, 2)
    for x in range(die_x + 70, die_x + die_w, 70):
        svg.line(x, die_y, x, die_y + die_h, GRID, 1)
    for y in range(die_y + 65, die_y + die_h, 65):
        svg.line(die_x, y, die_x + die_w, y, GRID, 1)

    for y in (175, 245, 370, 455):
        draw_pin(svg, die_x, y, BLUE)
    for x in (180, 330, 620, 720):
        draw_pin(svg, x, die_y + die_h, BLUE, False)
    for y in (280, 310, 340):
        draw_pin(svg, die_x + die_w, y, GREEN)

    draw_pin(svg, 215, 245, MAGENTA)
    svg.text(240, 240, "exact manual pin", "label")
    svg.text(240, 262, "(interior or boundary)", "small")

    draw_pin(svg, 240, 360, AMBER)
    draw_pin(svg, 630, 360, AMBER)
    svg.line(240, 360, 630, 360, AMBER, 2, "7 6")
    svg.line(435, 335, 435, 385, AMBER, 1, "4 4")
    svg.text(435, 408, "mirrored pair", "small", "middle")

    for row in range(3):
        for col in range(4):
            svg.circle(435 + col * 65, 170 + row * 50, 6, BLUE, BLUE)
    svg.text(532, 305, "area array", "small", "middle")

    legend_x = 845
    svg.text(legend_x, 145, "Legend", "heading")
    entries = [
        (BLUE, "automatic pin"),
        (MAGENTA, "manual fixed pin"),
        (GREEN, "ordered edge group"),
        (AMBER, "mirrored pins"),
    ]
    for index, (color, label) in enumerate(entries):
        y = 190 + index * 48
        draw_pin(svg, legend_x + 8, y - 5, color)
        svg.text(legend_x + 35, y, label, "label")

    svg.text(legend_x, 405, "Pins may be mixed:", "heading")
    svg.text(legend_x, 438, "1. Fix critical pins", "label")
    svg.text(legend_x, 468, "2. Place groups/mirrors", "label")
    svg.text(legend_x, 498, "3. Auto-place the remainder", "label")
    svg.write(path)


def command_flow(path):
    """Show the reproducible command loop and external signoff boundary."""
    svg = Svg("Dali I/O placement command and signoff flow")
    svg.text(50, 50, "Command-driven I/O signoff", "title")
    svg.text(
        50,
        80,
        "Recipes, interactive mode, and embedding use the same command processor.",
        "small",
    )

    boxes = [
        (50, 155, 190, 115, "#eef5f8", "Placed design", "LEF + DEF"),
        (300, 155, 220, 115, "#eef7f3", "Dali commands", ".dali / terminal / API"),
        (580, 130, 265, 165, "#fff7e9", "Manual review loop", "show, place, move, check"),
        (905, 155, 235, 115, "#eef5f8", "Export", "DEF / PhyDB"),
    ]
    for x, y, width, height, fill, heading, detail in boxes:
        svg.rect(x, y, width, height, fill, INK, 5)
        svg.text(x + width / 2, y + 47, heading, "heading", "middle")
        svg.text(x + width / 2, y + 78, detail, "small", "middle")

    svg.arrow(240, 212, 290, 212)
    svg.arrow(520, 212, 570, 212)
    svg.arrow(845, 212, 895, 212)

    svg.rect(300, 365, 545, 125, PANEL, MUTED, 4)
    svg.text(325, 400, "Optional automatic starting point", "heading")
    svg.text(325, 435, "perimeter  |  edge constraints  |  groups", "label")
    svg.text(325, 466, "mirror  |  regular area array", "label")
    svg.arrow(570, 365, 570, 305, MUTED)

    svg.rect(905, 350, 235, 155, "#fff0f0", RED, 4)
    svg.text(1022, 390, "External signoff", "heading", "middle")
    svg.text(1022, 425, "foundry DRC", "label", "middle")
    svg.text(1022, 453, "package + routing", "label", "middle")
    svg.text(1022, 481, "pin access", "label", "middle")
    svg.arrow(1022, 280, 1022, 340, RED)

    svg.text(50, 550, "Dali check-io: geometry, containment, overlap, scalar spacing", "small")
    svg.text(
        1150,
        550,
        "Foundry signoff remains authoritative",
        "small",
        "end",
    )
    svg.write(path)


def main():
    output_dir = Path(__file__).resolve().parent
    outputs = {
        "io_placement_modes.svg": placement_modes,
        "io_command_flow.svg": command_flow,
    }
    for name, generate in outputs.items():
        path = output_dir / name
        generate(path)
        print(f"{path.stat().st_size:7d} B  {name}")


if __name__ == "__main__":
    main()
