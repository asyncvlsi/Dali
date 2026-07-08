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
#ifndef DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_PLACEMENT_MODEL_H_
#define DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_PLACEMENT_MODEL_H_

#include <optional>
#include <vector>

#include "dali/common/misc.h"

namespace dali {

/** Horizontal legal interval on one standard-cell row. */
struct StandardCellFreeSegment {
  int lx = 0;
  int ux = 0;

  int Width() const { return ux - lx; }
  bool CanFit(int width) const { return Width() >= width; }
};

/** Row geometry used by standard-cell legalization. */
struct StandardCellRow {
  int lx = 0;
  int ly = 0;
  int height = 0;
  int site_width = 1;
  int site_count = 0;
  std::vector<StandardCellFreeSegment> free_segments;

  int Ux() const { return lx + site_width * site_count; }
  int Uy() const { return ly + height; }
};

/** Rectangular fixed blockage clipped against standard-cell rows. */
struct StandardCellBlockage {
  int lx = 0;
  int ly = 0;
  int ux = 0;
  int uy = 0;
};

/**
 * Standard-cell row and whitespace model.
 *
 * The model stores DEF-style rows and subtracts fixed macro/blockage rectangles
 * to produce legal row intervals. Later legalization stages can use the free
 * segments without knowing where the blockages came from.
 */
class StandardCellPlacementModel {
 public:
  /** Add one row with the given origin, site size, and number of sites. */
  void AddRow(int lx, int ly, int height, int site_width, int site_count);

  /** Add a fixed blockage rectangle in grid units. */
  void AddBlockage(int lx, int ly, int ux, int uy);

  /** Rebuild free row segments after rows or blockages change. */
  void BuildFreeSegments();

  /** Return all rows. */
  const std::vector<StandardCellRow>& Rows() const { return rows_; }

  /** Return all rows. */
  std::vector<StandardCellRow>& Rows() { return rows_; }

  /** Return row count. */
  int RowCount() const { return static_cast<int>(rows_.size()); }

  /** Find the row containing y, or std::nullopt if y is outside all rows. */
  std::optional<int> RowIndexAtY(int y) const;

  /** Return true when [lx, ux) lies inside one free segment on row_index. */
  bool IsIntervalFree(int row_index, int lx, int ux) const;

 private:
  static int AlignUpToSite(int x, const StandardCellRow& row);
  static int AlignDownToSite(int x, const StandardCellRow& row);

  std::vector<StandardCellRow> rows_;
  std::vector<StandardCellBlockage> blockages_;
};

}  // namespace dali

#endif  // DALI_PLACER_LEGALIZER_STANDARD_CELL_LEGALIZER_STANDARD_CELL_PLACEMENT_MODEL_H_
