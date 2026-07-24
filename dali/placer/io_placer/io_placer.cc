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
 * Places I/O pins along the placement boundary.
 *
 * A pin is assigned to a boundary and a position on it, on a metal layer the
 * technology allows, spaced so pins do not collide. Runs after component
 * placement, so pin positions can follow the nets that reach them; pins already
 * marked fixed keep their positions.
 */
#include "io_placer.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/phydb_helper.h"

#define NUM_OF_PLACE_BOUNDARY 4
#define LEFT 0
#define RIGHT 1
#define BOTTOM 2
#define TOP 3

namespace dali {

IoPlacer::IoPlacer() { InitializeBoundarySpaces(); }

IoPlacer::IoPlacer(phydb::PhyDB* phy_db, Circuit* circuit) {
  SetPhyDB(phy_db);
  SetCircuit(circuit);
  InitializeBoundarySpaces();
}

void IoPlacer::InitializeBoundarySpaces() {
  boundary_spaces_.reserve(NUM_OF_PLACE_BOUNDARY);
  // put all boundaries in a vector
  std::vector<double> boundary_loc{(double)circuit_->design().RegionLeft(),
                                   (double)circuit_->design().RegionRight(),
                                   (double)circuit_->design().RegionBottom(),
                                   (double)circuit_->design().RegionTop()};

  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    boundary_spaces_.emplace_back(i == BOTTOM || i == TOP, boundary_loc[i]);
    boundary_spaces_.back().manufacturing_grid_ =
        phy_db_ptr_->tech().GetManufacturingGrid();
  }
}

void IoPlacer::SetCircuit(Circuit* circuit) {
  DaliExpects(circuit != nullptr,
              "Cannot initialize an IoPlacer without providing a valid Circuit "
              "pointer");
  circuit_ = circuit;
}

void IoPlacer::SetPhyDB(phydb::PhyDB* phy_db_ptr) {
  DaliExpects(
      phy_db_ptr != nullptr,
      "Cannot initialize an IoPlacer without providing a valid PhyDB pointer");
  phy_db_ptr_ = phy_db_ptr;
}

bool IoPlacer::PartialPlaceIoPin() {
  DaliExpects(false, "to be implemented");
  return true;
}

bool IoPlacer::PartialPlaceCmd(int argc, char** argv) {
  // -place <pin> <metal> <lx> <ly> <ux> <uy> <x> <y> <orient>
  // All coordinates are in microns. The pin is fixed at the given location on
  // the given layer -- which may be anywhere, interior included -- and later
  // auto-placement leaves it untouched.
  if (argc != 9) {
    LOG(error) << "place-io -place needs 9 arguments: <pin> <metal> <lx> <ly> "
                  "<ux> <uy> <x> <y> <orient>\n";
    return false;
  }
  std::string pin_name(argv[0]);
  std::string metal_name(argv[1]);
  if (!circuit_->IsIoPinExisting(pin_name)) {
    LOG(error) << "No such I/O pin: " << pin_name << "\n";
    return false;
  }
  if (!circuit_->IsMetalLayerExisting(metal_name)) {
    LOG(error) << "No such metal layer: " << metal_name << "\n";
    return false;
  }
  double lx, ly, ux, uy, x, y;
  try {
    lx = std::stod(argv[2]);
    ly = std::stod(argv[3]);
    ux = std::stod(argv[4]);
    uy = std::stod(argv[5]);
    x = std::stod(argv[6]);
    y = std::stod(argv[7]);
  } catch (...) {
    LOG(error) << "place-io -place coordinates must be numbers\n";
    return false;
  }
  ComponentOrient orient = StrToOrient(std::string(argv[8]));

  IoPin* pin = circuit_->GetIoPinPtr(pin_name);
  MetalLayer* metal_layer = circuit_->GetMetalLayerPtr(metal_name);
  double dali_x = circuit_->LocPhydb2DaliX(circuit_->Micron2DatabaseUnit(x));
  double dali_y = circuit_->LocPhydb2DaliY(circuit_->Micron2DatabaseUnit(y));
  FixIoPin(pin, metal_layer, lx, ly, ux, uy, dali_x, dali_y, orient);
  LOG(info) << "Fixed I/O pin " << pin_name << " at (" << x << ", " << y
            << ") on " << metal_name << "\n";
  return true;
}

bool IoPlacer::ConfigSetMetalLayer(int boundary_index, int metal_layer_index) {
  bool is_legal_index = (metal_layer_index >= 0) &&
                        (metal_layer_index < (int)circuit_->Metals().size());
  if (!is_legal_index) {
    LOG(info) << "metal layer index is a bad value: " << metal_layer_index
              << "\n";
    return false;
  }
  MetalLayer* metal_layer = &(circuit_->Metals()[metal_layer_index]);
  boundary_spaces_[boundary_index].AddLayer(metal_layer);
  return true;
}

