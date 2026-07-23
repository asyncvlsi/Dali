# Placer architecture notes

Facts that hold across several files and belong to none of them. Anything here
is worth knowing before editing the placer; per-file details live in the file
headers themselves.

## Stages

`Dali::StartPlacement` runs the core stages, then post-placement completion:

    global placement
      -> legalization        (standard-cell or gridded well, by flow)
         -> detailed placement
      -> filler cells
      -> I/O pins

Detailed placement runs inside the legalization stage, after the legalizer has
handed over its context, not as a separate top-level stage. Legalization is
skipped wholesale by `-disable_legalization`, which takes detailed placement
with it.

## The two flows

A `-cell` file selects gridded well placement; `-is_standard_cell` selects the
standard-cell flow. They are not variations on one path — they use different
legalizers, and several quantities mean different things in each:

  * **Upper bound.** In the gridded flow with the rough legalizer enabled, the
    upper bound is a roughly legalized placement — what legalization will
    actually produce. Everywhere else it is the spread placement, with nothing
    legalized. The two differ by tens of percent, so any series mixing them is
    meaningless. `GlobalPlacer` keeps the rough-legalized ones separately for
    exactly this reason.
  * **The lower/upper HPWL gap.** For standard cells it measures how far
    placement is from converged, and falls to a few percent. For gridded designs
    it measures how much legalization costs, plateaus near 30%, and never
    shrinks. Convergence tests must not treat it as the same signal.

## The gridded data model

Containment runs:

    StripeColumn -> Stripe -> GriddedRow -> RowSegment -> Component

  * A **stripe column** is a full-height slice of the placement region and its
    own well region. Neighbouring columns are separated by well spacing, which
    is what makes them independent.
  * A **stripe** is the unit legalization succeeds or fails on. A stripe whose
    used height exceeds its available height has failed; the flow reports which
    stripes failed rather than emitting an illegal placement.
  * A **gridded row** is as tall as the tallest cell clustered into it, so rows
    are not on a fixed pitch. Adjacent rows are flipped so like wells abut.
  * A **row segment** is a contiguous run of usable whitespace within a row.
    Blockages and already-placed taps split a row into segments, each legalized
    in X on its own.

Two consequences that are easy to get wrong:

  * Row heights are content-dependent, so **adjacent columns do not share a row
    grid** — their P/N boundaries sit at different heights. Nothing may assume a
    row index is comparable across columns.
  * Row width is bounded by the technology's MaxPlugDist, because every
    transistor must sit within that distance of a compatible-well tap. Forcing a
    wider row through `-max_row_width` produces a placement that violates the
    latch-up rule, and the coverage check will reject it.

## Physical completion

Well taps and end caps are inserted after legalization, and are tracked apart
from assigned cells. A tap is short — it cannot span a row — so it straddles the
row's P/N boundary to tie both wells, and the rest of its column is filled with
implant. That fill is required for DRC, not cosmetic; see
[well-tap patterns](well_legalizer/README.md) for the full rule set.

## Validation

`GriddedPlacementValidator` checks a finished gridded placement geometrically —
rows within stripes, cells within rows, no overlaps, orientation, and tap
coverage against MaxPlugDist. The coverage check is deliberately
pattern-agnostic: it measures the distance from each cell to the nearest
compatible tap rather than assuming where a pattern puts them, so a new tap
pattern is validated without teaching the validator about it.
