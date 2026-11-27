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

Select `-legalizer mixed_height` with `-is_standard_cell`. The legalizer
imports horizontal N/FS DEF row fragments with a common site size and
contiguous sites. It subtracts fixed obstacles,
reserves each tall cell in every row it spans, and packs the single-height
cells into the remaining segments. The detailed placer keeps tall cells at
their legalized positions and optimizes the single-height cells using the DEF
site pitch and oriented LEF pins. Dali validates the full footprints after
each stage.

The ISPD 2005 benchmark commands select this flow for mixed-height and
single-height designs.

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

These runs use the mixed-height legalizer and local-search detailed placement.
Each timing is from one run.

```sh
dali -lef design.lef -def design.def \
  -is_standard_cell -legalizer mixed_height \
  -global_initializer density_aware -target_density 1 \
  -disable_io_place -num_threads 8 -metrics_file dali_metrics.json
```

Final weighted HPWL was identical at 1 and 8 threads for each design. All eight
final placements passed independent geometry checks at both thread counts.
HPWL is in millions of micrometres.

| Design | Cells | Global HPWL | Legalized HPWL | Final HPWL |
| --- | ---: | ---: | ---: | ---: |
| adaptec1 | 211 K | 78.691 | 81.195 | 79.857 |
| adaptec2 | 255 K | 88.599 | 90.738 | 89.362 |
| adaptec3 | 452 K | 206.955 | 211.767 | 208.923 |
| adaptec4 | 496 K | 182.474 | 186.955 | 184.409 |
| bigblue1 | 278 K | 96.012 | 98.614 | 97.229 |
| bigblue2 | 558 K | 146.569 | 151.511 | 148.336 |
| bigblue3 | 1.10 M | 351.271 | 364.130 | 356.833 |
| bigblue4 | 2.18 M | 789.293 | 806.467 | 796.354 |

Stage wall times are in seconds. Total sums global placement, legalization,
and detailed placement. Input parsing and export are outside these timings.
Legalization and detailed placement run serially.

| Design | 1T global | 1T legal | 1T detailed | 1T total | 8T global | 8T legal | 8T detailed | 8T total |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| adaptec1 | 54.7 | 1.9 | 6.6 | 63.1 | 24.0 | 1.9 | 6.6 | 32.5 |
| adaptec2 | 66.0 | 3.5 | 12.2 | 81.8 | 29.2 | 3.5 | 12.1 | 44.9 |
| adaptec3 | 125.0 | 3.3 | 15.4 | 143.8 | 56.0 | 3.3 | 15.5 | 74.8 |
| adaptec4 | 94.4 | 4.2 | 16.5 | 115.1 | 41.1 | 4.2 | 16.4 | 61.8 |
| bigblue1 | 89.4 | 6.4 | 22.5 | 118.3 | 37.5 | 6.5 | 22.7 | 66.6 |
| bigblue2 | 106.1 | 3.4 | 27.9 | 137.3 | 50.7 | 3.4 | 29.0 | 83.1 |
| bigblue3 | 260.9 | 13.3 | 51.2 | 325.4 | 122.1 | 13.5 | 51.5 | 187.1 |
| bigblue4 | 525.4 | 35.2 | 175.2 | 735.9 | 233.4 | 35.4 | 175.3 | 444.1 |

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