bool IoPlacer::SetGlobalMetalLayer(int metal_layer_index) {
  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    bool is_successful = ConfigSetMetalLayer(i, metal_layer_index);
    if (!is_successful) {
      return false;
    }
  }
  return true;
}

bool IoPlacer::ConfigAutoPlace() { return true; }

/**
 * Configure which metal layers each boundary may place pins on, from argv.
 * @return false if the arguments are malformed.
 */
bool IoPlacer::ConfigBoundaryMetal(int argc, char** argv) {
  if (argc < 2) {
    ReportConfigUsage();
    return false;
  }
  for (int i = 0; i < argc;) {
    std::string arg(argv[i++]);
    if (i < argc) {
      std::string metal_name = std::string(argv[i++]);
      bool is_layer_existing = circuit_->IsMetalLayerExisting(metal_name);
      if (!is_layer_existing) {
        LOG(fatal) << "Invalid metal layer name!\n";
        ReportConfigUsage();
        return false;
      }
      MetalLayer* metal_layer = circuit_->GetMetalLayerPtr(metal_name);
      int metal_index = metal_layer->Id();
      if (arg == "left") {
        bool is_success = ConfigSetMetalLayer(LEFT, metal_index);
        std::cout << is_success << "\n";
      } else if (arg == "right") {
        bool is_success = ConfigSetMetalLayer(RIGHT, metal_index);
        std::cout << is_success << "\n";
      } else if (arg == "bottom") {
        bool is_success = ConfigSetMetalLayer(BOTTOM, metal_index);
        std::cout << is_success << "\n";
      } else if (arg == "top") {
        bool is_success = ConfigSetMetalLayer(TOP, metal_index);
        std::cout << is_success << "\n";
      } else {
        LOG(fatal)
            << "Invalid boundary, possible values: left, right, bottom, top\n";
        ReportConfigUsage();
        return false;
      }
      std::cout << arg << "  " << metal_name << "\n";
    } else {
      LOG(fatal) << "Boundary specified, but metal layer is not given\n";
      ReportConfigUsage();
      return false;
    }
  }
  return true;
}

void IoPlacer::ReportConfigUsage() {
  LOG(info)
      << "\033[0;36m"
      << "Usage: place-io -c/--config\n"
      << "  -h/--help\n"
      << "      print out function usage\n"
      << "  -m/--metal <left/right/bottom/top> <metal layer>\n"
      << "      use this command to specify which metal layers to use for "
         "IOPINs on each placement boundary\n"
      << "      example: -m left m1, for IOPINs on the left boundary, using "
         "layer m1 to create physical geometry\n"
      << "      'place-io <metal layer>' is a shorthand for 'place-io -c -m "
         "left m1 right m1 bottom m1 top m1'\n"
      << "\033[0m\n";
}

bool IoPlacer::ConfigCmd(int argc, char** argv) {
  if (argc < 1) {
    ReportConfigUsage();
    return false;
  }

  std::string option_str(argv[0]);
  if (option_str == "-h" or option_str == "--help") {
    ReportConfigUsage();
    return true;
  } else if (option_str == "-m" or option_str == "--metal") {
    return ConfigBoundaryMetal(argc - 1, argv + 1);
  } else {
    bool is_metal_name = circuit_->IsMetalLayerExisting(option_str);
    // when the command is like 'place-io <metal layer>'
    if (is_metal_name) {
      MetalLayer* metal_layer = circuit_->GetMetalLayerPtr(option_str);
      return SetGlobalMetalLayer(metal_layer->Id());
    }
    LOG(fatal) << "Unknown flag: " << option_str << "\n";
    ReportConfigUsage();
    return false;
  }
}

// resource should be large enough for all IOPINs
bool IoPlacer::CheckConfiguration() {
  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    if (boundary_spaces_[i].layer_spaces_.empty()) {
      LOG(error) << "No metal layer configured for I/O placement boundary " << i
                 << "\n";
      ReportConfigUsage();
      return false;
    }
  }
  return true;
}

/**
 * Build the free-space map along every boundary and layer.
 *
 * Records where pins may go once existing fixed pins and blockages are removed,
 * so assignment has a resource picture to place into.
 * @return false if no usable boundary space exists.
 */
