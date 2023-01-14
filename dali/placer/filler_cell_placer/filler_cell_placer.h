/*******************************************************************************
 *
 * Copyright (c) 2023 Yihang Yang
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
#ifndef DALI_PLACER_FILLER_CELL_PLACER_FILLER_CELL_PLACER_H_
#define DALI_PLACER_FILLER_CELL_PLACER_FILLER_CELL_PLACER_H_

#include <phydb/phydb.h>

#include "dali/placer/placer.h"

namespace dali {

class FillerCellPlacer : public Placer{
  friend class Dali;
 public:
  FillerCellPlacer() = default;

  void CreateFillerCellTypes(int upper_width);
  void PlaceFillerCells(int lx, int ux, int ly, bool is_orient_N, int &filler_counter);
  bool StartPlacement() override;
 private:
  phydb::PhyDB *phy_db_ptr_ = nullptr;
};

} // dali

#endif //DALI_PLACER_FILLER_CELL_PLACER_FILLER_CELL_PLACER_H_
