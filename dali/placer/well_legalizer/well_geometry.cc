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

/**
 * @file
 * Builds the well and implant rectangles a legal placement must be shipped with.
 *
 * A well-tap cell is short and cannot span a row, so the rest of its column has
 * to be filled: above the tap, below it, and across any row the pattern leaves
 * untapped. That fill is a DRC requirement, not decoration -- a gap in the
 * implant layer causes minimum-area, notch and spacing violations. Both the
 * cell-area and tap-column passes follow the same P/N banding so the implant
 * type always matches the well beneath it.
 */
#include "dali/placer/well_legalizer/well_geometry.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "dali/placer/well_legalizer/stripe_helper.h"

namespace dali {

WellGeometryBuilder::WellGeometryBuilder(
    const std::vector<StripeColumn>& columns, int region_bottom, int region_top)
    : columns_(columns),
      region_bottom_(region_bottom),
      region_top_(region_top) {
  DaliExpects(region_top_ >= region_bottom_,
              "Well geometry requires a valid vertical region");
}

std::vector<WellGeometryRect> WellGeometryBuilder::Build(
    bool include_implants) const {
  std::vector<WellGeometryRect> geometry;
  AppendWellRects(&geometry);
  if (include_implants) {
    AppendImplantRects(&geometry);
  }
  return geometry;
}

void WellGeometryBuilder::AppendWellRects(
    std::vector<WellGeometryRect>* geometry) const {
  DaliExpects(geometry != nullptr, "Cannot append to null well geometry");
  for (const auto& column : columns_) {
    for (const auto& stripe : column.stripe_list_) {
      std::vector<RectI> n_rects;
      std::vector<RectI> p_rects;
      CollectWellFillingRects(stripe, region_bottom_, region_top_, n_rects,
                              p_rects);
      for (const RectI& rect : p_rects) {
        geometry->push_back({rect, WellGeometryLayer::kPwell});
      }
      for (const RectI& rect : n_rects) {
        geometry->push_back({rect, WellGeometryLayer::kNwell});
      }
    }
  }
}

std::vector<int> WellGeometryBuilder::CollectPnEdges(
    const Stripe& stripe) const {
  std::vector<int> edges;
  edges.reserve(stripe.gridded_rows_.size() + 2);
  edges.push_back(stripe.is_bottom_up_ ? region_bottom_ : region_top_);
  for (const auto& row : stripe.gridded_rows_) {
    edges.push_back(row.LLY() + row.PNEdge());
  }
  edges.push_back(stripe.is_bottom_up_ ? region_top_ : region_bottom_);
  if (!stripe.is_bottom_up_) {
    std::reverse(edges.begin(), edges.end());
  }
  return edges;
}

std::vector<std::pair<int, int>> WellGeometryBuilder::CollectTapCoverage(
    const Stripe& stripe) const {
  std::vector<std::pair<int, int>> covered;
  covered.reserve(stripe.gridded_rows_.size());
  for (const auto& row : stripe.gridded_rows_) {
    for (const Component* tap : row.TapCells()) {
      if (tap == nullptr) continue;
      const int lo = static_cast<int>(std::round(tap->LLY()));
      const int hi = static_cast<int>(std::round(tap->URY()));
      if (hi > lo) covered.emplace_back(lo, hi);
    }
  }
  std::sort(covered.begin(), covered.end());

  std::vector<std::pair<int, int>> merged;
  for (const auto& interval : covered) {
    if (!merged.empty() && interval.first <= merged.back().second) {
      merged.back().second = std::max(merged.back().second, interval.second);
    } else {
      merged.push_back(interval);
    }
  }
  return merged;
}

namespace {

/**
 * Return the sub-intervals of [lo, hi) left uncovered by `covered`, which must
 * be sorted ascending and non-overlapping. Used to find the parts of a well
 * band whose tap column holds no tap cell and therefore still needs implant.
 */
std::vector<std::pair<int, int>> UncoveredSpans(
    int lo, int hi, const std::vector<std::pair<int, int>>& covered) {
  std::vector<std::pair<int, int>> spans;
  int cursor = lo;
  for (const auto& interval : covered) {
    if (interval.second <= cursor) continue;
    if (interval.first >= hi) break;
    if (interval.first > cursor) {
      spans.emplace_back(cursor, std::min(interval.first, hi));
    }
    cursor = std::max(cursor, interval.second);
    if (cursor >= hi) break;
  }
  if (cursor < hi) spans.emplace_back(cursor, hi);
  return spans;
}

}  // namespace

void WellGeometryBuilder::AppendImplantRects(
    std::vector<WellGeometryRect>* geometry) const {
  DaliExpects(geometry != nullptr, "Cannot append to null implant geometry");
  for (const auto& column : columns_) {
    for (const auto& stripe : column.stripe_list_) {
      if (stripe.gridded_rows_.empty()) {
        continue;
      }

      const GriddedRow* reference_row = nullptr;
      for (const auto& row : stripe.gridded_rows_) {
        if (row.LeftWellTapCell() != nullptr &&
            row.RightWellTapCell() != nullptr) {
          reference_row = &row;
          break;
        }
      }
      DaliExpects(reference_row != nullptr,
                  "Cannot build implant geometry without boundary well taps");

      int left_tap_lx =
          static_cast<int>(std::round(reference_row->LeftWellTapCell()->LLX()));
      int left_tap_ux =
          static_cast<int>(std::round(reference_row->LeftWellTapCell()->URX()));
      int right_tap_lx =
          static_cast<int>(std::round(reference_row->RightWellTapCell()->LLX()));
      int right_tap_ux =
          static_cast<int>(std::round(reference_row->RightWellTapCell()->URX()));

      std::vector<int> pn_edges = CollectPnEdges(stripe);
      bool is_p_well = stripe.is_first_row_orient_N_;
      for (size_t i = 0; i + 1 < pn_edges.size(); ++i) {
        WellGeometryLayer layer =
            is_p_well ? WellGeometryLayer::kNplus : WellGeometryLayer::kPplus;
        if (pn_edges[i + 1] > pn_edges[i] && right_tap_lx > left_tap_ux) {
          geometry->push_back(
              {RectI(left_tap_ux, pn_edges[i], right_tap_lx, pn_edges[i + 1]),
               layer});
        }
        is_p_well = !is_p_well;
      }

      const std::vector<std::pair<int, int>> covered = CollectTapCoverage(stripe);
      is_p_well = stripe.is_first_row_orient_N_;
      for (size_t i = 0; i + 1 < pn_edges.size(); ++i) {
        WellGeometryLayer layer =
            is_p_well ? WellGeometryLayer::kPplus : WellGeometryLayer::kNplus;
        for (const auto& span :
             UncoveredSpans(pn_edges[i], pn_edges[i + 1], covered)) {
          geometry->push_back(
              {RectI(left_tap_lx, span.first, left_tap_ux, span.second), layer});
          geometry->push_back(
              {RectI(right_tap_lx, span.first, right_tap_ux, span.second),
               layer});
        }
        is_p_well = !is_p_well;
      }
    }
  }
}

}  // namespace dali