bool IoPlacer::BuildResourceMap() {
  std::vector<std::vector<Seg<double>>> all_used_segments(
      NUM_OF_PLACE_BOUNDARY,
      std::vector<Seg<double>>(0));  // TODO: carry layer info
  for (auto& iopin : circuit_->IoPins()) {
    if (iopin.IsPrePlaced()) {
      if (!iopin.IsShapeSet() || iopin.LayerPtr() == nullptr) {
        LOG(error) << "Pre-placed I/O pin " << iopin.Name()
                   << " is missing its layer or shape\n";
        return false;
      }
      double spacing = iopin.LayerPtr()->Spacing();
      if (IsPinOnBoundary(iopin, LEFT)) {
        auto bounds = BoundaryRunBounds(iopin, true);
        double spacing_in_grid = spacing / circuit_->GridValueY();
        all_used_segments[LEFT].emplace_back(bounds.first - spacing_in_grid,
                                             bounds.second + spacing_in_grid);
      } else if (IsPinOnBoundary(iopin, RIGHT)) {
        auto bounds = BoundaryRunBounds(iopin, true);
        double spacing_in_grid = spacing / circuit_->GridValueY();
        all_used_segments[RIGHT].emplace_back(bounds.first - spacing_in_grid,
                                              bounds.second + spacing_in_grid);
      } else if (IsPinOnBoundary(iopin, BOTTOM)) {
        auto bounds = BoundaryRunBounds(iopin, false);
        double spacing_in_grid = spacing / circuit_->GridValueX();
        all_used_segments[BOTTOM].emplace_back(bounds.first - spacing_in_grid,
                                               bounds.second + spacing_in_grid);
      } else if (IsPinOnBoundary(iopin, TOP)) {
        auto bounds = BoundaryRunBounds(iopin, false);
        double spacing_in_grid = spacing / circuit_->GridValueX();
        all_used_segments[TOP].emplace_back(bounds.first - spacing_in_grid,
                                            bounds.second + spacing_in_grid);
      } else {
        // A fixed pin placed in the interior (e.g. via `place-io -place`)
        // consumes no boundary resource, so it is simply left out of the
        // boundary-usage accounting rather than treated as an error.
      }
    }
  }

  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    for (auto& layer_space : boundary_spaces_[i].layer_spaces_) {
      layer_space.iopin_ptr_list.clear();
      layer_space.pin_clusters.clear();
    }

    std::vector<Seg<double>>& used_segments = all_used_segments[i];
    std::sort(used_segments.begin(), used_segments.end(),
              [](const Seg<double>& lhs, const Seg<double>& rhs) {
                return (lhs.lo < rhs.lo);
              });

    double region_lo = (i == LEFT || i == RIGHT)
                           ? circuit_->design().RegionBottom()
                           : circuit_->design().RegionLeft();
    double region_hi = (i == LEFT || i == RIGHT)
                           ? circuit_->design().RegionTop()
                           : circuit_->design().RegionRight();
    double cursor = region_lo;
    for (auto const& used : used_segments) {
      double used_lo = std::max(region_lo, used.lo);
      double used_hi = std::min(region_hi, used.hi);
      if (used_hi <= cursor) {
        continue;
      }
      if (used_lo > cursor) {
        boundary_spaces_[i].layer_spaces_[0].AddCluster(cursor,
                                                        used_lo - cursor);
      }
      cursor = std::max(cursor, used_hi);
    }
    if (cursor < region_hi) {
      boundary_spaces_[i].layer_spaces_[0].AddCluster(cursor,
                                                      region_hi - cursor);
    }
  }
  return true;
}

int IoPlacer::BoundaryNameToIndex(std::string const& name) {
  if (name == "left") return LEFT;
  if (name == "right") return RIGHT;
  if (name == "bottom") return BOTTOM;
  if (name == "top") return TOP;
  return -1;
}

ComponentOrient IoPlacer::ReflectOrientationAcrossVerticalCenterline(
    ComponentOrient orient) {
  switch (orient) {
    case N:
      return FN;
    case FN:
      return N;
    case S:
      return FS;
    case FS:
      return S;
    case E:
      return FE;
    case FE:
      return E;
    case W:
      return FW;
    case FW:
      return W;
    default:
      return orient;
  }
}

ComponentOrient IoPlacer::ReflectOrientationAcrossHorizontalCenterline(
    ComponentOrient orient) {
  switch (orient) {
    case N:
      return FS;
    case FS:
      return N;
    case S:
      return FN;
    case FN:
      return S;
    case E:
      return FW;
    case FW:
      return E;
    case W:
      return FE;
    case FE:
      return W;
    default:
      return orient;
  }
}

std::pair<double, double> IoPlacer::BoundaryRunBounds(
    IoPin const& pin, bool vertical_edge) const {
  if (vertical_edge) {
    double grid_value = circuit_->GridValueY();
    return {pin.Y() + (pin.LY() - pin.Y()) / grid_value,
            pin.Y() + (pin.UY() - pin.Y()) / grid_value};
  }
  double grid_value = circuit_->GridValueX();
  return {pin.X() + (pin.LX() - pin.X()) / grid_value,
          pin.X() + (pin.UX() - pin.X()) / grid_value};
}

