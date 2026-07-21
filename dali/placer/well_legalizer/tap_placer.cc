/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/tap_placer.h"

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

}  // namespace dali
