# Dali
## Gridded Cell Placement Flow

Dali is a standard-cell placer for gridded-cell, well-aware technologies. It runs
analytical global placement, then legalizes cells into gridded rows with
N/P-well-aware clustering, row orientation, row-location and detailed-placement
optimization, and finally completes each row with well taps and end caps before
writing a legal DEF. Every stage can be stepped through in a live Qt GUI or
exported as visualization snapshots.

### Recommended compilation toolchain
  * Ubuntu >= 18.04
  * GNU Compiler Collection (GCC), version >= 4.8.5
  * CMake, version >= 3.9.6
  * GNU Make

### Pre-requisite
  * [ACT](https://github.com/asyncvlsi/act)
  * Si2 LEF/DEF parser, a mirror can be found [here](https://github.com/asyncvlsi/lefdef)
  * [PhyDB](https://github.com/asyncvlsi/phyDB)
  * OpenMP (for MacOS user, `libomp` from Homebrew will work)
  * Qt 6 Widgets is optional and enables Dali's live placement GUI. Install it
    with `brew install qt` on macOS or `sudo apt install qt6-base-dev` on
    Ubuntu 22.04 and newer. CMake enables the GUI automatically when Qt is
    available.
  * GoogleTest is optional. If CMake cannot find it, tests in `tests/common` are
    skipped while the rest of the build remains available. On Ubuntu/Debian,
    install it with `sudo apt install libgtest-dev`.
  * [OR-Tools](https://developers.google.com/optimization/install/cpp) 9.15.x
    is optional. It enables the experimental CP-SAT legalization backend when
    CMake can find a compatible C++ package. On macOS, install it with
    `brew install or-tools pkgconf`. On Ubuntu, install the official 9.15 C++
    binary distribution or build and install it from source, then set
    `ORTOOLS_ROOT` if it is outside a standard system prefix.
  
### Clone repo and compile
    $ git clone --recursive https://github.com/asyncvlsi/Dali.git
    $ cd Dali/
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make
    $ make install
this will create a binary __dali__ in folder _Dali/bin_. 
The default installation destination is `$ACT_HOME`. 
One can use the following command to specify the installation destination and install this package:

    $ cmake .. -DCMAKE_INSTALL_PREFIX=path/to/installation

OR-Tools detection defaults to `AUTO`. Use `-DDALI_OR_TOOLS=ON` to require a
compatible installation or `-DDALI_OR_TOOLS=OFF` to build without it. For a
custom Ubuntu installation:

    $ ORTOOLS_ROOT=/path/to/or-tools-9.15 cmake ..

Qt GUI detection defaults to `AUTO`, so the normal `cmake ..` command enables
the GUI automatically when Qt 6 Widgets is installed. Use `-DDALI_GUI=ON` only
when configuration should fail if Qt is unavailable, or `-DDALI_GUI=OFF` to
force a non-GUI build:

    $ cmake .. -DDALI_GUI=ON

### Running Dali

A placement run needs a LEF and a DEF. Providing a `-cell` file triggers the
gridded well-placement flow:

    $ dali \
        -lef design.lef \
        -def design.def \
        -cell design.cell \
        -target_density 0.7 \
        -output_name placed.def

Commonly used options:
  * `-o`/`-output_name <name>.def` — output DEF (default `dali_out.def`)
  * `-d`/`-target_density <0..1>` — target placement density
  * `-well_legalization_mode <strict/scavenge>` — gridded well legalization mode
  * `-well_tap_pattern <row-end/row-end-every-other>` — well-tap
    arrangement; see [well-tap patterns](dali/placer/well_legalizer/README.md)
  * `-metrics_file <file.json>` — per-stage HPWL and runtime metrics
  * `-net_hpwl_file <file.tsv>` — final per-net weighted HPWL

Run `dali` with no arguments to print the full option list.

### Production configurations

The option list is long because most flags exist to isolate one stage during
development. Two combinations are the ones actually used for real runs; start
from whichever matches the design and change one thing at a time.

**Gridded well placement.** This is the flow a `-cell` file selects:

    $ dali \
        -lef design.lef \
        -def design.def \
        -cell design.cell \
        -well_legalization_mode strict \
        -target_density 0.65 \
        -global_max_iterations 40 \
        -enable_gridded_upper_bound_refiner \
        -enable_gridded_upper_bound_balancing \
        -gridded_legalization_feedback y_row_transactional_coherent \
        -enable_gridded_row_y_optimization \
        -enable_gridded_detailed_placement \
        -enable_gridded_detailed_relocation \
        -enable_end_cap_cell \
        -metrics_file dali_metrics.json

What each group does:

  * `-well_legalization_mode strict` refuses to spill into space the stripe
    planner did not assign; `scavenge` lets the last column use leftover space,
    which packs better but weakens the well guarantees.
  * Gridded row width is omitted above, so the legalizer derives it from the
    technology's MaxPlugDist as `2 * max_unplug_length` — every transistor has
    to sit within MaxPlugDist of a compatible-well tap, and that is what bounds
    the width. The derived width is not yet as good as a tuned one: on a test
    design it costs roughly 5% final HPWL. Set `-max_row_width <um>` when that
    matters and a good value for the design is known; closing the gap so the
    derived width is competitive is open work.
  * The two `upper_bound` flags roughly legalize every global-placement
    iteration and rebalance stripes that fail, so global placement optimizes
    against a legal-ish picture instead of an idealized one.
  * `-gridded_legalization_feedback` selects how much of that rough-legal result
    is fed back. `y_row_transactional_coherent` is the most conservative mode
    that still helps; `none` disables feedback entirely.
  * The three `gridded_detailed` / `row_y` flags are the post-legalization
    optimizers: row-group Y placement, then relocation into row whitespace,
    then swaps and local reordering.
  * `-enable_end_cap_cell` adds end caps at row ends. Add `-enable_filler_cell`
    when the implant layers must be continuous across the whole row.

**Standard cells.** No `-cell` file, and no well legalization:

    $ dali \
        -lef design.lef \
        -def design.def \
        -is_standard_cell \
        -global_initializer density_aware \
        -target_density 1 \
        -metrics_file dali_metrics.json

`-target_density 1` suits designs that are already close to fully utilized,
where asking for spare whitespace only distorts the result; lower it when the
design has room to spread.

Useful additions to either configuration: `-num_threads <n>` for the OpenMP
paths, `-v 3` with `-disable_log_prefix` and `-log_file_name` for a readable
log, and `-net_hpwl_file` for a per-net HPWL breakdown.

### Visualizing the placement flow

Dali can step through the placement live in a Qt GUI, showing a snapshot at
every stage: global-placement iterations, gridded stripe partitioning, component
clustering, orientation, row-location and detailed-placement steps, and physical
completion (well taps and end caps). The GUI requires a Qt-enabled build:

    $ dali ... -gui_debug -gui_pause every_snapshot

It pauses at each checkpoint so intermediate states — including the gridded row
structure, wells, taps, and end caps — can be inspected, and plots per-stage
HPWL curves for the stages the run will execute.

Two independent displacement toggles, both off by default, overlay an arrow per
movable cell drawn from where that cell sat in an earlier placement to where it
sits now. Enable either or both:

  * *Displacement vs global* (red) — total movement since global placement
    finished, i.e. what legalization and detailed placement cost overall. This
    overlay is empty while global placement is still running, since there is no
    global placement result to compare against yet.
  * *Displacement vs previous* (blue) — movement contributed by the current
    stage alone

Arrows are drawn to scale, so late detailed-placement stages that move cells by
a fraction of a row are close to invisible at fit-to-view zoom. Zoom in to
inspect them.

### Run tests
After configuring and building from the `build/` directory, run:

    $ make test-unit

This runs the fast GoogleTest-based unit tests in `tests/application`,
`tests/common`, and `tests/circuit`.

To include integration tests such as the I/O placer benchmarks, run:

    $ make test-integration

To run every test registered with CTest, run:

    $ make test-all

The equivalent raw CTest commands are:

    $ ctest --output-on-failure -L unit
    $ ctest --output-on-failure -L integration
    $ ctest --output-on-failure

GoogleTest is optional. If CMake cannot find it, GoogleTest-based unit tests are
skipped while integration tests and the rest of the build remain available. If
the test executables have not been built yet, build first:

    $ make

### 3rd Party Module List
  * Eigen: sparse matrix iterative linear solver
  
### Miscellaneous
  * Eigen gives different results for different C++ compilers, because floating point addition is not necessarily associative
  * g++ in MacOS is an alias of clang instead of GCC
  * 32bit and 64bit version g++ also give different results