bool IoPlacer::IsPinOnBoundary(IoPin const& pin, int boundary_index) const {
  phydb::IOPin* phydb_pin = phy_db_ptr_->GetIoPinPtr(pin.Name());
  if (phydb_pin == nullptr) {
    return false;
  }
  auto location = phydb_pin->GetLocation();
  const auto& die_area = phy_db_ptr_->GetDesignPtr()->GetDieArea();
  if (boundary_index == LEFT) {
    return location.x == die_area.LLX();
  }
  if (boundary_index == RIGHT) {
    return location.x == die_area.URX();
  }
  if (boundary_index == BOTTOM) {
    return location.y == die_area.LLY();
  }
  if (boundary_index == TOP) {
    return location.y == die_area.URY();
  }
  return false;
}

bool IoPlacer::ConstrainPinToEdge(std::string const& pin_name,
                                  int boundary_index) {
  if (!circuit_->IsIoPinExisting(pin_name)) {
    LOG(error) << "No such I/O pin: " << pin_name << "\n";
    return false;
  }
  pin_edge_constraint_[pin_name] = boundary_index;
  return true;
}

bool IoPlacer::ConstrainDirectionToEdge(SignalDirection direction,
                                        int boundary_index) {
  direction_edge_constraint_[direction] = boundary_index;
  return true;
}

int IoPlacer::ConstrainedEdge(IoPin const& iopin) const {
  auto by_pin = pin_edge_constraint_.find(iopin.Name());
  if (by_pin != pin_edge_constraint_.end()) return by_pin->second;
  auto by_dir = direction_edge_constraint_.find(iopin.SigDirection());
  if (by_dir != direction_edge_constraint_.end()) return by_dir->second;
  return -1;
}

bool IoPlacer::ConstraintCmd(int argc, char** argv) {
  // -constraint <pin_name | dir:input|output|inout> <left|right|bottom|top>
  if (argc != 2) {
    LOG(error) << "place-io -constraint needs 2 arguments: <pin|dir:DIR> "
                  "<left|right|bottom|top>\n";
    return false;
  }
  std::string target(argv[0]);
  int boundary = BoundaryNameToIndex(std::string(argv[1]));
  if (boundary < 0) {
    LOG(error) << "Unknown edge: " << argv[1]
               << " (use left/right/bottom/top)\n";
    return false;
  }
  const std::string dir_prefix = "dir:";
  if (target.rfind(dir_prefix, 0) == 0) {
    SignalDirection dir =
        StrToSignalDirection(target.substr(dir_prefix.size()));
    return ConstrainDirectionToEdge(dir, boundary);
  }
  return ConstrainPinToEdge(target, boundary);
}

bool IoPlacer::AreaArrayPlace(MetalLayer* metal_layer, int rows, int cols) {
  if (rows <= 0 || cols <= 0) {
    LOG(error) << "Area-array grid needs positive rows and cols\n";
    return false;
  }

  std::vector<IoPin*> movable;
  for (auto& iopin : circuit_->IoPins()) {
    if (iopin.IsPrePlaced()) continue;
    Net* net = iopin.NetPtr();
    if (net == nullptr || net->ComponentPins().empty()) {
      LOG(warning) << "I/O pin " << iopin.Name()
                   << " has no connected component, skip area-array placing\n";
      continue;
    }
    movable.push_back(&iopin);
  }

  int capacity = rows * cols;
  if (static_cast<int>(movable.size()) > capacity) {
    LOG(error) << "Area-array grid " << rows << "x" << cols << " = " << capacity
               << " sites cannot hold " << movable.size() << " I/O pins\n";
    return false;
  }

  double left = circuit_->design().RegionLeft();
  double right = circuit_->design().RegionRight();
  double bottom = circuit_->design().RegionBottom();
  double top = circuit_->design().RegionTop();
  struct Site {
    double x, y;
    bool used;
  };
  std::vector<Site> sites;
  sites.reserve(capacity);
  for (int r = 1; r <= rows; ++r) {
    double gy = bottom + (top - bottom) * r / (rows + 1);
    for (int c = 1; c <= cols; ++c) {
      double gx = left + (right - left) * c / (cols + 1);
      sites.push_back({gx, gy, false});
    }
  }

  // A manufacturing-grid-aligned pin geometry, same recipe as the boundary
  // placer's vertical default shape, so interior pins are manufacturable too.
  double mfg = phy_db_ptr_->tech().GetManufacturingGrid();
  double width = metal_layer->Width();
  double height = std::max(metal_layer->MinArea() / width, width);
  height = RoundOrCeiling(height / mfg) * mfg;
  double half_width = RoundOrCeiling(width / 2.0 / mfg) * mfg;

  for (IoPin* pin : movable) {
    Net* net = pin->NetPtr();
    net->UpdateMaxMinIndex();
    double cx = 0.5 * (net->MinX() + net->MaxX());
    double cy = 0.5 * (net->MinY() + net->MaxY());
    int best = -1;
    double best_dist = 0;
    for (int s = 0; s < static_cast<int>(sites.size()); ++s) {
      if (sites[s].used) continue;
      double dx = sites[s].x - cx;
      double dy = sites[s].y - cy;
      double dist = dx * dx + dy * dy;
      if (best < 0 || dist < best_dist) {
        best = s;
        best_dist = dist;
      }
    }
    sites[best].used = true;
    pin->SetLayerPtr(metal_layer);
    pin->SetShape(-half_width, 0, half_width, height);
    pin->SetOrient(N);
    pin->SetLoc(sites[best].x, sites[best].y, PLACED);
    pin->SetFinalX(circuit_->LocDali2PhydbX(sites[best].x));
    pin->SetFinalY(circuit_->LocDali2PhydbY(sites[best].y));
  }
  return true;
}

