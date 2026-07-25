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

Measured with the standard-cell configuration above, four threads on an Apple
M1 Max. HPWL in units of 1e6 um, after global placement, after legalization, and
after detailed placement.

| design | cells | GP iters | GP | legal | final | runtime |
|---|---|---|---|---|---|---|
| adaptec1 | 211 K | 49 | 79.05 | 81.60 | 80.87 | 134 s |
| adaptec2 | 255 K | 44 | 88.95 | 91.33 | 90.54 | 247 s |
| adaptec3 | 452 K | 45 | 200.80 | 205.52 | 203.50 | 280 s |
| adaptec4 | 496 K | 45 | 182.69 | 188.96 | 186.84 | 315 s |
| bigblue1 | 278 K | 45 | 95.84 | 98.75 | 98.49 | 411 s |
| bigblue2 | 558 K | 40 | 145.71 | 151.27 | 148.89 | 332 s |
| bigblue3 | 1.10 M | 39 | 338.71 | 348.84 | 345.70 | 1444 s |

Legalization costs a little wirelength and detailed placement recovers part of
it. Global placement converges in 39 to 49 iterations on every design, without
growing with design size.

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
