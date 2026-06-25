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

#include <phydb/phydb.h>

#include <vector>

#include "dali/circuit/circuit.h"
#include "ioboundaryspace.h"

namespace dali {

/** Places I/O pins manually before placement or automatically after placement.
 */
class IoPlacer {
 public:
  IoPlacer();
  explicit IoPlacer(phydb::PhyDB* phy_db, Circuit* circuit_);

  /** Create boundary-space containers for the current circuit. */
  void InitializeBoundarySpaces();

  /** Attach the circuit whose I/O pins are placed. */
  void SetCircuit(Circuit* circuit);

  /** Attach the PhyDB instance receiving final I/O pin locations. */
  void SetPhyDB(phydb::PhyDB* phy_db_ptr);

  /** Place configured subset of I/O pins. */
  bool PartialPlaceIoPin();

  /** Parse and run partial I/O placement command. */
  bool PartialPlaceCmd(int argc, char** argv);

  /** Set the metal layer for one boundary. */
  bool ConfigSetMetalLayer(int boundary_index, int metal_layer_index);

  /** Set the metal layer for all boundaries. */
  bool ConfigSetGlobalMetalLayer(int metal_layer_index);

  /** Enable automatic I/O pin placement. */
  bool ConfigAutoPlace();

  /** Configure boundary metal choices from command arguments. */
  bool ConfigBoundaryMetal(int argc, char** argv);

  /** Log I/O placer configuration usage. */
  static void ReportConfigUsage();

  /** Parse and apply I/O placer configuration command. */
  bool ConfigCmd(int argc, char** argv);

  /** Validate I/O placer configuration before placement. */
  bool CheckConfiguration();

  /** Build the available boundary/layer resource map. */
  bool BuildResourceMap();

  /** Assign each I/O pin to a boundary layer. */
  bool AssignIoPinToBoundaryLayers();

  /** Legalize and place pins on each configured boundary. */
  bool PlaceIoPinOnEachBoundary();

  /** Convert final I/O locations to PhyDB/database units. */
  void AdjustIoPinLocationForPhyDB();

  /** Run automatic I/O pin placement. */
  bool AutoPlaceIoPin();

  /** Parse and run automatic I/O placement command. */
  bool AutoPlaceCmd(int argc, char** argv);

 private:
  Circuit* p_ckt_ = nullptr;
  phydb::PhyDB* phy_db_ptr_ = nullptr;
  std::vector<IoBoundarySpace> boundary_spaces_;
};

}  // namespace dali

#endif  // DALI_PLACER_IOPLACER_IOPLACER_H_
