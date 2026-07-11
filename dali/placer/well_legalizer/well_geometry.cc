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
#include "dali/placer/well_legalizer/well_geometry.h"

#include <algorithm>
#include <cmath>

#include "dali/placer/well_legalizer/stripe_helper.h"

namespace dali {

WellGeometryBuilder::WellGeometryBuilder(
    const std::vector<StripeColumn>& columns, int region_bottom,
    int region_top)
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

std::vector<int> WellGeometryBuilder::CollectTapEdges(
    const Stripe& stripe) const {
  std::vector<int> edges;
  edges.reserve(stripe.gridded_rows_.size() * 2 + 2);
  edges.push_back(stripe.is_bottom_up_ ? region_bottom_ : region_top_);
  for (const auto& row : stripe.gridded_rows_) {
    Component* tap = row.WellTapCell();
    DaliExpects(tap != nullptr,
                "Cannot build implant geometry without row well taps");
    if (stripe.is_bottom_up_) {
      edges.push_back(static_cast<int>(std::round(tap->LLY())));
      edges.push_back(static_cast<int>(std::round(tap->URY())));
    } else {
      edges.push_back(static_cast<int>(std::round(tap->URY())));
      edges.push_back(static_cast<int>(std::round(tap->LLY())));
    }
  }
  edges.push_back(stripe.is_bottom_up_ ? region_top_ : region_bottom_);
  if (!stripe.is_bottom_up_) {
    std::reverse(edges.begin(), edges.end());
  }
  return edges;
}

void WellGeometryBuilder::AppendImplantRects(
    std::vector<WellGeometryRect>* geometry) const {
  DaliExpects(geometry != nullptr, "Cannot append to null implant geometry");
  for (const auto& column : columns_) {
    for (const auto& stripe : column.stripe_list_) {
      if (stripe.gridded_rows_.empty()) {
        continue;
      }

      const GriddedRow& reference_row = stripe.gridded_rows_.front();
      Component* left_tap = reference_row.LeftWellTapCell();
      Component* right_tap = reference_row.RightWellTapCell();
      DaliExpects(left_tap != nullptr && right_tap != nullptr,
                  "Cannot build implant geometry without boundary well taps");

      int left_tap_lx = static_cast<int>(std::round(left_tap->LLX()));
      int left_tap_ux = static_cast<int>(std::round(left_tap->URX()));
      int right_tap_lx = static_cast<int>(std::round(right_tap->LLX()));
      int right_tap_ux = static_cast<int>(std::round(right_tap->URX()));

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

      std::vector<int> tap_edges = CollectTapEdges(stripe);
      DaliExpects(tap_edges.size() % 2 == 0,
                  "Tap geometry requires paired vertical edges");
      is_p_well = stripe.is_first_row_orient_N_;
      for (size_t i = 0; i + 1 < tap_edges.size(); i += 2) {
        WellGeometryLayer layer =
            is_p_well ? WellGeometryLayer::kPplus : WellGeometryLayer::kNplus;
        if (tap_edges[i + 1] > tap_edges[i]) {
          geometry->push_back(
              {RectI(left_tap_lx, tap_edges[i], left_tap_ux, tap_edges[i + 1]),
               layer});
          geometry->push_back({RectI(right_tap_lx, tap_edges[i], right_tap_ux,
                                     tap_edges[i + 1]),
                               layer});
        }
        is_p_well = !is_p_well;
      }
    }
  }
}

}  // namespace dali
