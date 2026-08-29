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
#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"
#include "io_boundary_space.h"

namespace dali {

/** Summary produced by the lightweight I/O placement signoff check. */
struct IoPlacementCheckResult {
  int unplaced_pins = 0;
  int missing_geometry = 0;
  int outside_die = 0;
  int overlapping_pairs = 0;
  int spacing_violation_pairs = 0;

  /** Return the total number of reported placement violations. */
  int ViolationCount() const;
  /** Return true when no placement violation was found. */
  bool IsLegal() const;
};

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

  /**
   * The database this placer will use.
   *
   * Exposed so a rebind can be proven to have reached it, rather than assumed
   * from the fact that SetPhyDB was called somewhere.
   */
  const phydb::PhyDB* PhyDBPtr() const { return phy_db_ptr_; }

  /** Place configured subset of I/O pins. */
  bool PartialPlaceIoPin();

  /** Parse and run partial I/O placement command. */
  bool PartialPlaceCmd(int argc, char** argv);

  /**
   * Move a pin while preserving its current layer and shape.
   *
   * The new location is expressed in microns. The pin becomes FIXED so later
   * automatic placement cannot silently overwrite a reviewed location.
   */
  bool MoveIoPin(std::string const& pin_name, double x, double y,
                 ComponentOrient orient);
  /** Parse `move-io <pin> <x> <y> [orient]`. */
  bool MoveIoPinCmd(int argc, char** argv);

  /** Mark a pin UNPLACED so a later automatic I/O pass may place it again. */
  bool UnfixIoPin(std::string const& pin_name);
  /** Parse `unfix-io <pin>`. */
  bool UnfixIoPinCmd(int argc, char** argv);

  /** Log one named pin, or all pins when pin_name is empty. */
  bool ShowIoPins(std::string const& pin_name = "") const;
  /** Parse `show-io [pin]`. */
  bool ShowIoPinsCmd(int argc, char** argv) const;

  /**
   * Check basic placement legality without replacing foundry signoff DRC.
   *
   * Checks placement status, geometry, die containment, same-layer overlap,
   * and the layer's scalar minimum spacing.
   */
  IoPlacementCheckResult CheckIoPlacement() const;
  /** Parse and run `check-io`, returning false when violations are found. */
  bool CheckIoPlacementCmd(int argc, char** argv) const;

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

  /** Return true when at least one pin or direction has an edge constraint. */
  bool HasEdgeConstraints() const;

  /**
   * Spread constrained unplaced pins on their requested edges before global
   * placement so their existing nets provide directional anchors.
   */
  bool PlaceConstrainedPinsForGlobalPlacement();

  /**
   * Place every movable I/O pin on an interior area-array grid.
   *
   * The flip-chip / area-I/O model: instead of the four perimeter edges, pins
   * go on a rows x cols lattice of sites strictly inside the placement region,
   * each pin assigned to the free site nearest its net's bounding-box center.
   * @param metal_layer layer the interior pins are drawn on.
   * @param rows number of interior lattice rows (> 0).
   * @param cols number of interior lattice columns (> 0).
   * @return false if rows*cols cannot hold every movable pin.
   */
  bool AreaArrayPlace(MetalLayer* metal_layer, int rows, int cols);
  /** Parse and run a `place-io -area <metal> <rows> <cols>` command. */
  bool AreaArrayPlaceCmd(int argc, char** argv);

  /**
   * Fix a group of pins as a contiguous run along one boundary.
   *
   * The pins are placed adjacent to each other, spaced by one pin pitch, and
   * placed in the free interval nearest the group's average net bounding-box
   * center along that edge. Each is marked fixed, so nothing separates them
   * afterwards. Guarantees adjacency the uniform boundary legalizer cannot,
   * since it interleaves pins by net-center position.
   * @param metal_layer layer the group is drawn on.
   * @param boundary_index LEFT/RIGHT/BOTTOM/TOP edge to place the run on.
   * @param pin_names group members, placed in the given order along the edge.
   * @return false if the edge is invalid, a pin is repeated or missing, or no
   *   legal contiguous interval can hold the group.
   */
  bool GroupPlace(MetalLayer* metal_layer, int boundary_index,
                  std::vector<std::string> const& pin_names);
  /** Parse and run `place-io -group <metal> <edge> <pin>...`. */
  bool GroupPlaceCmd(int argc, char** argv);

  /**
   * Fix a pin at the mirror image of a reference pin across a die center axis.
   *
   * The reference must already have a location. The new pin copies the
   * reference's layer and shape, takes the reflected location, and takes the
   * orientation that reflects the reference's geometry so the mirrored shape
   * still points into the die.
   * @param pin_name pin to place.
   * @param ref_pin_name already-placed reference pin.
   * @param axis 'x' reflects across the vertical center line (x changes); 'y'
   *   reflects across the horizontal center line (y changes).
   * @return false if either pin is missing or the reference has no location.
   */
  bool MirrorPlace(std::string const& pin_name, std::string const& ref_pin_name,
                   char axis);
  /** Parse and run `place-io -mirror <pin> <ref_pin> <x|y>`. */
  bool MirrorPlaceCmd(int argc, char** argv);

  /** Convert final I/O locations to PhyDB/database units. */
  void AdjustIoPinLocationForPhyDB();

  /** Run automatic I/O pin placement. */
  bool RunAutoPlacement();

  /** Parse and run automatic I/O placement command. */
  bool AutoPlaceCmd(int argc, char** argv);

 private:
  /**
   * Fix one pin at an explicit location on a layer, like a DEF pre-placed pin.
   *
   * Sets the in-memory pin FIXED and writes the same geometry, status, and
   * orientation into PhyDB, so both the auto-placer and the standard export
   * skip it. Shape corners are microns; the location is in Dali grid units.
   */
  void FixIoPin(IoPin* pin, MetalLayer* layer, double lx, double ly, double ux,
                double uy, double dali_x, double dali_y,
                ComponentOrient orient);

  /** Convert a boundary name to LEFT/RIGHT/BOTTOM/TOP, or -1 if invalid. */
  static int BoundaryNameToIndex(std::string const& name);

  /** Reflect an orientation when its x coordinate is mirrored. */
  static ComponentOrient ReflectOrientationAcrossVerticalCenterline(
      ComponentOrient orient);

  /** Reflect an orientation when its y coordinate is mirrored. */
  static ComponentOrient ReflectOrientationAcrossHorizontalCenterline(
      ComponentOrient orient);

  /**
   * Return a placed pin's occupied interval along its boundary in grid units.
   *
   * Pin shapes are stored in microns while pin locations use Dali grid units;
   * this helper performs that conversion after applying the pin orientation.
   */
  std::pair<double, double> BoundaryRunBounds(IoPin const& pin,
                                              bool vertical_edge) const;

  /** Return true when PhyDB places the pin on the selected declared die edge.
   */
  bool IsPinOnBoundary(IoPin const& pin, int boundary_index) const;

  Circuit* circuit_ = nullptr;
  phydb::PhyDB* phy_db_ptr_ = nullptr;
  std::vector<IoBoundarySpace> boundary_spaces_;
  // Optional edge constraints. A pin listed here, or (failing that) a pin whose
  // signal direction is listed, is forced to that boundary index rather than
  // the automatically chosen closest one.
  std::unordered_map<std::string, int> pin_edge_constraint_;
  std::unordered_map<int, int> direction_edge_constraint_;
  int global_metal_layer_index_ = -1;
};

}  // namespace dali

#endif  // DALI_PLACER_IO_PLACER_IO_PLACER_H_
