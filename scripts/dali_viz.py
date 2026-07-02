#!/usr/bin/env python3
"""Render Dali placement snapshots to browser-zoomable SVG files.

The input is a Dali visualization directory produced by:

  dali ... -visualization_dir dali_viz

Each snapshot folder is rendered independently. Components are drawn as cell
rectangles, and selected top weighted-HPWL nets are drawn as translucent
bounding boxes with optional pin/star overlays. The renderer intentionally uses
only the Python standard library so it can run wherever Dali is installed.
"""

from __future__ import annotations

import argparse
import html
import json
from pathlib import Path
from typing import Any, Iterable


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Render Dali visualization snapshots to SVG."
    )
    parser.add_argument(
        "visualization_dir",
        type=Path,
        help="Directory containing manifest.json and snapshots/",
    )
    parser.add_argument(
        "-s",
        "--snapshot",
        default="final",
        help=(
            "Snapshot index, id, or 'all'. Default: final. Examples: 0, "
            "global_placement.final, all"
        ),
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output file for a single snapshot, or the interactive HTML viewer.",
    )
    parser.add_argument(
        "--format",
        choices=("svg", "html"),
        default="svg",
        help="Output format, default: svg.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("dali_viz_svg"),
        help="Output directory when rendering --snapshot all.",
    )
    parser.add_argument(
        "--max-components",
        type=int,
        default=0,
        help="Draw at most this many components using deterministic stride sampling.",
    )
    parser.add_argument(
        "--top-net-count",
        type=int,
        default=80,
        help="Maximum top nets to draw, default: 80.",
    )
    parser.add_argument(
        "--no-pins",
        action="store_true",
        help="Do not draw top-net pins and star lines.",
    )
    parser.add_argument(
        "--heatmap",
        action="store_true",
        help="Accepted for compatibility; bounding-box heatmaps are no longer drawn.",
    )
    parser.add_argument(
        "--margin",
        type=float,
        default=20.0,
        help="SVG margin in design units, default: 20.",
    )
    return parser.parse_args()


def load_json(path: Path) -> Any:
    with path.open(encoding="utf-8") as json_file:
        return json.load(json_file)


def escape(value: object) -> str:
    return html.escape(str(value), quote=True)


def format_number(value: float) -> str:
    return f"{value:.6g}"


def read_manifest(visualization_dir: Path) -> dict[str, Any]:
    manifest_path = visualization_dir / "manifest.json"
    if not manifest_path.is_file():
        raise FileNotFoundError(f"missing manifest: {manifest_path}")
    return load_json(manifest_path)


def select_snapshot_records(
    manifest: dict[str, Any], snapshot_selector: str
) -> list[dict[str, Any]]:
    snapshots = manifest.get("snapshots", [])
    if snapshot_selector == "all":
        return list(snapshots)

    for record in snapshots:
        if str(record.get("index")) == snapshot_selector:
            return [record]
        if record.get("id") == snapshot_selector:
            return [record]

    if snapshot_selector == "final" and snapshots:
        return [snapshots[-1]]

    available = ", ".join(str(record.get("id")) for record in snapshots)
    raise ValueError(
        f"snapshot '{snapshot_selector}' not found; available snapshots: {available}"
    )


def snapshot_dir(visualization_dir: Path, record: dict[str, Any]) -> Path:
    return visualization_dir / str(record["path"])


def load_snapshot_payloads(
    visualization_dir: Path, record: dict[str, Any]
) -> tuple[
    dict[str, Any],
    list[dict[str, Any]],
    list[dict[str, Any]],
    list[dict[str, Any]],
]:
    base = snapshot_dir(visualization_dir, record)
    metadata = load_json(base / "metadata.json")
    components = load_json(base / "components.json")
    top_nets = load_json(base / "top_net_pins.json")
    bottom_net_path = base / "bottom_net_pins.json"
    bottom_nets = load_json(bottom_net_path) if bottom_net_path.is_file() else []
    return metadata, components, top_nets, bottom_nets


