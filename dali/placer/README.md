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

## Mixed-height standard cells

Add `-enable_mixed_height_standard_cells` to `-is_standard_cell` to legalize
mixed-height cells on the DEF rows. The rows must use a common site size and
horizontal N/FS orientations. Row fragments may have gaps and different X
origins. The flow subtracts fixed obstacles, reserves each tall cell in every
row it spans, and packs the single-height cells into the remaining segments.
Detailed placement keeps the tall reservations fixed and optimizes the
single-height cells. The flow validates the full footprints after each stage
and stops on a geometry or capacity failure. Use
`-force_mixed_height_standard_cells` with `-is_standard_cell` to run the
fixed-row flow on single-height designs as well.

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

Measured at commit `9878f537` on a MacBook Pro with an Apple M1 Max CPU, 10
cores (8 performance and 2 efficiency), and 32 GB unified memory, running macOS
27.2.1 on AC power. These are single-run engineering
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
legalization, and detailed placement. Legalization and detailed placement are
currently serial, so their 8-thread-run measurements are shown once.

| design | global, 1T (s) | global, 8T (s) | global speedup | legalization (s) | detailed placement (s) | total, 1T (s) | total, 8T (s) | total speedup |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| adaptec1 | 54.7 | 23.9 | 2.29× | 2.3 | 9.4 | 66.5 | 35.7 | 1.86× |
| adaptec2 | 66.0 | 29.4 | 2.25× | 4.6 | 20.2 | 90.5 | 54.2 | 1.67× |
| adaptec3 | 125.2 | 58.8 | 2.13× | 3.9 | 16.5 | 145.1 | 79.2 | 1.83× |
| adaptec4 | 94.4 | 41.2 | 2.29× | 5.3 | 20.6 | 120.2 | 67.1 | 1.79× |
| bigblue1 | 89.2 | 38.0 | 2.35× | 8.7 | 41.6 | 139.0 | 88.2 | 1.58× |
| bigblue2 | 106.0 | 51.1 | 2.08× | 3.9 | 18.3 | 128.1 | 73.3 | 1.75× |
| bigblue3 | 261.1 | 122.4 | 2.13× | 27.4 | 128.2 | 414.6 | 278.0 | 1.49× |
| bigblue4 | 526.1 | 251.9 | 2.09× | 49.7 | 242.6 | 809.5 | 544.1 | 1.49× |

Across the complete suite, global-placement time falls from 1322.8 seconds to
616.7 seconds, a 2.15x aggregate speedup. Complete placement time falls from
1913.4 seconds to 1219.8 seconds, a 1.57x speedup, because standard-cell
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