bool IoPlacer::AreaArrayPlaceCmd(int argc, char** argv) {
  // -area <metal> <rows> <cols>
  if (argc != 3) {
    LOG(error) << "place-io -area needs 3 arguments: <metal> <rows> <cols>\n";
    return false;
  }
  std::string metal_name(argv[0]);
  if (!circuit_->IsMetalLayerExisting(metal_name)) {
    LOG(error) << "No such metal layer: " << metal_name << "\n";
    return false;
  }
  int rows, cols;
  try {
    rows = std::stoi(argv[1]);
    cols = std::stoi(argv[2]);
  } catch (...) {
    LOG(error) << "place-io -area rows and cols must be integers\n";
    return false;
  }
  return AreaArrayPlace(circuit_->GetMetalLayerPtr(metal_name), rows, cols);
}

void IoPlacer::FixIoPin(IoPin* pin, MetalLayer* layer, double lx, double ly,
                        double ux, double uy, double dali_x, double dali_y,
                        ComponentOrient orient) {
  pin->SetLayerPtr(layer);
  pin->SetShape(lx, ly, ux, uy);
  pin->SetOrient(orient);
  pin->SetLoc(dali_x, dali_y, FIXED);
  pin->SetInitPlaceStatus(FIXED);
  int db_x = circuit_->LocDali2PhydbX(dali_x);
  int db_y = circuit_->LocDali2PhydbY(dali_y);
  // On the right/top boundary the die extent may not be an integer grid
  // multiple; snap to the exact die edge, as AdjustIoPinLocationForPhyDB does.
  const auto& die_area = phy_db_ptr_->GetDesignPtr()->GetDieArea();
  if (dali_x == circuit_->RegionURX()) db_x = die_area.URX();
  if (dali_y == circuit_->RegionURY()) db_y = die_area.URY();
  pin->SetFinalX(db_x);
  pin->SetFinalY(db_y);
  phydb::IOPin* phydb_pin = phy_db_ptr_->GetIoPinPtr(pin->Name());
  phydb_pin->SetShape(layer->Name(), circuit_->Micron2DatabaseUnit(lx),
                      circuit_->Micron2DatabaseUnit(ly),
                      circuit_->Micron2DatabaseUnit(ux),
                      circuit_->Micron2DatabaseUnit(uy));
  phydb_pin->SetPlacement(phydb::PlaceStatus::FIXED, db_x, db_y,
                          OrientDali2PhyDB(orient));
}

