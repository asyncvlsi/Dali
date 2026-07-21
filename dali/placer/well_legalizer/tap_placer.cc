/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/tap_placer.h"

#include <iostream>

#include "dali/common/logging.h"

namespace dali {

void RowEndTapPlacer::ValidateRow(const GriddedRow& row,
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

std::vector<double> RowEndTapPlacer::RowTapCenters(
    const GriddedRow& row, std::size_t /*row_index*/,
    const TapPlacementContext& ctx) const {
  const double tap_width = ctx.well_tap_macro->Width();
  return {
      row.LLX() + ctx.pre_end_cap_width + tap_width / 2.0,
      row.URX() - ctx.post_end_cap_width - tap_width / 2.0,
  };
}

std::vector<double> EveryOtherRowTapPlacer::RowTapCenters(
    const GriddedRow& row, std::size_t row_index,
    const TapPlacementContext& ctx) const {
  if (row_index % 2 == 1) {
    return {};  // Odd rows rely on taps in the abutting even rows.
  }
  return RowEndTapPlacer::RowTapCenters(row, row_index, ctx);
}

bool WellTapPatternHasFixedCount(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kEveryOtherRow:
      return false;
    case WellTapPattern::kRowEnd:
      return true;
  }
  return true;
}

bool IsWellTapPatternSupported(WellTapPattern pattern) {
  switch (pattern) {
    case WellTapPattern::kRowEnd:
      return true;
    // every-other-row places and legalizes but is not yet handled by the
    // well-implant geometry builder; keep it known-but-unsupported until that
    // work lands.
    case WellTapPattern::kEveryOtherRow:
      return false;
  }
  return false;
}

const std::vector<std::string>& KnownWellTapPatternNames() {
  static const std::vector<std::string> names = {
      WellTapPatternName(WellTapPattern::kRowEnd),
      WellTapPatternName(WellTapPattern::kEveryOtherRow),
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
  switch (pattern) {
    case WellTapPattern::kEveryOtherRow:
      return std::make_unique<EveryOtherRowTapPlacer>();
    case WellTapPattern::kRowEnd:
      return std::make_unique<RowEndTapPlacer>();
  }
  return std::make_unique<RowEndTapPlacer>();
}

bool TryParseWellTapPattern(const std::string& name, WellTapPattern* pattern) {
  if (name == "row-end" || name == "row_end") {
    *pattern = WellTapPattern::kRowEnd;
    return true;
  }
  if (name == "every-other-row" || name == "every_other_row") {
    *pattern = WellTapPattern::kEveryOtherRow;
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
    case WellTapPattern::kEveryOtherRow:
      return "every-other-row";
    case WellTapPattern::kRowEnd:
      return "row-end";
  }
  return "row-end";
}

}  // namespace dali
