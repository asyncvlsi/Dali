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
  * OR-Tools is optional and only enables an experimental CP-SAT legalization
    backend that no production flow uses yet. Nothing needs to be installed for
    a normal build; see [placer options](dali/placer/README.md) if you want it.
  
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

Qt GUI detection defaults to `AUTO`, so the normal `cmake ..` command enables
the GUI automatically when Qt 6 Widgets is installed. Use `-DDALI_GUI=ON` only
when configuration should fail if Qt is unavailable, or `-DDALI_GUI=OFF` to
force a non-GUI build:

    $ cmake .. -DDALI_GUI=ON

### Running Dali

A placement run needs a LEF and a DEF, supplied on the command line or by a
`.dali` recipe. Providing a `-cell` file triggers the gridded well-placement
flow:

    $ dali \
        -lef design.lef \
        -def design.def \
        -cell design.cell \
        -target_density 0.7 \
        -output_name placed

Commonly used options:
  * `-o`/`-output_name <base>` — output base name; Dali appends `.def`
    (default `dali_out`)
  * `-d`/`-target_density <0..1>` — target placement density
  * `-well_legalization_mode <strict/scavenge>` — gridded well legalization mode
  * `-well_tap_pattern <row-end/row-end-every-other>` — well-tap
    arrangement; see [well-tap patterns](dali/placer/well_legalizer/README.md)
  * `-metrics_file <file.json>` — per-stage HPWL and runtime metrics
  * `-net_hpwl_file <file.tsv>` — final per-net weighted HPWL

Run `dali` with no arguments to print the full option list.

### Dali command recipes

The normal command above remains the default. A `.dali` recipe can own the
complete flow, including inputs and output (the longer `-command_file` spelling
is also accepted):

    $ dali -script placement_flow.dali

Paths in a recipe are resolved relative to that recipe, not the shell's current
directory:

    # dali-script 1
    read-lef "design.lef"
    read-def "design.def"
    # read-cell "design.cell"

    set target_density 0.70
    set num_threads 4
    set disable_io_place true
    set output_name "results/placed"

    show settings
    run placement

    place-io -constraint clock top
    place-io M4
    show-io clock
    move-io clock 120.0 400.0 N
    check-io

    write-def "results/signed_off"

Commands are executed in order and the file stops at the first error. Blank
lines, `#` comments, quoted arguments, escaped characters, and backslash line
continuations are supported. `write-def` exports immediately; without it, the
standalone application exports the final design using `output_name`. Supported
settings include:

  * `output_name`, `target_density`, `num_threads`, `io_metal_layer`,
    `net_ignore_threshold`
  * `global_min_iterations`, `global_max_iterations`,
    `global_initializer`
  * `detailed_max_rounds`, `detailed_max_move_candidates`
  * `well_legalization_mode`, `standard_cell_legalizer_cost`
  * `disable_global_place`, `disable_legalization`,
    `disable_detailed_place`, `disable_io_place`
  * `disable_welltap`, `disable_cell_flip`, `is_standard_cell`,
    `enable_filler_cell`, `enable_end_cap_cell`

Boolean values accept `true`/`false`, `on`/`off`, or `1`/`0`. The legacy
`place-design <density> [threads]` and `global-place <density> [threads]`
commands remain available, but new recipes should configure settings separately
and use `run placement`.

Manual I/O signoff commands use microns for locations:

  * `show-io [pin]` reports status, location, orientation, layer, and shape.
  * `move-io <pin> <x> <y> [orientation]` preserves the pin shape and layer,
    fixes it at the new location, and optionally changes its orientation.
  * `unfix-io <pin>` releases a pin for a later `place-io -auto-place`.
  * `check-io` checks that pins are placed, have geometry, remain inside the
    die, do not overlap, and satisfy basic same-layer scalar spacing.

`check-io` is an early feedback tool, not a replacement for process-specific
foundry signoff DRC.

### Interactive mode

Use `-interactive` to open the input design without implicitly running
placement:

    $ dali -lef design.lef -def placed.def -interactive

The prompt accepts the same commands as `.dali` files:

    dali> show-io
    dali> check-io
    dali> move-io clock 120.0 400.0 N
    dali> check-io
    dali> quit

Command failures are reported without closing the session. `history` prints
commands entered during the current session, `source <file.dali>` runs a
recipe, and `quit` or `exit` finishes the session and exports the current
design.

To run a placement recipe and then keep the design open for manual signoff,
combine both modes:

    $ dali \
        -script placement_flow.dali \
        -interactive

See the [Dali command language guide](dali/command/README.md) for recipe
syntax, all execution modes, I/O signoff commands, and the public embedding
API.

The same recipe API is designed for a future `interact` adapter:

    dali:init 3
    dali:source "placement_flow.dali"
    dali:export-phydb
    dali:close

`interact` does not provide `dali:source` yet; Dali already exposes
`ExecuteCommand`, `ExecuteCommandLine`, and `RunCommandFile` so that adapter can
remain small.

### Production configurations

Two flag combinations are the ones used for real runs — the gridded well flow a
`-cell` file selects, and the standard-cell flow `-is_standard_cell` selects.
Both, with an explanation of what each flag buys and when to change it, are in
[placer options](dali/placer/README.md).

### Visualizing the placement flow

Dali can step through a placement live in a Qt window, pausing at every stage so
intermediate state can be inspected, with per-stage HPWL curves and displacement
overlays:

    $ dali ... -gui_debug -gui_pause every_snapshot

Requires a Qt-enabled build. See [the live placement GUI](dali/gui/README.md).

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
