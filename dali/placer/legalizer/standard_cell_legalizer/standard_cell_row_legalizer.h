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
#ifndef DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_ROW_LEGALIZER_H_
#define DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_ROW_LEGALIZER_H_

#include <vector>

#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_placement_model.h"

namespace dali {

/** Lightweight cell record used by the row legalization primitive. */
struct StandardCellRowLegalizationCell {
  int id = -1;
  int width = 0;
  double target_lx = 0.0;
  int legal_lx = 0;
};

/**
 * Abacus-style one-row legalization primitive.
 *
 * The primitive assumes cells are already assigned to one free row segment. It
 * preserves x order, merges overlapping clusters, snaps cluster starts to
 * sites, and clamps the final packed placement to the segment.
 */
class StandardCellRowLegalizer {
 public:
  /** Legalize cells inside one free segment. Returns false when width
   * overflows. */
  bool Legalize(StandardCellFreeSegment segment, int site_width,
                std::vector<StandardCellRowLegalizationCell>* cells) const;

 private:
  struct Cluster {
    std::vector<int> cell_indices;
    int total_width = 0;
    int lx = 0;
  };

  static int AlignToNearestSite(int x, StandardCellFreeSegment segment,
                                int site_width);
  static int ComputeClusterX(
      const Cluster& cluster,
      const std::vector<StandardCellRowLegalizationCell>& cells,
      StandardCellFreeSegment segment, int site_width);
};

}  // namespace dali

#endif  // DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_ROW_LEGALIZER_H_
