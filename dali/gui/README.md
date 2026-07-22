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
