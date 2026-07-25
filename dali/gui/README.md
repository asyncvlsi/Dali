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

Pausing lets intermediate state be inspected as it is built. The window also
plots per-stage HPWL curves.

## What it shows

Global placement finishes with cells spread by density and no row structure —
each dot is one movable cell:

![After global placement](images/01_global_placement.png)

After legalization the same design is organised into stripe columns, with the
P and N wells of the gridded rows shown in pink and blue:

![After legalization](images/02_legalized.png)

Zooming in resolves individual rows across a few stripe columns:

![Stripe columns](images/03_stripe_columns.png)

Zoomed further, cells are drawn as rectangles with a corner marker showing
orientation, and the well taps are orange:

![Cells and well taps](images/04_cells_and_taps.png)

At a column boundary: cells, orange well taps, green end caps, and the spacing
between columns:

![End caps at a column boundary](images/05_end_caps.png)

Regenerate all of these with [images/capture.sh](images/capture.sh), which
drives the capture hook described below.

## Controls

Mouse wheel zooms, left-drag pans. Controls are split by what they act on:

  * **Run** — pause at every snapshot, Step, Continue, and Save PNG
  * **View** — movable dots, I/O pins, the two displacement overlays, and Fit

## Interactive signoff

Combine `-gui_debug` with `-interactive` to use the terminal for commands while
the Qt window monitors the design:

    $ dali \
        -lef design.lef \
        -def placed.def \
        -gui_debug \
        -interactive

Placed I/O pins are drawn above cells. Fixed pins are magenta and other placed
pins are blue; the **I/O pins** checkbox toggles the layer. Successful
placement-changing commands such as `move-io` publish a fresh snapshot. The
window remains responsive while the prompt waits for input and stays open
until the session ends and the window is closed.

## Displacement overlays

Two independent toggles, both off by default, draw an arrow per movable cell
from where it sat earlier to where it sits now. Either or both can be enabled.

  * *Displacement vs global* (red) — total movement since global placement
    finished. Empty while global placement is still running.
  * *Displacement vs previous* (blue) — movement from the current stage alone.

Arrows are drawn to scale, so late stages that move cells by a fraction of a row
need zooming in to see.

## Capturing screenshots

The GUI can write PNGs of chosen snapshots unattended, which is how the images
above are produced. Off unless `DALI_GUI_CAPTURE` is set.

    DALI_GUI_CAPTURE="<dir>;<stem>@<snapshot id>:<region>;..."
    DALI_GUI_CAPTURE_SIZE="<width>x<height>"

Each request writes `<dir>/<stem>.png` when the named snapshot arrives, so one
snapshot can be captured at several zoom levels. `<region>` is `fit` for the
whole design, or `<fx0>,<fy0>,<fx1>,<fy1>` as fractions of the design bounding
box. Append `+cells` to draw cells as rectangles instead of dots.

Set `QT_QPA_PLATFORM=offscreen` to run without a display.
