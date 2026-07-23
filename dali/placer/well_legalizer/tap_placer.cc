/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/

/**
 * @file
 * Chooses where well taps go within a row, and parses the pattern option.
 *
 * A pattern decides tap positions only; whether the result satisfies the
 * latch-up rule is checked separately and geometrically, so adding a pattern
 * does not mean teaching the validator about it. See
 * [well-tap patterns](README.md) for what a new pattern must satisfy.
 */
#include "dali/placer/well_legalizer/tap_placer.h"

#include <iostream>

#include "dali/common/logging.h"

namespace dali {

std::string RowTapPlacer::Name() const {
  return cadence_ == TapCadence::kEveryOtherRow ? "row-end-every-other"
                                                : "row-end";
}

void RowTapPlacer::ValidateRow(const GriddedRow& row,
                               const TapPlacementContext& ctx) const {
  DaliExpects(ctx.well_tap_macro != nullptr,
              "Cannot place well taps without a well-tap macro");
  const int tap_width = ctx.well_tap_macro->Width();
  const int required_left_margin =
      ctx.pre_end_cap_width + tap_width + ctx.space_to_well_tap;
  const int required_right_margin =
      ctx.post_end_cap_width + tap_width + ctx.space_to_well_tap;
  DaliExpects(
      row.LeftBoundaryMargin() >= required_left_margin &&
          row.RightBoundaryMargin() >= required_right_margin,
      "Gridded row did not reserve enough physical-completion space");
  for (const Component* component : row.Components()) {
    DaliExpects(
        component->LLX() >= row.LLX() + required_left_margin &&
            component->URX() <= row.URX() - required_right_margin,
        "Ordinary component is outside the physical-completion interval");
  }
}

std::vector<double> RowTapPlacer::RowTapCenters(
    const GriddedRow& row, std::size_t row_index,
    const TapPlacementContext& ctx) const {
  if (cadence_ == TapCadence::kEveryOtherRow && row_index % 2 == 1) {
    return {};  // Odd rows rely on taps in the abutting even rows.
  }
  const double tap_width = ctx.well_tap_macro->Width();
  return {
      row.LLX() + ctx.pre_end_cap_width + tap_width / 2.0,
      row.URX() - ctx.post_end_cap_width - tap_width / 2.0,
  };
}

TapCadence CadenceOf(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kRowEndEveryOther:
      return TapCadence::kEveryOtherRow;
    case WellTapPattern::kRowEnd:
      return TapCadence::kEveryRow;
  }
  return TapCadence::kEveryRow;
}

bool WellTapPatternHasFixedCount(WellTapPattern pattern) {
  // Fixed-count patterns place the same number of taps in every row. Only the
  // every-other cadence varies the per-row count.
  return CadenceOf(pattern) != TapCadence::kEveryOtherRow;
}

bool IsWellTapPatternSupported(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kRowEnd:
    case WellTapPattern::kRowEndEveryOther:
      return true;
  }
  return false;
}

const std::vector<std::string>& KnownWellTapPatternNames() {
  static const std::vector<std::string> names = {
      WellTapPatternName(WellTapPattern::kRowEnd),
      WellTapPatternName(WellTapPattern::kRowEndEveryOther),
  };
  return names;
}

std::string SupportedWellTapPatternList() {
  std::string list;
  for (const std::string& name : KnownWellTapPatternNames()) {
    WellTapPattern pattern;
    if (TryParseWellTapPattern(name, &pattern) &&
        IsWellTapPatternSupported(pattern)) {
      if (!list.empty()) list += ", ";
      list += name;
    }
  }
  return list;
}

std::unique_ptr<TapPlacer> CreateTapPlacer(WellTapPattern pattern) {
  return std::make_unique<RowTapPlacer>(CadenceOf(pattern));
}

bool TryParseWellTapPattern(const std::string& name, WellTapPattern* pattern) {
  if (name == "row-end" || name == "row_end") {
    *pattern = WellTapPattern::kRowEnd;
    return true;
  }
  // "every-other-row" is the legacy name for the row-end every-other pattern.
  if (name == "row-end-every-other" || name == "row_end_every_other" ||
      name == "every-other-row" || name == "every_other_row") {
    *pattern = WellTapPattern::kRowEndEveryOther;
    return true;
  }
  return false;
}

WellTapPattern ParseWellTapPattern(const std::string& name) {
  WellTapPattern pattern = WellTapPattern::kRowEnd;
  if (!TryParseWellTapPattern(name, &pattern)) {
    std::cout << "Ignore unknown well_tap_pattern: " << name
              << " (using row-end)\n";
  }
  return pattern;
}

std::string WellTapPatternName(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kRowEndEveryOther:
      return "row-end-every-other";
    case WellTapPattern::kRowEnd:
      return "row-end";
  }
  return "row-end";
}

}  // namespace dali