def component_bounds(components: Iterable[dict[str, Any]]) -> tuple[float, float, float, float]:
    lx = float("inf")
    ly = float("inf")
    ux = float("-inf")
    uy = float("-inf")
    for component in components:
        x = float(component["x"])
        y = float(component["y"])
        w = float(component["w"])
        h = float(component["h"])
        lx = min(lx, x)
        ly = min(ly, y)
        ux = max(ux, x + w)
        uy = max(uy, y + h)
    if lx == float("inf"):
        return 0.0, 0.0, 1.0, 1.0
    return lx, ly, ux, uy


def draw_components(
    components: list[dict[str, Any]], max_components: int
) -> list[str]:
    if max_components > 0 and len(components) > max_components:
        stride = max(1, (len(components) + max_components - 1) // max_components)
        components = components[::stride]

    lines = ['  <g id="components">']
    for component in components:
        status = str(component.get("status", ""))
        if status == "FIXED":
            fill = "#4c566a"
        elif status == "PLACED":
            fill = "#88c0d0"
        else:
            fill = "#a3be8c"
        x = float(component["x"])
        y = float(component["y"])
        w = float(component["w"])
        h = float(component["h"])
        lines.append(
            "    "
            f'<rect x="{format_number(x)}" y="{format_number(y)}" '
            f'width="{format_number(w)}" height="{format_number(h)}" '
            f'fill="{fill}" opacity="0.78">'
            f"<title>{escape(component.get('name', ''))} "
            f"{escape(status)}</title></rect>"
        )
    lines.append("  </g>")
    return lines


def center_pin_for_net(net: dict[str, Any]) -> dict[str, Any] | None:
    pins = net.get("pins", [])
    if not pins:
        return None

    center_x = sum(float(pin["x"]) for pin in pins) / len(pins)
    center_y = sum(float(pin["y"]) for pin in pins) / len(pins)
    return min(
        pins,
        key=lambda pin: (float(pin["x"]) - center_x) ** 2
        + (float(pin["y"]) - center_y) ** 2,
    )


def draw_top_nets(
    top_nets: list[dict[str, Any]], top_net_count: int, draw_pins: bool
) -> list[str]:
    selected = top_nets[: max(0, top_net_count)]
    if not selected:
        return []

    lines = ['  <g id="top-nets">']
    for net in selected:
        center_pin = center_pin_for_net(net)
        if center_pin is None:
            continue
        cx = float(center_pin["x"])
        cy = float(center_pin["y"])
        hpwl = float(net.get("weighted_hpwl", 0.0))
        drawable_pins = [
            pin
            for pin in net.get("pins", [])
            if pin is not center_pin
            and (
                abs(float(pin["x"]) - cx) > 1e-9
                or abs(float(pin["y"]) - cy) > 1e-9
            )
        ]
        if not drawable_pins:
            continue

        for pin in drawable_pins:
            px = float(pin["x"])
            py = float(pin["y"])
            lines.append(
                "    "
                f'<line x1="{format_number(cx)}" y1="{format_number(cy)}" '
                f'x2="{format_number(px)}" y2="{format_number(py)}" '
                'stroke="#bf616a" stroke-width="0.35" '
                'vector-effect="non-scaling-stroke" opacity="0.62">'
                f"<title>{escape(net.get('name', ''))} weighted HPWL "
                f"{format_number(hpwl)}</title></line>"
            )
        if draw_pins:
            for pin in net.get("pins", []):
                px = float(pin["x"])
                py = float(pin["y"])
                fill = "#111827" if pin is center_pin else "#5e81ac"
                radius = 1.1 if pin is center_pin else 0.8
                lines.append(
                    "    "
                    f'<circle cx="{format_number(px)}" cy="{format_number(py)}" '
                    f'r="{format_number(radius)}" fill="{fill}" '
                    'vector-effect="non-scaling-stroke">'
                    f"<title>{escape(pin.get('component', ''))}/"
                    f"{escape(pin.get('pin', ''))}</title></circle>"
                )
    lines.append("  </g>")
    return lines


def render_snapshot(
    visualization_dir: Path,
    manifest: dict[str, Any],
    record: dict[str, Any],
    output_path: Path,
    args: argparse.Namespace,
) -> None:
    metadata, components, top_nets, _ = load_snapshot_payloads(
        visualization_dir, record
    )
    bounds = metadata.get("die_area") or {}
    if {"lx", "ly", "ux", "uy"}.issubset(bounds):
        lx = float(bounds["lx"])
        ly = float(bounds["ly"])
        ux = float(bounds["ux"])
        uy = float(bounds["uy"])
    else:
        lx, ly, ux, uy = component_bounds(components)

    margin = max(0.0, float(args.margin))
    view_lx = lx - margin
    view_ly = ly - margin
    view_w = max(ux - lx + 2 * margin, 1.0)
    view_h = max(uy - ly + 2 * margin, 1.0)

    title = (
        f"{manifest.get('design_name', 'unknown')} | "
        f"{record.get('id', record.get('index'))} | "
        f"weighted HPWL {format_number(float(record.get('weighted_hpwl', 0.0)))}"
    )

    lines = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        (
            f'<svg xmlns="http://www.w3.org/2000/svg" '
            f'viewBox="{format_number(view_lx)} {format_number(view_ly)} '
            f'{format_number(view_w)} {format_number(view_h)}">'
        ),
        "  <style>",
        "    text { font-family: Helvetica, Arial, sans-serif; }",
        "  </style>",
        f"  <title>{escape(title)}</title>",
        f'  <rect x="{format_number(view_lx)}" y="{format_number(view_ly)}" '
        f'width="{format_number(view_w)}" height="{format_number(view_h)}" '
        'fill="#f8f9fb" />',
        (
            f'  <g id="world" transform="translate(0 '
            f'{format_number(ly + uy)}) scale(1 -1)">'
        ),
        f'  <rect x="{format_number(lx)}" y="{format_number(ly)}" '
        f'width="{format_number(max(ux - lx, 0.0))}" '
        f'height="{format_number(max(uy - ly, 0.0))}" '
        'fill="none" stroke="#2e3440" stroke-width="0.7" '
        'vector-effect="non-scaling-stroke" />',
    ]
    lines.extend(draw_components(components, args.max_components))
    lines.extend(draw_top_nets(top_nets, args.top_net_count, not args.no_pins))
    lines.append("  </g>")
    lines.append("</svg>")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_interactive_snapshot(
    visualization_dir: Path, record: dict[str, Any]
) -> dict[str, Any]:
    metadata, components, top_nets, bottom_nets = load_snapshot_payloads(
        visualization_dir, record
    )
    return {
        "record": record,
        "metadata": metadata,
        "components": components,
        "topNets": top_nets,
        "bottomNets": bottom_nets,
    }