bool IoPlacer::GroupPlace(MetalLayer* metal_layer, int boundary_index,
                          std::vector<std::string> const& pin_names) {
  if (metal_layer == nullptr) {
    LOG(error) << "Cannot place an I/O pin group without a metal layer\n";
    return false;
  }
  if (boundary_index < LEFT || boundary_index > TOP) {
    LOG(error) << "Invalid I/O pin group boundary index: " << boundary_index
               << "\n";
    return false;
  }

  std::unordered_set<std::string> unique_names;
  for (auto const& name : pin_names) {
    if (!circuit_->IsIoPinExisting(name)) {
      LOG(error) << "No such I/O pin: " << name << "\n";
      return false;
    }
    if (!unique_names.insert(name).second) {
      LOG(error) << "I/O pin " << name
                 << " appears more than once in the group\n";
      return false;
    }
  }
  int n = static_cast<int>(pin_names.size());
  if (n == 0) return true;

  double mfg = phy_db_ptr_->tech().GetManufacturingGrid();
  double width = metal_layer->Width();
  double height = std::max(metal_layer->MinArea() / width, width);
  height = RoundOrCeiling(height / mfg) * mfg;
  double half_width = RoundOrCeiling(width / 2.0 / mfg) * mfg;

  bool vertical_edge = (boundary_index == LEFT || boundary_index == RIGHT);
  double left = circuit_->design().RegionLeft();
  double right = circuit_->design().RegionRight();
  double bottom = circuit_->design().RegionBottom();
  double top = circuit_->design().RegionTop();

  // MetalLayer::Spacing() is Dali's normalized LEF minimum spacing, including
  // the first entry of a spacing table when no scalar SPACING value exists. A
  // manufacturing-grid margin keeps exported integer database coordinates
  // strictly beyond that minimum after rounding.
  double spacing = metal_layer->Spacing();
  double footprint = 2.0 * half_width;
  double grid_value =
      vertical_edge ? circuit_->GridValueY() : circuit_->GridValueX();
  double pitch = (footprint + spacing + mfg) / grid_value;

  double sum = 0;
  int cnt = 0;
  for (auto const& name : pin_names) {
    IoPin* pin = circuit_->GetIoPinPtr(name);
    Net* net = pin->NetPtr();
    if (net == nullptr || net->ComponentPins().empty()) continue;
    net->UpdateMaxMinIndex();
    double center = vertical_edge ? 0.5 * (net->MinY() + net->MaxY())
                                  : 0.5 * (net->MinX() + net->MaxX());
    sum += center;
    ++cnt;
  }
  double region_lo = vertical_edge ? bottom : left;
  double region_hi = vertical_edge ? top : right;
  double center = (cnt > 0) ? sum / cnt : 0.5 * (region_lo + region_hi);

  double total = (n - 1) * pitch;
  double edge_coord;
  ComponentOrient orient;
  if (boundary_index == LEFT) {
    edge_coord = left;
    orient = E;
  } else if (boundary_index == RIGHT) {
    edge_coord = right;
    orient = W;
  } else if (boundary_index == BOTTOM) {
    edge_coord = bottom;
    orient = N;
  } else {
    edge_coord = top;
    orient = S;
  }

  double run_half_extent = half_width / grid_value;
  double required_span = total + 2.0 * run_half_extent;
  if (required_span > region_hi - region_lo) {
    LOG(error) << "I/O pin group needs " << required_span
               << " grid units, but the selected edge only has "
               << region_hi - region_lo << "\n";
    return false;
  }

  // Existing fixed pins divide the edge into free intervals. Expand their
  // occupied intervals by the required clearance, then choose the legal group
  // interval closest to the net-driven target.
  double clearance = (spacing + mfg) / grid_value;
  std::vector<Seg<double>> blocked;
  for (auto const& iopin : circuit_->IoPins()) {
    if (unique_names.find(iopin.Name()) != unique_names.end()) {
      continue;
    }
    if (!iopin.IsPrePlaced() || !iopin.IsShapeSet() ||
        iopin.LayerPtr() == nullptr ||
        iopin.LayerPtr()->Name() != metal_layer->Name()) {
      continue;
    }
    if (!IsPinOnBoundary(iopin, boundary_index)) {
      continue;
    }
    auto bounds = BoundaryRunBounds(iopin, vertical_edge);
    blocked.emplace_back(bounds.first - clearance, bounds.second + clearance);
  }
  std::sort(blocked.begin(), blocked.end(),
            [](Seg<double> const& lhs, Seg<double> const& rhs) {
              return lhs.lo < rhs.lo;
            });

  double desired_start = center - total / 2.0;
  double desired_envelope_lo = desired_start - run_half_extent;
  double best_start = 0.0;
  double best_distance = 0.0;
  bool found_interval = false;
  auto consider_free_interval = [&](double free_lo, double free_hi) {
    if (free_hi - free_lo < required_span) {
      return;
    }
    double envelope_lo = std::max(
        free_lo, std::min(desired_envelope_lo, free_hi - required_span));
    double candidate_start = envelope_lo + run_half_extent;
    double distance = std::fabs(candidate_start - desired_start);
    if (!found_interval || distance < best_distance) {
      best_start = candidate_start;
      best_distance = distance;
      found_interval = true;
    }
  };

  double free_lo = region_lo;
  for (auto const& segment : blocked) {
    double blocked_lo = std::max(region_lo, segment.lo);
    double blocked_hi = std::min(region_hi, segment.hi);
    if (blocked_hi <= free_lo) {
      continue;
    }
    if (blocked_lo > free_lo) {
      consider_free_interval(free_lo, blocked_lo);
    }
    free_lo = std::max(free_lo, blocked_hi);
  }
  if (free_lo < region_hi) {
    consider_free_interval(free_lo, region_hi);
  }
  if (!found_interval) {
    LOG(error) << "No contiguous interval on the selected edge can hold the "
                  "I/O pin group\n";
    return false;
  }

  for (int i = 0; i < n; ++i) {
    double pos = best_start + i * pitch;
    IoPin* pin = circuit_->GetIoPinPtr(pin_names[i]);
    if (vertical_edge) {
      FixIoPin(pin, metal_layer, -half_width, 0, half_width, height, edge_coord,
               pos, orient);
    } else {
      FixIoPin(pin, metal_layer, -half_width, 0, half_width, height, pos,
               edge_coord, orient);
    }
  }
  return true;
}

