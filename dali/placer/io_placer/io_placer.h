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
#ifndef DALI_PLACER_IO_PLACER_IO_PLACER_H_
#define DALI_PLACER_IO_PLACER_IO_PLACER_H_

#include <phydb/phydb.h>

#include <unordered_map>
#include <vector>

#include "dali/circuit/circuit.h"
#include "io_boundary_space.h"

namespace dali {

/** Places I/O pins manually before placement or automatically after placement.
 */
class IoPlacer {
 public:
  /** Places I/O pins along the placement boundary. */
  IoPlacer();
  /** Construct bound to a PhyDB and circuit. */
  explicit IoPlacer(phydb::PhyDB* phy_db, Circuit* circuit);

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
  bool SetGlobalMetalLayer(int metal_layer_index);

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

  /** Force a named pin onto a boundary (LEFT/RIGHT/BOTTOM/TOP). */
  bool ConstrainPinToEdge(std::string const& pin_name, int boundary_index);
  /** Force all pins of a signal direction onto a boundary. */
  bool ConstrainDirectionToEdge(SignalDirection direction, int boundary_index);
  /** The constrained boundary for a pin, or -1 if unconstrained. */
  int ConstrainedEdge(IoPin const& iopin) const;
  /** Parse and apply a `place-io -constraint ...` command. */
  bool ConstraintCmd(int argc, char** argv);

  /**
   * Place every movable I/O pin on an interior area-array grid.
   *
   * The flip-chip / area-I/O model: instead of the four perimeter edges, pins go
   * on a rows x cols lattice of sites strictly inside the placement region, each
   * pin assigned to the free site nearest its net's bounding-box center.
   * @param metal_layer layer the interior pins are drawn on.
   * @param rows number of interior lattice rows (> 0).
   * @param cols number of interior lattice columns (> 0).
   * @return false if rows*cols cannot hold every movable pin.
   */
  bool AreaArrayPlace(MetalLayer *metal_layer, int rows, int cols);
  /** Parse and run a `place-io -area <metal> <rows> <cols>` command. */
  bool AreaArrayPlaceCmd(int argc, char **argv);

  /** Convert final I/O locations to PhyDB/database units. */
  void AdjustIoPinLocationForPhyDB();

  /** Run automatic I/O pin placement. */
  bool RunAutoPlacement();

  /** Parse and run automatic I/O placement command. */
  bool AutoPlaceCmd(int argc, char** argv);

 private:
  Circuit* circuit_ = nullptr;
  phydb::PhyDB* phy_db_ptr_ = nullptr;
  std::vector<IoBoundarySpace> boundary_spaces_;
  // Optional edge constraints. A pin listed here, or (failing that) a pin whose
  // signal direction is listed, is forced to that boundary index rather than
  // the automatically chosen closest one.
  std::unordered_map<std::string, int> pin_edge_constraint_;
  std::unordered_map<int, int> direction_edge_constraint_;
};

}  // namespace dali

#endif  // DALI_PLACER_IO_PLACER_IO_PLACER_H_