def render_interactive_html(
    visualization_dir: Path,
    manifest: dict[str, Any],
    records: list[dict[str, Any]],
    output_path: Path,
) -> None:
    snapshots = [
        build_interactive_snapshot(visualization_dir, record) for record in records
    ]
    payload = {
        "manifest": manifest,
        "snapshots": snapshots,
    }
    payload_json = json.dumps(payload, separators=(",", ":"))
    html_text = f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Dali Visualization</title>
  <style>
    html, body {{
      margin: 0;
      width: 100%;
      height: 100%;
      overflow: hidden;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      color: #1f2933;
      background: #f6f7f9;
    }}
    #toolbar {{
      position: fixed;
      z-index: 2;
      left: 12px;
      top: 12px;
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      align-items: center;
      padding: 8px 10px;
      border: 1px solid #d7dde5;
      background: rgba(255, 255, 255, 0.92);
      box-shadow: 0 8px 24px rgba(15, 23, 42, 0.12);
      font-size: 13px;
    }}
    #toolbar label {{
      display: inline-flex;
      gap: 4px;
      align-items: center;
      white-space: nowrap;
    }}
    #toolbar button, #toolbar select {{
      height: 28px;
      border: 1px solid #bcc6d3;
      background: #fff;
      color: #1f2933;
      border-radius: 4px;
    }}
    #toolbar button {{
      padding: 0 10px;
      cursor: pointer;
    }}
    #toolbar input[type="range"] {{
      width: 92px;
    }}
    .net-percent {{
      display: inline-block;
      min-width: 34px;
      text-align: right;
    }}
    #status {{
      position: fixed;
      z-index: 2;
      left: 12px;
      bottom: 12px;
      max-width: min(760px, calc(100vw - 24px));
      padding: 7px 10px;
      border: 1px solid #d7dde5;
      background: rgba(255, 255, 255, 0.92);
      font-size: 12px;
      box-shadow: 0 8px 24px rgba(15, 23, 42, 0.10);
    }}
    canvas {{
      display: block;
      width: 100vw;
      height: 100vh;
      cursor: grab;
    }}
    canvas.dragging {{
      cursor: grabbing;
    }}
  </style>