bool IoPlacer::GroupPlaceCmd(int argc, char** argv) {
  // -group <metal> <edge> <pin1> <pin2> ...
  if (argc < 3) {
    LOG(error) << "place-io -group needs: <metal> <left|right|bottom|top> "
                  "<pin1> [pin2 ...]\n";
    return false;
  }
  std::string metal_name(argv[0]);
  if (!circuit_->IsMetalLayerExisting(metal_name)) {
    LOG(error) << "No such metal layer: " << metal_name << "\n";
    return false;
  }
  int boundary = BoundaryNameToIndex(std::string(argv[1]));
  if (boundary < 0) {
    LOG(error) << "Unknown edge: " << argv[1]
               << " (use left/right/bottom/top)\n";
    return false;
  }
  std::vector<std::string> pin_names;
  for (int i = 2; i < argc; ++i) pin_names.emplace_back(argv[i]);
  return GroupPlace(circuit_->GetMetalLayerPtr(metal_name), boundary,
                    pin_names);
}

bool IoPlacer::MirrorPlace(std::string const& pin_name,
                           std::string const& ref_pin_name, char axis) {
  if (axis != 'x' && axis != 'y') {
    LOG(error) << "I/O pin mirror axis must be x or y\n";
    return false;
  }
  if (pin_name == ref_pin_name) {
    LOG(error) << "An I/O pin cannot be mirrored onto itself: " << pin_name
               << "\n";
    return false;
  }
  if (!circuit_->IsIoPinExisting(pin_name)) {
    LOG(error) << "No such I/O pin: " << pin_name << "\n";
    return false;
  }
  if (!circuit_->IsIoPinExisting(ref_pin_name)) {
    LOG(error) << "No such reference I/O pin: " << ref_pin_name << "\n";
    return false;
  }
  IoPin* ref = circuit_->GetIoPinPtr(ref_pin_name);
  if (!ref->IsPlaced() && !ref->IsPrePlaced()) {
    LOG(error) << "Reference I/O pin " << ref_pin_name
               << " has no location to mirror\n";
    return false;
  }
  if (ref->LayerPtr() == nullptr) {
    LOG(error) << "Reference I/O pin " << ref_pin_name << " has no layer\n";
    return false;
  }
  if (!ref->IsShapeSet()) {
    LOG(error) << "Reference I/O pin " << ref_pin_name << " has no shape\n";
    return false;
  }

  const auto& die_area = phy_db_ptr_->GetDesignPtr()->GetDieArea();
  int ref_db_x;
  int ref_db_y;
  if (ref->IsPrePlaced()) {
    auto ref_location = phy_db_ptr_->GetIoPinPtr(ref_pin_name)->GetLocation();
    ref_db_x = ref_location.x;
    ref_db_y = ref_location.y;
  } else {
    ref_db_x = ref->FinalX();
    ref_db_y = ref->FinalY();
  }

  double new_x, new_y;
  ComponentOrient orient;
  if (axis == 'x') {
    int new_db_x = die_area.LLX() + die_area.URX() - ref_db_x;
    new_x = circuit_->LocPhydb2DaliX(new_db_x);
    new_y = circuit_->LocPhydb2DaliY(ref_db_y);
    orient = ReflectOrientationAcrossVerticalCenterline(ref->Orient());
  } else {
    int new_db_y = die_area.LLY() + die_area.URY() - ref_db_y;
    new_x = circuit_->LocPhydb2DaliX(ref_db_x);
    new_y = circuit_->LocPhydb2DaliY(new_db_y);
    orient = ReflectOrientationAcrossHorizontalCenterline(ref->Orient());
  }

  RectD const& shape = ref->Shape();
  IoPin* pin = circuit_->GetIoPinPtr(pin_name);
  FixIoPin(pin, ref->LayerPtr(), shape.LLX(), shape.LLY(), shape.URX(),
           shape.URY(), new_x, new_y, orient);
  return true;
}

bool IoPlacer::MirrorPlaceCmd(int argc, char** argv) {
  // -mirror <pin> <ref_pin> <x|y>
  if (argc != 3) {
    LOG(error) << "place-io -mirror needs 3 arguments: <pin> <ref_pin> <x|y>\n";
    return false;
  }
  std::string axis_str(argv[2]);
  if (axis_str != "x" && axis_str != "y") {
    LOG(error) << "place-io -mirror axis must be x or y\n";
    return false;
  }
  return MirrorPlace(std::string(argv[0]), std::string(argv[1]), axis_str[0]);
}

/**
 * Assign each movable I/O pin to a boundary, layer, and position.
 *
 * Pins already marked fixed keep their location; the rest are placed near the
 * nets that reach them, subject to the layer configuration and spacing.
 * @return false if some pin could not be placed.
 */
