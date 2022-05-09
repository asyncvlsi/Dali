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
#ifndef DALI_PLACER_IOPLACER_IOPLACER_H_
#define DALI_PLACER_IOPLACER_IOPLACER_H_

#include <vector>

#include <phydb/phydb.h>

#include "dali/circuit/circuit.h"

#include "ioboundaryspace.h"

namespace dali {

/**
 * A class for placing IOPINs.
 * This class mainly provides two functions:
 * 1. manually place IOPINs before placement using given locations
 * 2. automatically place IOPINs after placement
 */
class IoPlacer {
 public:
  IoPlacer();
  explicit IoPlacer(phydb::PhyDB *phy_db, Circuit *circuit_);

  void InitializeBoundarySpaces();
  void SetCiruit(Circuit *circuit);
  void SetPhyDB(phydb::PhyDB *phy_db_ptr);

  bool PartialPlaceIoPin();
  bool PartialPlaceCmd(int argc, char **argv);

  bool ConfigSetMetalLayer(int boundary_index, int metal_layer_index);
  bool ConfigSetGlobalMetalLayer(int metal_layer_index);
  bool ConfigAutoPlace();

  bool ConfigBoundaryMetal(int argc, char **argv);

  static void ReportConfigUsage();
  bool ConfigCmd(int argc, char **argv);

  bool CheckConfiguration();

  bool BuildResourceMap();
  bool AssignIoPinToBoundaryLayers();
  bool PlaceIoPinOnEachBoundary();

  void AdjustIoPinLocationForPhyDB();

  bool AutoPlaceIoPin();
  bool AutoPlaceCmd(int argc, char **argv);
 private:
  Circuit *p_ckt_ = nullptr;
  phydb::PhyDB *phy_db_ptr_ = nullptr;
  std::vector<IoBoundarySpace> boundary_spaces_;
};

}

#endif //DALI_PLACER_IOPLACER_IOPLACER_H_
