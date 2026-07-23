/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

/**
 * @file
 * Collects the well-filling rectangles a stripe needs for DEF output.
 */

#include "stripe_helper.h"

#include <algorithm>

namespace dali {

void CollectWellFillingRects(const Stripe& stripe, int bottom_boundary,
                             int top_boundary, std::vector<RectI>& n_rects,
                             std::vector<RectI>& p_rects) {
  std::vector<const GriddedRow*> rows;
  rows.reserve(stripe.gridded_rows_.size());
  for (auto& row : stripe.gridded_rows_) {
    rows.push_back(&row);
  }
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return lhs->LLY() < rhs->LLY();
            });

  int loc_bottom = bottom_boundary;
  if (!rows.empty()) {
    loc_bottom = std::min(loc_bottom, rows.front()->LLY());
  }
  int loc_top = top_boundary;
  if (!rows.empty()) {
    loc_top = std::max(loc_top, rows.back()->URY());
  }

  std::vector<int> pn_edge_list;
  pn_edge_list.reserve(rows.size() + 2);
  pn_edge_list.push_back(loc_bottom);
  for (auto* row : rows) {
    pn_edge_list.push_back(row->LLY() + row->PNEdge());
  }
  pn_edge_list.push_back(loc_top);

  bool is_p_well_rect;
  if (rows.empty()) {
    is_p_well_rect = stripe.is_first_row_orient_N_;
  } else {
    is_p_well_rect = rows.front()->IsOrientN();
  }
  int lx = stripe.LLX();
  int ux = stripe.URX();
  int ly;
  int uy;
  int rect_count = (int)pn_edge_list.size() - 1;
  for (int i = 0; i < rect_count; ++i) {
    ly = pn_edge_list[i];
    uy = pn_edge_list[i + 1];
    if (uy <= ly) {
      is_p_well_rect = !is_p_well_rect;
      continue;
    }
    if (is_p_well_rect) {
      p_rects.emplace_back(lx, ly, ux, uy);
    } else {
      n_rects.emplace_back(lx, ly, ux, uy);
    }
    is_p_well_rect = !is_p_well_rect;
  }
}

}  // namespace dali
