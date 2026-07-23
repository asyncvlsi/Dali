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
 * Cost model the standard-cell legalizer scores candidate placements with,
 * either displacement or wirelength.
 */
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_placement_model.h"

#include <algorithm>

namespace dali {

void StandardCellPlacementModel::AddRow(int lx, int ly, int height,
                                        int site_width, int site_count) {
  DaliExpects(height > 0, "Standard-cell row height must be positive");
  DaliExpects(site_width > 0, "Standard-cell site width must be positive");
  DaliExpects(site_count >= 0, "Standard-cell site count cannot be negative");

  rows_.push_back({lx, ly, height, site_width, site_count, {}});
}

void StandardCellPlacementModel::AddBlockage(int lx, int ly, int ux, int uy) {
  DaliExpects(lx <= ux, "Blockage x range is invalid");
  DaliExpects(ly <= uy, "Blockage y range is invalid");
  if (lx == ux || ly == uy) {
    return;
  }
  blockages_.push_back({lx, ly, ux, uy});
}

/** Build the free-space segments per row from blockages and fixed cells. */
void StandardCellPlacementModel::BuildFreeSegments() {
  for (auto& row : rows_) {
    row.free_segments.clear();
    row.free_segments.push_back({row.lx, row.Ux()});

    for (const auto& blockage : blockages_) {
      if (blockage.uy <= row.ly || blockage.ly >= row.Uy()) {
        continue;
      }

      int block_lx = AlignDownToSite(std::max(blockage.lx, row.lx), row);
      int block_ux = AlignUpToSite(std::min(blockage.ux, row.Ux()), row);
      block_lx = std::max(block_lx, row.lx);
      block_ux = std::min(block_ux, row.Ux());
      if (block_lx >= block_ux) {
        continue;
      }

      std::vector<StandardCellFreeSegment> split_segments;
      for (const auto& segment : row.free_segments) {
        if (block_ux <= segment.lx || block_lx >= segment.ux) {
          split_segments.push_back(segment);
          continue;
        }
        if (segment.lx < block_lx) {
          split_segments.push_back({segment.lx, block_lx});
        }
        if (block_ux < segment.ux) {
          split_segments.push_back({block_ux, segment.ux});
        }
      }
      row.free_segments = split_segments;
    }
  }
}

std::optional<int> StandardCellPlacementModel::RowIndexAtY(int y) const {
  for (int row_index = 0; row_index < static_cast<int>(rows_.size());
       ++row_index) {
    const auto& row = rows_[row_index];
    if (y >= row.ly && y < row.Uy()) {
      return row_index;
    }
  }
  return std::nullopt;
}

bool StandardCellPlacementModel::IsIntervalFree(int row_index, int lx,
                                                int ux) const {
  if (row_index < 0 || row_index >= static_cast<int>(rows_.size()) ||
      lx >= ux) {
    return false;
  }
  const auto& row = rows_[row_index];
  for (const auto& segment : row.free_segments) {
    if (lx >= segment.lx && ux <= segment.ux) {
      return true;
    }
  }
  return false;
}

int StandardCellPlacementModel::AlignUpToSite(int x,
                                              const StandardCellRow& row) {
  int offset = x - row.lx;
  if (offset <= 0) {
    return row.lx;
  }
  int site_count = (offset + row.site_width - 1) / row.site_width;
  return row.lx + site_count * row.site_width;
}

int StandardCellPlacementModel::AlignDownToSite(int x,
                                                const StandardCellRow& row) {
  int offset = x - row.lx;
  if (offset <= 0) {
    return row.lx;
  }
  return row.lx + (offset / row.site_width) * row.site_width;
}

}  // namespace dali
