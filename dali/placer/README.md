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

## Latest runtime and placement quality

Measured at `9878f537` with one run per design. Standard-cell runs used
default row repacking. Set `OMP_NUM_THREADS` to the same count as
`-num_threads`. The gridded designs are DaliTest cases 1–3.

End-to-end time includes input parsing and export. Gridded legalization
time excludes detailed placement.

| Design | Threads | Global placement (s) | Legalization (s) | Detailed placement (s) | End to end (s) | Final weighted HPWL (µm) |
|---|---:|---:|---:|---:|---:|---:|
| Adaptec1 | 8 | 23.93 | 2.31 | 9.42 | 46.80 | 80,933,657.50 |
| Adaptec2 | 8 | 29.39 | 4.60 | 20.24 | 67.23 | 90,201,375.00 |
| Adaptec3 | 8 | 58.80 | 3.90 | 16.46 | 102.22 | 210,016,950.50 |
| Adaptec4 | 8 | 41.22 | 5.30 | 20.62 | 91.12 | 186,048,692.00 |
| Big Blue 1 | 8 | 37.96 | 8.68 | 41.57 | 102.20 | 98,732,183.50 |
| Big Blue 2 | 8 | 51.09 | 3.92 | 18.28 | 99.70 | 149,850,922.50 |
| Big Blue 3 | 8 | 122.39 | 27.41 | 128.18 | 328.31 | 358,243,837.50 |
| Big Blue 4 | 8 | 251.88 | 49.71 | 242.56 | 663.33 | 804,606,037.50 |
| Gridded case 1 | 1 | 4.49 | 0.10 | 5.66 | 10.60 | 271,650.45 |
| Gridded case 2 | 1 | 5.09 | 0.29 | 3.02 | 8.78 | 150,154.39 |
| Gridded case 3 | 1 | 73.04 | 3.99 | 23.10 | 102.37 | 2,931,825.60 |
| ISPD 2019 test3 | 1 | 1.47 | 0.02 | 0.21 | 2.16 | 95,976.78 |

Big Blue 3 had 11,940 overlapping pairs in the geometry check. An older
placement of the same design also had overlaps. The other seven ISPD 2005
designs passed geometry checks, and all three gridded cases reported zero
legality violations. The ISPD 2019 I/O fixture has pre-existing geometry
violations.

<details>
<summary>Historical ISPD 2005 results at afc54ea4</summary>

Measured at commit `afc54ea4` on a MacBook Pro with an Apple M1 Max CPU, 10
cores (8 performance and 2 efficiency), and 32 GB unified memory, running macOS
26.5.2 on AC power in Automatic power mode. These are single-run engineering
measurements intended to show representative behavior, not publication-quality
performance measurements.

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
legalization, and detailed placement. Legalization and detailed placement are currently serial. Their
8-thread-run measurements are shown once.

| design | global, 1T (s) | global, 8T (s) | global speedup | legalization (s) | detailed placement (s) | total, 1T (s) | total, 8T (s) | total speedup |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| adaptec1 | 64.4 | 29.5 | 2.18× | 14.3 | 38.1 | 120.0 | 81.9 | 1.47× |
| adaptec2 | 79.1 | 37.9 | 2.09× | 28.7 | 115.4 | 222.1 | 181.9 | 1.22× |
| adaptec3 | 154.2 | 83.1 | 1.86× | 61.5 | 46.8 | 261.1 | 191.5 | 1.36× |
| adaptec4 | 116.0 | 59.9 | 1.94× | 92.8 | 75.0 | 291.1 | 227.7 | 1.28× |
| bigblue1 | 102.1 | 44.3 | 2.31× | 40.3 | 279.1 | 411.7 | 363.7 | 1.13× |
| bigblue2 | 139.7 | 77.6 | 1.80× | 85.7 | 76.0 | 302.6 | 239.3 | 1.26× |
| bigblue3 | 320.7 | 176.2 | 1.82× | 244.6 | 844.2 | 1331.9 | 1265.0 | 1.05× |
| bigblue4 | 730.2 | 421.7 | 1.73× | 595.6 | 1811.6 | 3102.1 | 2828.9 | 1.10× |

Across the complete suite, global-placement time falls from 1706.4 seconds to
930.2 seconds, a 1.83x aggregate speedup. Complete placement time falls from
6042.6 seconds to 5379.8 seconds, a 1.12x speedup, because standard-cell
legalization and detailed placement are currently serial. These two stages are
the main remaining parallelization opportunity, especially on bigblue3 and
bigblue4.

</details>

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
