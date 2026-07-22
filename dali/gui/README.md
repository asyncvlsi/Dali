# Live placement GUI

Dali can step through a placement live in a Qt window, showing a snapshot at
every stage: global-placement iterations, gridded stripe partitioning, component
clustering, orientation, row-location and detailed-placement steps, and physical
completion (well taps and end caps).

Requires a Qt-enabled build. Add the flags to any placement command:

    $ dali \
        -lef design.lef \
        -def design.def \
        -cell design.cell \
        -gui_debug \
        -gui_pause every_snapshot

  * `-gui_debug` — open the window and publish snapshots to it
  * `-gui_pause <every_snapshot/off>` — pause at each checkpoint, default
    `every_snapshot`. With `off` the run streams through without waiting.

Pausing is what makes intermediate state inspectable — the gridded row
structure, wells, taps, and end caps as they are built. The window also plots
per-stage HPWL curves, reserving one slot per stage the run will actually
execute rather than discovering them as they arrive.

## What it shows

Global placement finishes with cells spread by density, with no row structure —
each dot is one movable cell:

![After global placement](images/01_global_placement.png)

After legalization and physical completion the same design is organised into
stripe columns. Each column is its own well region, separated from its
neighbours by well spacing, and the pink and blue bands are the P and N wells of
the gridded rows:

![After legalization](images/02_legalized.png)

Zooming in resolves individual rows across a few stripe columns. The coloured
strips running up the column edges are the cells physical completion inserts:

![Stripe columns](images/03_stripe_columns.png)

Zoomed further, cells are drawn as rectangles rather than dots, with a corner
marker showing orientation. Rows abut so that adjacent rows share a well type,
and the orange row-end taps tie each row's wells:

![Cells and well taps](images/04_cells_and_taps.png)

At a column boundary the full result of physical completion is visible. Reading
outward from either column: cells, the orange well taps, then the green end caps
that terminate the row, and between the two columns the well spacing that makes
each column an independent well region:

![End caps at a column boundary](images/05_end_caps.png)

Regenerate all of these with [images/capture.sh](images/capture.sh), which
drives the capture hook described below.

## Controls

Mouse wheel zooms, left-drag pans. Controls are split by what they act on:

  * **Run** — pause at every snapshot, Step, Continue, and Save PNG
  * **View** — movable dots, the two displacement overlays, and Fit

## Displacement overlays

Two independent toggles, both off by default, overlay an arrow per movable cell
drawn from where that cell sat in an earlier placement to where it sits now.
Either or both can be enabled, so total and incremental movement can be compared
in one view.

  * *Displacement vs global* (red) — total movement since global placement
    finished, i.e. what legalization and detailed placement cost overall. This
    overlay is empty while global placement is still running, since there is no
    global placement result to compare against yet.
  * *Displacement vs previous* (blue) — movement contributed by the current
    stage alone.

Arrows are drawn to scale. Late detailed-placement stages move cells by a
fraction of a row, which is close to invisible at fit-to-view zoom, so zoom in
to inspect them. As a rough guide, on a large gridded design the vs-global
arrows are clearly visible from detailed placement onward, while vs-previous is
most informative during global placement.

## Capturing screenshots

The GUI can write PNGs of chosen snapshots without an operator at the window,
which is how the images above are produced. It is off unless asked for: with
`DALI_GUI_CAPTURE` unset the GUI behaves exactly as if the feature did not
exist.

    DALI_GUI_CAPTURE="<dir>;<stem>@<snapshot id>:<region>;..."
    DALI_GUI_CAPTURE_SIZE="<width>x<height>"

Each request writes `<dir>/<stem>.png` when the named snapshot arrives, so one
snapshot can be captured at several zoom levels. `<region>` is either `fit` for
the whole design, or `<fx0>,<fy0>,<fx1>,<fy1>` as fractions of the design
bounding box — fractions rather than microns so the same request frames a
comparable area on any design. Append `+cells` to draw movable cells as
rectangles instead of dots.

Set `QT_QPA_PLATFORM=offscreen` to run without a display.