</head>
<body>
  <div id="toolbar">
    <label>Snapshot <select id="snapshot"></select></label>
    <label><input id="showCells" type="checkbox" checked> Cells</label>
    <label><input id="showNets" type="checkbox" checked> Nets</label>
    <label>Top <input id="topNetPercent" type="range" min="0" max="100" value="30"><span id="topNetPercentValue" class="net-percent">30%</span></label>
    <label>Bottom <input id="bottomNetPercent" type="range" min="0" max="100" value="0"><span id="bottomNetPercentValue" class="net-percent">0%</span></label>
    <label><input id="showPins" type="checkbox" checked> Pins</label>
    <button id="fit">Fit</button>
  </div>
  <canvas id="canvas"></canvas>
  <div id="status"></div>
  <script id="dali-data" type="application/json">{payload_json}</script>
  <script>
(() => {{
  const data = JSON.parse(document.getElementById("dali-data").textContent);
  const canvas = document.getElementById("canvas");
  const ctx = canvas.getContext("2d");
  const snapshotSelect = document.getElementById("snapshot");
  const showCells = document.getElementById("showCells");
  const showNets = document.getElementById("showNets");
  const topNetPercent = document.getElementById("topNetPercent");
  const bottomNetPercent = document.getElementById("bottomNetPercent");
  const topNetPercentValue = document.getElementById("topNetPercentValue");
  const bottomNetPercentValue = document.getElementById("bottomNetPercentValue");
  const showPins = document.getElementById("showPins");
  const fitButton = document.getElementById("fit");
  const status = document.getElementById("status");

  let snapshotIndex = 0;
  let scale = 1;
  let offsetX = 0;
  let offsetY = 0;
  let dragging = false;
  let dragX = 0;
  let dragY = 0;
  let mouseX = 0;
  let mouseY = 0;
  let drawStats = {{
    topSelected: 0,
    topAvailable: 0,
    topDrawn: 0,
    bottomSelected: 0,
    bottomAvailable: 0,
    bottomDrawn: 0
  }};

  for (let i = 0; i < data.snapshots.length; ++i) {{
    const record = data.snapshots[i].record;
    const option = document.createElement("option");
    option.value = String(i);
    option.textContent = `${{record.index}} ${{record.id}}`;
    snapshotSelect.appendChild(option);
  }}

  function currentSnapshot() {{
    return data.snapshots[snapshotIndex];
  }}

  function dieArea(snapshot) {{
    const area = snapshot.metadata.die_area;
    if (area) return area;
    let lx = Infinity, ly = Infinity, ux = -Infinity, uy = -Infinity;
    for (const cell of snapshot.components) {{
      lx = Math.min(lx, cell.x);
      ly = Math.min(ly, cell.y);
      ux = Math.max(ux, cell.x + cell.w);
      uy = Math.max(uy, cell.y + cell.h);
    }}
    if (!Number.isFinite(lx)) return {{lx: 0, ly: 0, ux: 1, uy: 1}};
    return {{lx, ly, ux, uy}};
  }}

  function resize() {{
    const ratio = window.devicePixelRatio || 1;
    canvas.width = Math.floor(window.innerWidth * ratio);
    canvas.height = Math.floor(window.innerHeight * ratio);
    ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
    draw();
  }}

  function fit() {{
    const area = dieArea(currentSnapshot());
    const margin = 32;
    const w = Math.max(area.ux - area.lx, 1);
    const h = Math.max(area.uy - area.ly, 1);
    scale = Math.min(
      (window.innerWidth - 2 * margin) / w,
      (window.innerHeight - 2 * margin) / h
    );
    offsetX = margin - area.lx * scale;
    offsetY = margin + area.uy * scale;
    draw();
  }}

  function worldToScreenX(x) {{
    return x * scale + offsetX;
  }}

  function worldToScreenY(y) {{
    return offsetY - y * scale;
  }}

  function screenToWorldX(x) {{
    return (x - offsetX) / scale;
  }}

  function screenToWorldY(y) {{
    return (offsetY - y) / scale;
  }}

  function rectVisible(x, y, w, h) {{
    const sx0 = worldToScreenX(x);
    const sx1 = worldToScreenX(x + w);
    const sy0 = worldToScreenY(y + h);
    const sy1 = worldToScreenY(y);
    return sx1 >= 0 && sx0 <= window.innerWidth && sy1 >= 0 && sy0 <= window.innerHeight;
  }}

  function drawDie(area) {{
    ctx.save();
    ctx.strokeStyle = "#1f2933";
    ctx.lineWidth = 1;
    ctx.strokeRect(
      worldToScreenX(area.lx),
      worldToScreenY(area.uy),
      Math.max((area.ux - area.lx) * scale, 1),
      Math.max((area.uy - area.ly) * scale, 1)
    );
    ctx.restore();
  }}

  function drawComponents(snapshot) {{
    if (!showCells.checked) return;
    const labelZoom = scale > 10;
    let labelCount = 0;
    ctx.save();
    for (const cell of snapshot.components) {{
      if (!rectVisible(cell.x, cell.y, cell.w, cell.h)) continue;
      const x = worldToScreenX(cell.x);
      const y = worldToScreenY(cell.y + cell.h);
      const w = Math.max(cell.w * scale, 0.6);
      const h = Math.max(cell.h * scale, 0.6);
      ctx.fillStyle = cell.status === "FIXED" ? "#4c566a" : "#88c0d0";
      ctx.globalAlpha = cell.status === "FIXED" ? 0.86 : 0.72;
      ctx.fillRect(x, y, w, h);
      if (scale > 6) {{
        ctx.globalAlpha = 0.38;
        ctx.strokeStyle = "#2e3440";
        ctx.lineWidth = 0.6;
        ctx.strokeRect(x, y, w, h);
      }}
      if (labelZoom && w > 36 && h > 10 && labelCount < 350) {{
        ctx.globalAlpha = 1;
        ctx.fillStyle = "#111827";
        ctx.font = "10px sans-serif";
        ctx.fillText(cell.name, x + 2, y + Math.min(h - 2, 11));
        labelCount += 1;
      }}
    }}
    ctx.restore();
  }}

  function selectedNets(snapshot) {{
    const selected = [];
    const topSource = snapshot.topNets || [];
    const bottomSource =
      snapshot.bottomNets && snapshot.bottomNets.length > 0
        ? snapshot.bottomNets
        : [...topSource].reverse();
    const topSelected = Math.ceil(topSource.length * Number(topNetPercent.value) / 100);
    const bottomSelected = Math.ceil(bottomSource.length * Number(bottomNetPercent.value) / 100);

    for (const net of topSource.slice(0, topSelected)) {{
      selected.push({{net, direction: "top"}});
    }}

    for (const net of bottomSource.slice(0, bottomSelected)) {{
      selected.push({{net, direction: "bottom"}});
    }}
    drawStats.topSelected = topSelected;
    drawStats.topAvailable = topSource.length;
    drawStats.topDrawn = 0;
    drawStats.bottomSelected = bottomSelected;
    drawStats.bottomAvailable = bottomSource.length;
    drawStats.bottomDrawn = 0;
    return selected;
  }}

  function centerPinForNet(net) {{
    const pins = net.pins || [];
    if (pins.length === 0) return null;
    const cx = pins.reduce((sum, pin) => sum + pin.x, 0) / pins.length;
    const cy = pins.reduce((sum, pin) => sum + pin.y, 0) / pins.length;
    let bestPin = pins[0];
    let bestDistance = Infinity;
    for (const pin of pins) {{
      const dx = pin.x - cx;
      const dy = pin.y - cy;
      const distance = dx * dx + dy * dy;
      if (distance < bestDistance) {{
        bestDistance = distance;
        bestPin = pin;
      }}
    }}
    return bestPin;
  }}

  function netVisible(net) {{
    const pins = net.pins || [];
    if (pins.length === 0) return false;
    let lx = Infinity, ly = Infinity, ux = -Infinity, uy = -Infinity;
    for (const pin of pins) {{
      lx = Math.min(lx, pin.x);
      ly = Math.min(ly, pin.y);
      ux = Math.max(ux, pin.x);
      uy = Math.max(uy, pin.y);
    }}
    return rectVisible(lx, ly, Math.max(ux - lx, 0.001), Math.max(uy - ly, 0.001));
  }}

  function drawableFanoutPins(net, centerPin) {{
    return (net.pins || []).filter(pin =>
      pin !== centerPin &&
      (Math.abs(pin.x - centerPin.x) > 1e-9 ||
       Math.abs(pin.y - centerPin.y) > 1e-9)
    );
  }}

  function drawTopNets(snapshot) {{
    if (!showNets.checked) return;
    const drawPins = showPins.checked && scale > 1.4;
    ctx.save();
    for (const selected of selectedNets(snapshot)) {{
      const net = selected.net;
      if (!netVisible(net)) continue;
      const centerPin = centerPinForNet(net);
      if (!centerPin) continue;
      const fanoutPins = drawableFanoutPins(net, centerPin);
      if (fanoutPins.length === 0) continue;
      const cx = worldToScreenX(centerPin.x);
      const cy = worldToScreenY(centerPin.y);
      const netColor = selected.direction === "top" ? "#bf616a" : "#2f855a";
      ctx.globalAlpha = 0.68;
      ctx.strokeStyle = netColor;
      ctx.lineWidth = scale > 10 ? 1.4 : 1;
      let drewSegment = false;
      for (const pin of fanoutPins) {{
        const px = worldToScreenX(pin.x);
        const py = worldToScreenY(pin.y);
        const dx = px - cx;
        const dy = py - cy;
        const screenLength = Math.hypot(dx, dy);
        ctx.beginPath();
        ctx.moveTo(cx, cy);
        ctx.lineTo(px, py);
        ctx.stroke();
        if (screenLength < 4) {{
          const mx = (cx + px) / 2;
          const my = (cy + py) / 2;
          ctx.globalAlpha = 0.86;
          ctx.fillStyle = netColor;
          ctx.beginPath();
          ctx.arc(mx, my, 2.2, 0, 2 * Math.PI);
          ctx.fill();
          ctx.globalAlpha = 0.68;
        }}
        drewSegment = true;
      }}
      if (drewSegment) {{
        if (selected.direction === "top") {{
          drawStats.topDrawn += 1;
        }} else {{
          drawStats.bottomDrawn += 1;
        }}
      }}
      if (drawPins) {{
        for (const pin of [centerPin, ...fanoutPins]) {{
          const px = worldToScreenX(pin.x);
          const py = worldToScreenY(pin.y);
          ctx.globalAlpha = 0.9;
          if (pin === centerPin) {{
            ctx.fillStyle = "#111827";
          }} else {{
            ctx.fillStyle = selected.direction === "top" ? "#5e81ac" : "#68a063";
          }}
          ctx.beginPath();
          ctx.arc(px, py, pin === centerPin ? 3.0 : 2.1, 0, 2 * Math.PI);
          ctx.fill();
        }}
      }}
    }}
    ctx.restore();
  }}

  function updateStatus(snapshot) {{
    const wx = screenToWorldX(mouseX);
    const wy = screenToWorldY(mouseY);
    const record = snapshot.record;
    const hpwl = Number(record.weighted_hpwl || 0).toLocaleString(undefined, {{
      maximumFractionDigits: 2
    }});
    const topPercent = Number(topNetPercent.value);
    const bottomPercent = Number(bottomNetPercent.value);
    status.textContent =
      `${{data.manifest.design_name || "unknown"}} | ${{record.id}} | ` +
      `weighted HPWL ${{hpwl}} | zoom ${{scale.toFixed(2)}} px/um | ` +
      `top ${{drawStats.topDrawn}}/${{drawStats.topSelected}} ` +
      `(${{topPercent}}% of ${{drawStats.topAvailable}}) | ` +
      `bottom ${{drawStats.bottomDrawn}}/${{drawStats.bottomSelected}} ` +
      `(${{bottomPercent}}% of ${{drawStats.bottomAvailable}}) | ` +
      `x ${{wx.toFixed(3)}} y ${{wy.toFixed(3)}}`;
  }}

  function draw() {{
    const snapshot = currentSnapshot();
    ctx.clearRect(0, 0, window.innerWidth, window.innerHeight);
    ctx.fillStyle = "#f6f7f9";
    ctx.fillRect(0, 0, window.innerWidth, window.innerHeight);
    drawDie(dieArea(snapshot));
    drawComponents(snapshot);
    drawTopNets(snapshot);
    updateStatus(snapshot);
  }}

  canvas.addEventListener("mousedown", event => {{
    dragging = true;
    dragX = event.clientX;
    dragY = event.clientY;
    canvas.classList.add("dragging");
  }});
  window.addEventListener("mouseup", () => {{
    dragging = false;
    canvas.classList.remove("dragging");
  }});
  window.addEventListener("mousemove", event => {{
    mouseX = event.clientX;
    mouseY = event.clientY;
    if (dragging) {{
      offsetX += event.clientX - dragX;
      offsetY += event.clientY - dragY;
      dragX = event.clientX;
      dragY = event.clientY;
    }}
    draw();
  }});
  canvas.addEventListener("wheel", event => {{
    event.preventDefault();
    const beforeX = screenToWorldX(event.clientX);
    const beforeY = screenToWorldY(event.clientY);
    const zoom = Math.exp(-event.deltaY * 0.0012);
    scale = Math.max(0.02, Math.min(600, scale * zoom));
    offsetX = event.clientX - beforeX * scale;
    offsetY = event.clientY + beforeY * scale;
    draw();
  }}, {{passive: false}});

  snapshotSelect.addEventListener("change", () => {{
    snapshotIndex = Number(snapshotSelect.value);
    fit();
  }});
  for (const control of [showCells, showNets, showPins]) {{
    control.addEventListener("change", draw);
  }}
  for (const slider of [topNetPercent, bottomNetPercent]) {{
    slider.addEventListener("input", () => {{
      topNetPercentValue.textContent = `${{topNetPercent.value}}%`;
      bottomNetPercentValue.textContent = `${{bottomNetPercent.value}}%`;
      draw();
    }});
  }}
  fitButton.addEventListener("click", fit);
  window.addEventListener("resize", resize);

  resize();
  fit();
}})();
  </script>
</body>
</html>
"""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(html_text, encoding="utf-8")


def output_path_for_record(output_dir: Path, record: dict[str, Any]) -> Path:
    index = int(record.get("index", 0))
    snapshot_id = str(record.get("id", f"snapshot_{index}")).replace("/", "_")
    return output_dir / f"{index:03d}_{snapshot_id}.svg"


def main() -> int:
    args = parse_args()
    visualization_dir = args.visualization_dir
    try:
        manifest = read_manifest(visualization_dir)
        records = select_snapshot_records(manifest, args.snapshot)
        if args.format == "html":
            output_path = args.output or Path("dali_viz.html")
            render_interactive_html(visualization_dir, manifest, records, output_path)
            print(f"Wrote {output_path}")
            return 0

        if args.snapshot == "all":
            for record in records:
                output_path = output_path_for_record(args.output_dir, record)
                render_snapshot(visualization_dir, manifest, record, output_path, args)
                print(f"Wrote {output_path}")
        else:
            output_path = args.output or output_path_for_record(Path("."), records[0])
            render_snapshot(visualization_dir, manifest, records[0], output_path, args)
            print(f"Wrote {output_path}")
    except (OSError, KeyError, TypeError, ValueError) as exc:
        print(f"dali_viz: {exc}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