bool IoPlacer::AssignIoPinToBoundaryLayers() {
  for (auto& iopin : circuit_->IoPins()) {
    // do nothing for placed IOPINs
    if (iopin.IsPrePlaced()) continue;

    // find the bounding box of the net containing this IOPIN
    Net* net = iopin.NetPtr();
    if (net == nullptr || net->ComponentPins().empty()) {
      // if this net only contain this IOPIN, do nothing
      LOG(warning) << "I/O pin " << iopin.Name()
                   << " has no connected component; skip placement\n";
      continue;
    }
    net->UpdateMaxMinIndex();
    double net_minx = net->MinX();
    double net_maxx = net->MaxX();
    double net_miny = net->MinY();
    double net_maxy = net->MaxY();

    // placement boundary
    std::vector<double> distance_to_boundary{
        net_minx - circuit_->design().RegionLeft(),
        circuit_->design().RegionRight() - net_maxx,
        net_miny - circuit_->design().RegionBottom(),
        circuit_->design().RegionTop() - net_maxy};

    std::vector<double> loc_candidate_x{
        (double)circuit_->design().RegionLeft(),
        (double)circuit_->design().RegionRight(), (net_minx + net_maxx) / 2,
        (net_minx + net_maxx) / 2};
    std::vector<double> loc_candidate_y{
        (net_maxy + net_miny) / 2, (net_maxy + net_miny) / 2,
        (double)circuit_->design().RegionBottom(),
        (double)circuit_->design().RegionTop()};

    // determine which placement boundary this net bounding box is most close
    // to, unless the pin is constrained to a specific edge
    std::vector<bool> close_to_boundary{false, false, false, false};
    int constrained = ConstrainedEdge(iopin);
    if (constrained >= 0) {
      close_to_boundary[constrained] = true;
    } else {
      double min_distance_x =
          std::min(distance_to_boundary[0], distance_to_boundary[1]);
      double min_distance_y =
          std::min(distance_to_boundary[2], distance_to_boundary[3]);
      if (min_distance_x < min_distance_y) {
        close_to_boundary[0] =
            distance_to_boundary[0] < distance_to_boundary[1];
        close_to_boundary[1] = !close_to_boundary[0];
      } else {
        close_to_boundary[2] =
            distance_to_boundary[2] < distance_to_boundary[3];
        close_to_boundary[3] = !close_to_boundary[2];
      }
    }

    for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
      if (close_to_boundary[i]) {
        if (boundary_spaces_[i].layer_spaces_[0].pin_clusters.empty()) {
          LOG(error) << "No free boundary interval is available for I/O pin "
                     << iopin.Name() << "\n";
          return false;
        }
        iopin.SetLoc(loc_candidate_x[i], loc_candidate_y[i], PLACED);
        boundary_spaces_[i].layer_spaces_[0].iopin_ptr_list.push_back(&iopin);
        break;
      }
    }
  }
  return true;
}

bool IoPlacer::PlaceIoPinOnEachBoundary() {
  for (auto& boundary_space : boundary_spaces_) {
    boundary_space.PlaceAssignedPins();
  }
  return true;
}

/****
 * @brief Adjust the location of an I/O pin, if it is placed on the right/top
 * boundary
 *
 * Because the width and height of the placement may not be integer multiple of
 * grid values. If this is the case, then I/O pins placed on the right/top
 * boundary by Dali are not on the actual placement boundary, so we need to
 * adjust their locations to address this problem.
 */
void IoPlacer::AdjustIoPinLocationForPhyDB() {
  const auto& die_area = phy_db_ptr_->GetDesignPtr()->GetDieArea();
  for (auto& iopin : circuit_->IoPins()) {
    // ignore pre-placed I/O pins
    if (iopin.IsPrePlaced()) continue;

    // grid values
    int final_x = circuit_->LocDali2PhydbX(iopin.X());
    int final_y = circuit_->LocDali2PhydbY(iopin.Y());

    // for I/O pins on the right/top boundary, need to adjust their location
    if (iopin.X() == circuit_->RegionURX()) {
      final_x = die_area.URX();
    }
    if (iopin.Y() == circuit_->RegionURY()) {
      final_y = die_area.URY();
    }

    iopin.SetFinalX(final_x);
    iopin.SetFinalY(final_y);
  }
}

bool IoPlacer::RunAutoPlacement() {
  PrintHorizontalLine();
  LOG(info) << "Start I/O Placement\n";
  if (!CheckConfiguration()) {
    LOG(info) << "\033[0;36m"
              << "I/O Placement fail!\n"
              << "\033[0m";
    return false;
  }

  if (!BuildResourceMap() || !AssignIoPinToBoundaryLayers() ||
      !PlaceIoPinOnEachBoundary()) {
    LOG(error) << "I/O placement failed while assigning boundary resources\n";
    return false;
  }
  AdjustIoPinLocationForPhyDB();

  LOG(info) << "\033[0;36m"
            << "I/O Placement complete!"
            << "\033[0m\n";
  return true;
}

bool IoPlacer::AutoPlaceCmd(int argc, char** argv) {
  bool is_config_successful = ConfigCmd(argc, argv);
  if (!is_config_successful) {
    LOG(fatal) << "Cannot successfully configure the IoPlacer\n";
    return false;
  }
  return RunAutoPlacement();
}

}  // namespace dali
