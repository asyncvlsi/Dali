# Placer options

Reference for the flags that shape a placement run. Run `dali` with no arguments
for the complete list; this page covers the ones worth understanding before
changing them.

For well-tap arrangement specifically, see
[well-tap patterns](well_legalizer/README.md).

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

  * `-well_legalization_mode <strict/scavenge>` — `strict` refuses to spill into
    space the stripe planner did not assign. `scavenge` lets the last column
    consume whatever is left over to the right boundary, which packs better but
    weakens the guarantee that each column is an independent well region.

  * `-max_row_width <um>` — caps gridded row width. Omitted above, so the
    legalizer derives it from the technology's MaxPlugDist as
    `2 * max_unplug_length`; every transistor must sit within MaxPlugDist of a
    compatible-well tap, and that is what bounds row width. The derived value is
    not yet as good as a tuned one — on the largest design measured it costs
    about 2.3% final HPWL, and it is the dominant term in the gap between a
    tuned and a fully automatic run — so set this when a good width for the
    design is known.

### Steering global placement with legalization

Global placement optimizes wirelength against a model that has no notion of
gridded rows. Left alone it converges to a picture legalization then has to
undo. These three flags feed legalization results back into it.

  * `-enable_gridded_upper_bound_refiner` — roughly legalize the placement each
    global-placement iteration, so the upper bound global placement optimizes
    against is a legal-ish placement rather than an idealized one. This is what
    makes the other two meaningful; without it there is no rough-legal result to
    balance or feed back.

  * `-enable_gridded_upper_bound_balancing` — when that rough legalization
    leaves a stripe over capacity, minimally rebalance across stripes instead of
    accepting the failure. Without it an overfull stripe simply reports failure
    for that iteration.

  * `-gridded_legalization_feedback <mode>` — how much of the rough-legal result
    is written back into the placement global placement continues from. `none`
    discards it; `full` accepts it wholesale; the `y_*` modes accept only
    vertical information, in increasing order of caution.
    `y_row_transactional_coherent` is the most conservative mode that still
    helps: row assignments are accepted as a transaction, and only when the
    result stays coherent.

### After legalization

  * `-enable_gridded_row_y_optimization` — shift whole legal row groups toward
    the Y region their nets want. Operates on rows, so it preserves legality by
    construction.

  * `-enable_gridded_detailed_relocation` — move cells into legal whitespace
    elsewhere in the rows before any swapping. Relocation opens up the space
    that makes swaps productive, so it is normally enabled alongside detailed
    placement rather than on its own.

  * `-enable_gridded_detailed_placement` — the swap-based optimizers: global
    swap, vertical swap, and local reorder within a row.

### Physical completion

  * `-enable_end_cap_cell` — insert end caps at gridded row ends.
  * `-enable_filler_cell` — insert filler cells, needed when implant layers must
    stay continuous across the whole row.
  * `-disable_welltap` — skip well-tap insertion entirely.

## Standard cells

    $ dali \
        -lef design.lef \
        -def design.def \
        -is_standard_cell \
        -global_initializer density_aware \
        -target_density 1 \
        -metrics_file dali_metrics.json

`-target_density 1` suits designs already close to fully utilized, where asking
for spare whitespace only distorts the result; lower it when the design has room
to spread.

## Iteration control

Global placement stops when its upper-bound wirelength stops trending downward,
so the iteration count adapts to the design and normally needs no attention.

  * `-global_max_iterations <n>` — upper limit, default 100. Reach for it only
    to bound runtime on a design that would otherwise run long.
  * `-global_min_iterations <n>` — floor below which it will not stop, default
    10.

## Experimental: CP-SAT legalization

Not used by any production flow yet. Dali can optionally solve bounded
legalization sub-problems exactly with OR-Tools CP-SAT, exposed through
`-enable_ortools_row_optimization`, the `-analyze_exact_gridded_*` /
`-solve_exact_gridded_*` family, and the exact stripe and boundary refiners.
These are research switches: they trade large amounts of runtime for small
placement gains and are off by default.

They are only available when the build found OR-Tools 9.15.x. Detection defaults
to `AUTO`, so a normal `cmake ..` picks it up if present and silently builds
without it otherwise. To install it: `brew install or-tools pkgconf` on macOS,
or the official 9.15 C++ binary distribution on Ubuntu. If it lives outside a
standard prefix, point CMake at it:

    $ ORTOOLS_ROOT=/path/to/or-tools-9.15 cmake ..

Use `-DDALI_OR_TOOLS=ON` to make configuration fail when it is missing, or
`-DDALI_OR_TOOLS=OFF` to ignore an installed copy.

## Useful in either flow

  * `-num_threads <n>` — threads for the OpenMP paths
  * `-v 3` with `-disable_log_prefix` and `-log_file_name <file>` — readable log
  * `-metrics_file <file.json>` — per-stage HPWL and runtime
  * `-net_hpwl_file <file.tsv>` — final per-net weighted HPWL
