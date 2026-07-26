# Placer options

Reference for the flags that shape a placement run. Run `dali` with no arguments
for the complete list; this page covers the ones worth understanding before
changing them.

For well-tap arrangement specifically, see
[well-tap patterns](well_legalizer/README.md). For how the placer is put
together, see [architecture notes](ARCHITECTURE.md).

## Choosing a flow

A `-cell` file selects the gridded well-placement flow. `-is_standard_cell`
selects the standard-cell flow, which skips well legalization entirely.

## Gridded well placement

    $ dali \
        -lef design.lef \
        -def design.def \
        -cell design.cell \
        -well_legalization_mode strict \
        -target_density 0.65 \
        -enable_gridded_upper_bound_refiner \
        -enable_gridded_upper_bound_balancing \
        -gridded_legalization_feedback y_row_transactional_coherent \
        -enable_gridded_row_y_optimization \
        -enable_gridded_detailed_placement \
        -enable_gridded_detailed_relocation \
        -enable_end_cap_cell \
        -metrics_file dali_metrics.json

### Region and well structure

  * `-well_legalization_mode <strict/scavenge>` — `scavenge` packs better,
    `strict` keeps each column an independent well region.

  * `-max_row_width <um>` — force a gridded row width. Omitted above: the
    legalizer derives one from the technology, and forcing a width that the
    technology does not allow produces an illegal placement.

### Steering global placement with legalization

These feed legalization results back into global placement, so it optimizes
against something close to legal. The refiner enables the other two.

  * `-enable_gridded_upper_bound_refiner` — roughly legalize every iteration.
  * `-enable_gridded_upper_bound_balancing` — rebalance stripes that overflow.
  * `-gridded_legalization_feedback <mode>` — how much of that result to keep.
    `none` discards it, `full` takes it wholesale, the `y_*` modes are
    increasingly cautious middle grounds.

### After legalization

  * `-enable_gridded_row_y_optimization` — move row groups toward the Y their
    nets want.
  * `-enable_gridded_detailed_relocation` — move cells into row whitespace.
  * `-enable_gridded_detailed_placement` — global swap, vertical swap, and
    local reorder. Normally enabled together with relocation.

### Physical completion

  * `-enable_end_cap_cell` — insert end caps at row ends.
  * `-enable_filler_cell` — insert fillers, for continuous implant layers.
  * `-disable_welltap` — skip well-tap insertion.

## Standard cells

    $ dali \
        -lef design.lef \
        -def design.def \
        -is_standard_cell \
        -global_initializer density_aware \
        -target_density 1 \
        -metrics_file dali_metrics.json

`-target_density 1` suits designs already close to fully utilized; lower it when
the design has room to spread.

## I/O pin placement

Unplaced I/O pins are placed by the `place-io` command, after component
placement so pin positions can follow the nets that reach them. Pins already
marked `FIXED`/`PLACED` in the input keep their locations.

See the dedicated [I/O placement README](io_placer/README.md) for the manual
signoff workflow, diagrams, full command reference, and explicit limitations.

  * `place-io <metal>` — auto-place every unplaced pin on the four perimeter
    edges (wire-bond model), all on `<metal>`. Long form: `place-io -c -m left
    <metal> right <metal> bottom <metal> top <metal>`.
  * `place-io -place <pin> <metal> <lx> <ly> <ux> <uy> <x> <y> <orient>` — fix
    one named pin at an explicit location (microns), anywhere including the die
    interior; later auto-placement leaves it untouched.
  * `place-io -constraint <pin | dir:input|output|inout> <left|right|bottom|top>`
    — force a named pin, or every pin of a signal direction, onto a chosen edge
    instead of the automatically selected closest one. A per-pin constraint wins
    over a per-direction one.
  * `place-io -area <metal> <rows> <cols>` — area-array (flip-chip) placement:
    put every unplaced pin on an interior `rows x cols` lattice on `<metal>`,
    each pin assigned to the free site nearest its net's bounding-box center.
    `rows*cols` must cover the pin count.
  * `place-io -group <metal> <left|right|bottom|top> <pin> [pin ...]` — place
    the named pins as one adjacent run at legal pitch. Command order maps to
    increasing y on a vertical edge and increasing x on a horizontal edge. The
    run uses the free interval nearest its nets and is fixed before later
    automatic placement.
  * `place-io -mirror <pin> <reference_pin> <x|y>` — place and fix `<pin>` at
    the symmetric location of an already placed reference. `x` reflects the x
    coordinate across the vertical die centerline; `y` reflects the y coordinate
    across the horizontal centerline. Layer, shape, and reflected orientation
    come from the reference pin.

Manual pins, groups, and mirrored pins can be configured first, followed by
perimeter auto-placement or an area array for all remaining pins. Fixed
boundary pins reserve their occupied intervals so later automatic pins do not
overlap them.

## Iteration control

Global placement stops on its own when it stops improving, so these rarely need
attention.

  * `-global_max_iterations <n>` — upper limit, default 100.
  * `-global_min_iterations <n>` — floor, default 10.

## Standard-cell results on ISPD 2005

