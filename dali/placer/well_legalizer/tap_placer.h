/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_TAP_PLACER_H_
#define DALI_PLACER_WELL_LEGALIZER_TAP_PLACER_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/gridded_row.h"

namespace dali {

/** Geometry and macro parameters a TapPlacer needs to position taps in a row. */
struct TapPlacementContext {
  Macro* well_tap_macro = nullptr;
  int pre_end_cap_width = 0;
  int post_end_cap_width = 0;
  int space_to_well_tap = 0;
};

/**
 * Strategy that decides where well-tap cells go within a legalized gridded row.
 *
 * Implementations return the tap-cell center X coordinates (grid units, left to
 * right) for one row. An empty list means the row receives no taps of its own;
 * its cells must then be covered by taps in neighboring rows, which the
 * GriddedPlacementValidator's MaxPlugDist coverage check enforces
 * independently. Component creation and Y/orientation placement stay with
 * WellRowCompleter, so a single materialization path serves every pattern.
 */
class TapPlacer {
 public:
  virtual ~TapPlacer() = default;

  /** Human-readable pattern name, for logging. */
  virtual std::string Name() const = 0;

  /** Check row preconditions (reserved margins, interval clearance) before
   * placement. Default: no preconditions. */
  virtual void ValidateRow(const GriddedRow& /*row*/,
                           const TapPlacementContext& /*ctx*/) const {}

  /** Tap-cell center X coordinates (grid units, left to right) for this row.
   * `row_index` is the row's flat position across all stripes, so patterns can
   * vary placement by row parity. */
  virtual std::vector<double> RowTapCenters(
      const GriddedRow& row, std::size_t row_index,
      const TapPlacementContext& ctx) const = 0;
};

/** Which rows carry taps. */
enum class TapCadence {
  kEveryRow,        // Every row gets its taps.
  kEveryOtherRow,   // Even rows only; odd rows rely on neighbors' taps.
};

/**
 * Places taps in the reserved row-end margins at the configured cadence. An
 * every-other cadence leaves odd rows untapped, relying on the abutting rows'
 * taps -- the coverage verifier is the guard for that.
 */
class RowTapPlacer : public TapPlacer {
 public:
  explicit RowTapPlacer(TapCadence cadence) : cadence_(cadence) {}

  std::string Name() const override;
  void ValidateRow(const GriddedRow& row,
                   const TapPlacementContext& ctx) const override;
  std::vector<double> RowTapCenters(
      const GriddedRow& row, std::size_t row_index,
      const TapPlacementContext& ctx) const override;

 private:
  /** Whether this row carries taps under the configured cadence. */
  bool PlacesTapInRow(std::size_t row_index) const;

  TapCadence cadence_;
};

/** Selectable well-tap placement patterns, exposed via -well_tap_pattern. Taps
 * always sit in the reserved row-end margins; patterns differ only in which
 * rows carry them. */
enum class WellTapPattern {
  kRowEnd,            // row-end, every row (default).
  kRowEndEveryOther,  // row-end, every other row.
};

/** Row cadence a pattern uses. */
TapCadence CadenceOf(WellTapPattern pattern);

/** Whether a pattern places the same number of taps in every row (row-end) or a
 * per-row-varying number (sparse patterns rely on the coverage check instead of
 * an exact tap count / taps-in-every-row invariant). */
bool WellTapPatternHasFixedCount(WellTapPattern pattern);

/** Whether a pattern runs end-to-end today. Patterns that place and legalize but
 * are not yet handled by later stages (e.g. the well-implant geometry builder)
 * are known but unsupported until that work lands. This is the single source of
 * truth for option validation -- deliberately independent of tap count. */
bool IsWellTapPatternSupported(WellTapPattern pattern);

/** Canonical CLI names of every known pattern, in enum order. */
const std::vector<std::string>& KnownWellTapPatternNames();

/** Comma-separated canonical names of the patterns that run end-to-end today. */
std::string SupportedWellTapPatternList();

/** Construct the TapPlacer implementing a pattern. */
std::unique_ptr<TapPlacer> CreateTapPlacer(WellTapPattern pattern);

/** Parse a known CLI pattern name into `*pattern`. Returns false (leaving
 * `*pattern` untouched) when the name is not a known pattern. */
bool TryParseWellTapPattern(const std::string& name, WellTapPattern* pattern);

/** Parse a CLI pattern name; unknown names fall back to row-end with a note. */
WellTapPattern ParseWellTapPattern(const std::string& name);

/** Canonical CLI name for a pattern. */
std::string WellTapPatternName(WellTapPattern pattern);

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_TAP_PLACER_H_
