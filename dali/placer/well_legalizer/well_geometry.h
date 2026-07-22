/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_WELL_GEOMETRY_H_
#define DALI_PLACER_WELL_LEGALIZER_WELL_GEOMETRY_H_

#include <utility>
#include <vector>

#include "dali/common/misc.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Layer represented by a generated well-geometry rectangle. */
enum class WellGeometryLayer {
  kPwell,
  kNwell,
  kPplus,
  kNplus,
};

/** One generated well or implant rectangle in Dali grid coordinates. */
struct WellGeometryRect {
  RectI bounds;
  WellGeometryLayer layer = WellGeometryLayer::kPwell;
};

/**
 * Builds canonical well and implant geometry from finalized gridded rows.
 *
 * Geometry stays in integer Dali grid coordinates. Output adapters are
 * responsible for converting it to microns or database units.
 */
class WellGeometryBuilder {
 public:
  WellGeometryBuilder(const std::vector<StripeColumn>& columns,
                      int region_bottom, int region_top);

  /** Build N/P-well rectangles and, when requested, N+/P+ rectangles. */
  std::vector<WellGeometryRect> Build(bool include_implants) const;

 private:
  /** Append alternating N/P-well filling rectangles for every stripe. */
  void AppendWellRects(std::vector<WellGeometryRect>* geometry) const;

  /** Append active-area and tap-column implant rectangles. */
  void AppendImplantRects(std::vector<WellGeometryRect>* geometry) const;

  /** Return alternating P/N boundary edges in ascending Y order. */
  std::vector<int> CollectPnEdges(const Stripe& stripe) const;

  /**
   * Return the Y intervals of the tap columns already covered by tap cells,
   * sorted ascending and merged. Tap macros carry their own implant, so the
   * builder only fills what these intervals leave uncovered. Rows without a tap
   * contribute nothing, which is what makes sparse patterns fillable.
   */
  std::vector<std::pair<int, int>> CollectTapCoverage(
      const Stripe& stripe) const;

  const std::vector<StripeColumn>& columns_;
  int region_bottom_ = 0;
  int region_top_ = 0;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_WELL_GEOMETRY_H_
