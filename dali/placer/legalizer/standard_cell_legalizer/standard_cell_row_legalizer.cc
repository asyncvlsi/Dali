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
 * Places the cells assigned to one standard-cell row along X.
 */
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_row_legalizer.h"

#include <algorithm>
#include <cmath>

#include "dali/common/misc.h"

namespace dali {

/**
 * Place the cells assigned to one row's free segment along X.
 *
 * Clusters span contiguous ranges of the sorted cells. On a merge, the
 * left cluster's sum is the unchanged prefix of the merged sum. Append
 * right-hand terms in cell order to preserve the rounding of a full scan.
 *
 * @param site_width the row's placement site pitch.
 * @return true if all cells fit legally in the segment.
 */
bool StandardCellRowLegalizer::Legalize(
    StandardCellFreeSegment segment, int site_width,
    std::vector<StandardCellRowLegalizationCell>* cells) const {
  DaliExpects(cells != nullptr, "Cannot legalize a null cell list");
  DaliExpects(site_width > 0, "Site width must be positive");

  int total_width = 0;
  for (const auto& cell : *cells) {
    DaliExpects(cell.width > 0, "Cell width must be positive");
    total_width += cell.width;
  }
  if (total_width > segment.Width()) {
    return false;
  }

  std::sort(cells->begin(), cells->end(),
            [](const StandardCellRowLegalizationCell& lhs,
               const StandardCellRowLegalizationCell& rhs) {
              if (lhs.target_lx == rhs.target_lx) {
                return lhs.id < rhs.id;
              }
              return lhs.target_lx < rhs.target_lx;
            });

  std::vector<Cluster> clusters;
  clusters.reserve(cells->size());
  for (int cell_index = 0; cell_index < static_cast<int>(cells->size());
       ++cell_index) {
    const auto& cell = (*cells)[cell_index];
    auto& cluster = clusters.emplace_back();
    cluster.begin = cell_index;
    cluster.end = cell_index + 1;
    cluster.total_width = cell.width;
    cluster.shifted_target_sum += cell.target_lx;
    cluster.lx = ComputeClusterX(cluster, segment, site_width);

    while (clusters.size() >= 2) {
      auto& right_cluster = clusters.back();
      auto& left_cluster = clusters[clusters.size() - 2];
      if (left_cluster.lx + left_cluster.total_width <= right_cluster.lx) {
        break;
      }

      int offset = left_cluster.total_width;
      for (int index = right_cluster.begin; index < right_cluster.end; ++index) {
        const auto& right_cell = (*cells)[index];
        left_cluster.shifted_target_sum += right_cell.target_lx - offset;
        offset += right_cell.width;
      }
      left_cluster.end = right_cluster.end;
      left_cluster.total_width += right_cluster.total_width;
      left_cluster.lx = ComputeClusterX(left_cluster, segment, site_width);
      clusters.pop_back();
    }
  }

  for (const auto& cluster : clusters) {
    int x = cluster.lx;
    for (int cell_index = cluster.begin; cell_index < cluster.end; ++cell_index) {
      (*cells)[cell_index].legal_lx = x;
      x += (*cells)[cell_index].width;
    }
  }

  return true;
}

int StandardCellRowLegalizer::AlignToNearestSite(
    int x, StandardCellFreeSegment segment, int site_width) {
  int offset = x - segment.lx;
  int snapped_offset =
      static_cast<int>(std::llround(static_cast<double>(offset) / site_width)) *
      site_width;
  return segment.lx + snapped_offset;
}

int StandardCellRowLegalizer::ComputeClusterX(
    const Cluster& cluster, StandardCellFreeSegment segment, int site_width) {
  double optimal_cluster_lx =
      cluster.shifted_target_sum /
      static_cast<double>(cluster.end - cluster.begin);

  int cluster_lx = AlignToNearestSite(
      static_cast<int>(std::llround(optimal_cluster_lx)), segment, site_width);
  int min_lx = segment.lx;
  int max_lx = segment.ux - cluster.total_width;
  return std::max(min_lx, std::min(max_lx, cluster_lx));
}

}  // namespace dali
