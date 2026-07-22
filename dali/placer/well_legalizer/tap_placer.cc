/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/tap_placer.h"

#include <iostream>

#include "dali/common/logging.h"

namespace dali {

std::string RowTapPlacer::Name() const {
  const std::string base =
      position_ == TapPosition::kRowMid ? "row-mid" : "row-end";
  return cadence_ == TapCadence::kEveryOtherRow ? base + "-every-other" : base;
}

void RowTapPlacer::ValidateRow(const GriddedRow& row,
                               const TapPlacementContext& ctx) const {
  DaliExpects(ctx.well_tap_macro != nullptr,
              "Cannot place well taps without a well-tap macro");
  // row-mid reserves an interior gap at the row center rather than the boundary
  // margins; that reservation is part of the not-yet-implemented row-splitting
  // work, so only the row-end preconditions are checked here (row-mid never
  // reaches placement while unsupported).
  if (position_ != TapPosition::kRowEnd) {
    return;
  }
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
  if (position_ == TapPosition::kRowMid) {
    return {(row.LLX() + row.URX()) / 2.0};
  }
  return {
      row.LLX() + ctx.pre_end_cap_width + tap_width / 2.0,
      row.URX() - ctx.post_end_cap_width - tap_width / 2.0,
  };
}

TapPosition PositionOf(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kRowMid:
      return TapPosition::kRowMid;
    case WellTapPattern::kRowEnd:
    case WellTapPattern::kRowEndEveryOther:
      return TapPosition::kRowEnd;
  }
  return TapPosition::kRowEnd;
}

TapCadence CadenceOf(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kRowEndEveryOther:
      return TapCadence::kEveryOtherRow;
    case WellTapPattern::kRowEnd:
    case WellTapPattern::kRowMid:
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
    // Known and planned, but not yet handled end-to-end: a row-mid tap splits
    // the row into two segments, which the space partitioner does not yet
    // produce. Keep it known-but-unsupported until that lands.
    case WellTapPattern::kRowMid:
      return false;
  }
  return false;
}

const std::vector<std::string>& KnownWellTapPatternNames() {
  static const std::vector<std::string> names = {
      WellTapPatternName(WellTapPattern::kRowEnd),
      WellTapPatternName(WellTapPattern::kRowEndEveryOther),
      WellTapPatternName(WellTapPattern::kRowMid),
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
  return std::make_unique<RowTapPlacer>(PositionOf(pattern), CadenceOf(pattern));
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
  if (name == "row-mid" || name == "row_mid") {
    *pattern = WellTapPattern::kRowMid;
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
    case WellTapPattern::kRowMid:
      return "row-mid";
    case WellTapPattern::kRowEnd:
      return "row-end";
  }
  return "row-end";
}

}  // namespace dali