Measured at commit `afc54ea4` on a MacBook Pro with an Apple M1 Max CPU, 10
cores (8 performance and 2 efficiency), and 32 GB unified memory, running macOS
26.5.2 on AC power in Automatic power mode. These are single-run engineering
measurements intended to show representative behavior, not publication-quality
performance measurements.

The runs use `density_aware` initialization and continue through global
placement, legalization, and detailed placement:

    $ dali \
        -lef design.lef \
        -def design.def \
        -is_standard_cell \
        -global_initializer density_aware \
        -target_density 1 \
        -num_threads <1-or-8> \
        -disable_io_place \
        -metrics_file dali_metrics.json

Weighted HPWL is reported in units of 1e6 um. The 1-thread and 8-thread runs
produce identical HPWL for every benchmark.

| design | cells | global HPWL | legal HPWL | final HPWL |
|---|---:|---:|---:|---:|
| adaptec1 | 211 K | 78.69 | 81.97 | 80.93 |
| adaptec2 | 255 K | 88.60 | 91.04 | 90.20 |
| adaptec3 | 452 K | 206.96 | 212.25 | 210.02 |
| adaptec4 | 496 K | 182.47 | 187.41 | 186.05 |
| bigblue1 | 278 K | 96.01 | 99.03 | 98.73 |
| bigblue2 | 558 K | 146.57 | 152.11 | 149.85 |
| bigblue3 | 1.10 M | 351.27 | 361.87 | 358.24 |
| bigblue4 | 2.18 M | 789.29 | 808.29 | 804.61 |

Runtime is Dali's internal wall time for each placement stage and excludes
LEF/DEF parsing and result export. `Total` is the sum of global placement,
legalization, and detailed placement.

| design | threads | global | legal | detailed | total | total speedup |
|---|---:|---:|---:|---:|---:|---:|
| adaptec1 | 1 | 64.4 s | 14.5 s | 41.2 s | 120.0 s | - |
| adaptec1 | 8 | 29.5 s | 14.3 s | 38.1 s | 81.9 s | 1.47x |
| adaptec2 | 1 | 79.1 s | 27.5 s | 115.4 s | 222.1 s | - |
| adaptec2 | 8 | 37.9 s | 28.7 s | 115.4 s | 181.9 s | 1.22x |
| adaptec3 | 1 | 154.2 s | 60.4 s | 46.4 s | 261.1 s | - |
| adaptec3 | 8 | 83.1 s | 61.5 s | 46.8 s | 191.5 s | 1.36x |
| adaptec4 | 1 | 116.0 s | 93.2 s | 81.9 s | 291.1 s | - |
| adaptec4 | 8 | 59.9 s | 92.8 s | 75.0 s | 227.7 s | 1.28x |
| bigblue1 | 1 | 102.1 s | 41.3 s | 268.3 s | 411.7 s | - |
| bigblue1 | 8 | 44.3 s | 40.3 s | 279.1 s | 363.7 s | 1.13x |
| bigblue2 | 1 | 139.7 s | 85.6 s | 77.3 s | 302.6 s | - |
| bigblue2 | 8 | 77.6 s | 85.7 s | 76.0 s | 239.3 s | 1.26x |
| bigblue3 | 1 | 320.7 s | 237.9 s | 773.4 s | 1331.9 s | - |
| bigblue3 | 8 | 176.2 s | 244.6 s | 844.2 s | 1265.0 s | 1.05x |
| bigblue4 | 1 | 730.2 s | 589.6 s | 1782.3 s | 3102.1 s | - |
| bigblue4 | 8 | 421.7 s | 595.6 s | 1811.6 s | 2828.9 s | 1.10x |

Across the complete suite, global-placement time falls from 1706.4 seconds to
930.2 seconds, a 1.83x aggregate speedup. Complete placement time falls from
6042.6 seconds to 5379.8 seconds, a 1.12x speedup, because standard-cell
legalization and detailed placement are currently serial. These two stages are
the main remaining parallelization opportunity, especially on bigblue3 and
bigblue4.

## Experimental: CP-SAT legalization

Research switches, off by default and not used by any production flow: they buy
small placement gains for a lot of runtime. Exposed through
`-enable_ortools_row_optimization` and the `-*_exact_gridded_*` families.

They need OR-Tools 9.15.x at build time. Detection defaults to `AUTO`, so a
normal `cmake ..` picks it up if present. To install: `brew install or-tools
pkgconf` on macOS, or the official 9.15 C++ binary distribution on Ubuntu. If it
lives outside a standard prefix:

    $ ORTOOLS_ROOT=/path/to/or-tools-9.15 cmake ..

Use `-DDALI_OR_TOOLS=ON` to make configuration fail when it is missing, or
`-DDALI_OR_TOOLS=OFF` to ignore an installed copy.

## Useful in either flow

  * `-num_threads <n>` — threads for the OpenMP paths
  * `-v 3` with `-disable_log_prefix` and `-log_file_name <file>` — readable log
  * `-metrics_file <file.json>` — per-stage HPWL and runtime
  * `-net_hpwl_file <file.tsv>` — final per-net weighted HPWL
