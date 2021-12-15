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
#include "ioplacer.h"

#include "dali/common/logging.h"
#include "dali/common/phydbhelper.h"

#define NUM_OF_PLACE_BOUNDARY 4
#define LEFT 0
#define RIGHT 1
#define BOTTOM 2
#define TOP 3

namespace dali {

IoPlacer::IoPlacer() {
  InitializeBoundarySpaces();
}

IoPlacer::IoPlacer(phydb::PhyDB *phy_db, Circuit *circuit) {
  SetPhyDB(phy_db);
  SetCiruit(circuit);
  InitializeBoundarySpaces();
}

void IoPlacer::InitializeBoundarySpaces() {
  boundary_spaces_.reserve(NUM_OF_PLACE_BOUNDARY);
  // put all boundaries in a vector
  std::vector<double> boundary_loc{
      (double) p_ckt_->design().RegionLeft(),
      (double) p_ckt_->design().RegionRight(),
      (double) p_ckt_->design().RegionBottom(),
      (double) p_ckt_->design().RegionTop()
  };

  // initialize each boundary
  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    boundary_spaces_.emplace_back(
        i == BOTTOM || i == TOP,
        boundary_loc[i]
    );
    boundary_spaces_.back().manufacturing_grid_ =
        phy_db_ptr_->tech().GetManufacturingGrid();
  }

}

void IoPlacer::SetCiruit(Circuit *circuit) {
  DaliExpects(circuit != nullptr,
              "Cannot initialize an IoPlacer without providing a valid Circuit pointer");
  p_ckt_ = circuit;
}

void IoPlacer::SetPhyDB(phydb::PhyDB *phy_db_ptr) {
  DaliExpects(phy_db_ptr != nullptr,
              "Cannot initialize an IoPlacer without providing a valid PhyDB pointer");
  phy_db_ptr_ = phy_db_ptr;
}

bool IoPlacer::PartialPlaceIoPin() {
  return true;
}

bool IoPlacer::PartialPlaceCmd(int argc, char **argv) {
  return true;
}

bool IoPlacer::ConfigSetMetalLayer(int boundary_index, int metal_layer_index) {
  // check if this metal index exists or not
  bool is_legal_index = (metal_layer_index >= 0) &&
      (metal_layer_index < (int) p_ckt_->Metals().size());
  if (!is_legal_index) {
    BOOST_LOG_TRIVIAL(info)
      << "metal layer index is a bad value: "
      << metal_layer_index << "\n";
    return false;
  }
  MetalLayer *metal_layer = &(p_ckt_->Metals()[metal_layer_index]);
  boundary_spaces_[boundary_index].AddLayer(metal_layer);
  return true;
}

bool IoPlacer::ConfigSetGlobalMetalLayer(int metal_layer_index) {
  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    bool is_successful = ConfigSetMetalLayer(i, metal_layer_index);
    if (!is_successful) {
      return false;
    }
  }
  return true;
}

bool IoPlacer::ConfigAutoPlace() {
  return true;
}

bool IoPlacer::ConfigBoundaryMetal(int argc, char **argv) {
  if (argc < 2) {
    ReportConfigUsage();
    return false;
  }
  for (int i = 0; i < argc;) {
    std::string arg(argv[i++]);
    if (i < argc) {
      std::string metal_name = std::string(argv[i++]);
      bool is_layer_existing =
          p_ckt_->IsMetalLayerExisting(metal_name);
      if (!is_layer_existing) {
        BOOST_LOG_TRIVIAL(fatal) << "Invalid metal layer name!\n";
        ReportConfigUsage();
        return false;
      }
      MetalLayer *metal_layer =
          p_ckt_->GetMetalLayerPtr(metal_name);
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
        BOOST_LOG_TRIVIAL(fatal)
          << "Invalid boundary, possible values: left, right, bottom, top\n";
        ReportConfigUsage();
        return false;
      }
      std::cout << arg << "  " << metal_name << "\n";
    } else {
      BOOST_LOG_TRIVIAL(fatal)
        << "Boundary specified, but metal layer is not given\n";
      ReportConfigUsage();
      return false;
    }
  }
  return true;
}

void IoPlacer::ReportConfigUsage() {
  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "Usage: place-io -c/--config\n"
    << "  -h/--help\n"
    << "      print out function usage\n"
    << "  -m/--metal <left/right/bottom/top> <metal layer>\n"
    << "      use this command to specify which metal layers to use for IOPINs on each placement boundary\n"
    << "      example: -m left m1, for IOPINs on the left boundary, using layer m1 to create physical geometry\n"
    << "      'place-io <metal layer>' is a shorthand for 'place-io -c -m left m1 right m1 bottom m1 top m1'\n"
    << "\033[0m\n";
}

bool IoPlacer::ConfigCmd(int argc, char **argv) {
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
    bool is_metal_name = p_ckt_->IsMetalLayerExisting(option_str);
    // when the command is like 'place-io <metal layer>'
    if (is_metal_name) {
      MetalLayer *metal_layer = p_ckt_->GetMetalLayerPtr(option_str);
      return ConfigSetGlobalMetalLayer(metal_layer->Id());
    }
    BOOST_LOG_TRIVIAL(fatal) << "Unknown flag: " << option_str << "\n";
    ReportConfigUsage();
    return false;
  }
}

// resource should be large enough for all IOPINs
bool IoPlacer::CheckConfiguration() {
  return true;
}

bool IoPlacer::BuildResourceMap() {
  std::vector<std::vector<Seg<double>>> all_used_segments(
      NUM_OF_PLACE_BOUNDARY,
      std::vector<Seg<double>>(0)
  ); // TODO: carry layer info
  for (auto &iopin: p_ckt_->IoPins()) {
    if (iopin.IsPrePlaced()) {
      double spacing = iopin.LayerPtr()->Spacing();
      if (iopin.X() == p_ckt_->design().RegionLeft()) {
        double lly = iopin.LY(spacing);
        double ury = iopin.UY(spacing);
        all_used_segments[LEFT].emplace_back(lly, ury);
      } else if (iopin.X()
          == p_ckt_->design().RegionRight()) {
        double lly = iopin.LY(spacing);
        double ury = iopin.UY(spacing);
        all_used_segments[RIGHT].emplace_back(lly, ury);
      } else if (iopin.Y()
          == p_ckt_->design().RegionBottom()) {
        double llx = iopin.LX(spacing);
        double urx = iopin.UX(spacing);
        all_used_segments[BOTTOM].emplace_back(llx, urx);
      } else if (iopin.Y() == p_ckt_->design().RegionTop()) {
        double llx = iopin.LX(spacing);
        double urx = iopin.UX(spacing);
        all_used_segments[TOP].emplace_back(llx, urx);
      } else {
        DaliExpects(false,
                    "Pre-placed IOPIN is not on placement boundary? "
                        + iopin.Name());
      }
    }
  }

  std::vector<double> boundary_loc{
      (double) p_ckt_->design().RegionLeft(),
      (double) p_ckt_->design().RegionRight(),
      (double) p_ckt_->design().RegionBottom(),
      (double) p_ckt_->design().RegionTop()
  };

  for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
    std::vector<Seg<double>> &used_segments = all_used_segments[i];
    std::sort(
        used_segments.begin(),
        used_segments.end(),
        [](const Seg<double> &lhs, const Seg<double> &rhs) {
          return (lhs.lo < rhs.lo);
        }
    );
    std::vector<Seg<double>> avail_space;
    if (i == LEFT || i == RIGHT) {
      double lo = p_ckt_->design().RegionBottom();
      double span = 0;
      int len = (int) used_segments.size();
      if (len == 0) {
        span = p_ckt_->design().RegionTop() - lo;
        boundary_spaces_[i].layer_spaces_[0].AddCluster(lo, span);
      }
      for (int j = 0; j < len; ++j) {
        if (lo < used_segments[j].lo) {
          double hi = p_ckt_->design().RegionTop();
          if (j + 1 < len) {
            hi = used_segments[j + 1].lo;
          }
          span = hi - lo;
          boundary_spaces_[i].layer_spaces_[0].AddCluster(lo, span);
        }
        lo = used_segments[j].hi;
      }
    } else {
      double lo = p_ckt_->design().RegionLeft();
      double span = 0;
      int len = (int) used_segments.size();
      if (len == 0) {
        span = p_ckt_->design().RegionRight() - lo;
        boundary_spaces_[i].layer_spaces_[0].AddCluster(lo, span);
      }
      for (int j = 0; j < len; ++j) {
        if (lo < used_segments[j].lo) {
          double hi = p_ckt_->design().RegionRight();
          if (j + 1 < len) {
            hi = used_segments[j + 1].lo;
          }
          span = hi - lo;
          boundary_spaces_[i].layer_spaces_[0].AddCluster(lo, span);
        }
        lo = used_segments[j].hi;
      }
    }
  }
  return true;
}

bool IoPlacer::AssignIoPinToBoundaryLayers() {
  for (auto &iopin: p_ckt_->IoPins()) {
    // do nothing for placed IOPINs
    if (iopin.IsPrePlaced()) continue;

    // find the bounding box of the net containing this IOPIN
    Net *net = iopin.NetPtr();
    if (net->BlockPins().empty()) {
      // if this net only contain this IOPIN, do nothing
      BOOST_LOG_TRIVIAL(warning)
        << "Net " << net->Name()
        << " only contains IOPIN "
        << iopin.Name()
        << ", skip placing this IOPIN\n";
      continue;
    }
    net->UpdateMaxMinIndex();
    double net_minx = net->MinX();
    double net_maxx = net->MaxX();
    double net_miny = net->MinY();
    double net_maxy = net->MaxY();

    // compute distances from edges of this bounding box to the corresponding placement boundary
    std::vector<double> distance_to_boundary{
        net_minx - p_ckt_->design().RegionLeft(),
        p_ckt_->design().RegionRight() - net_maxx,
        net_miny - p_ckt_->design().RegionBottom(),
        p_ckt_->design().RegionTop() - net_maxy
    };

    // compute candidate locations for this IOPIN on each boundary
    std::vector<double> loc_candidate_x{
        (double) p_ckt_->design().RegionLeft(),
        (double) p_ckt_->design().RegionRight(),
        (net_minx + net_maxx) / 2,
        (net_minx + net_maxx) / 2
    };
    std::vector<double> loc_candidate_y{
        (net_maxy + net_miny) / 2,
        (net_maxy + net_miny) / 2,
        (double) p_ckt_->design().RegionBottom(),
        (double) p_ckt_->design().RegionTop()
    };

    // determine which placement boundary this net bounding box is most close to
    std::vector<bool> close_to_boundary{false, false, false, false};
    double min_distance_x = std::min(distance_to_boundary[0],
                                     distance_to_boundary[1]);
    double min_distance_y = std::min(distance_to_boundary[2],
                                     distance_to_boundary[3]);
    if (min_distance_x < min_distance_y) {
      close_to_boundary[0] =
          distance_to_boundary[0] < distance_to_boundary[1];
      close_to_boundary[1] = !close_to_boundary[0];
    } else {
      close_to_boundary[2] =
          distance_to_boundary[2] < distance_to_boundary[3];
      close_to_boundary[3] = !close_to_boundary[2];
    }

    // set this IOPIN to the best candidate location
    for (int i = 0; i < NUM_OF_PLACE_BOUNDARY; ++i) {
      if (close_to_boundary[i]) {
        iopin.SetLoc(loc_candidate_x[i], loc_candidate_y[i], PLACED);
        boundary_spaces_[i].layer_spaces_[0].iopin_ptr_list.push_back(&iopin);
        break;
      }
    }
  }
  return true;
}

bool IoPlacer::PlaceIoPinOnEachBoundary() {
  for (auto &boundary_space: boundary_spaces_) {
    boundary_space.AutoPlaceIoPin();
  }
  return true;
}

/****
 * @brief Adjust the location of an I/O pin, if it is placed on the right/top boundary
 *
 * Because the width and height of the placement may not be integer multiple of
 * grid values. If this is the case, then I/O pins placed on the right/top boundary
 * by Dali are not on the actual placement boundary, so we need to adjust their
 * locations to address this problem.
 */
void IoPlacer::AdjustIoPinLocationForPhyDB() {
  for (auto &iopin: p_ckt_->IoPins()) {
    // ignore pre-placed I/O pins
    if (iopin.IsPrePlaced()) continue;

    // compute PhyDB locations assuming width and height are integer multiple of grid values
    int final_x = p_ckt_->LocDali2PhydbX(iopin.X());
    int final_y = p_ckt_->LocDali2PhydbY(iopin.Y());

    // for I/O pins on the right/top boundary, need to adjust their location
    if (iopin.X() == p_ckt_->RegionURX()) {
      final_x += p_ckt_->design().DieAreaOffsetXResidual();
    }
    if (iopin.Y() == p_ckt_->RegionURY()) {
      final_y += p_ckt_->design().DieAreaOffsetYResidual();
    }

    // set final locations
    iopin.SetFinalX(final_x);
    iopin.SetFinalY(final_y);
  }
}

bool IoPlacer::AutoPlaceIoPin() {
  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start I/O Placement\n";
  if (!CheckConfiguration()) {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;36m"
      << "I/O Placement fail!\n"
      << "\033[0m";
    return false;
  }

  BuildResourceMap();
  AssignIoPinToBoundaryLayers();
  PlaceIoPinOnEachBoundary();
  AdjustIoPinLocationForPhyDB();

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "I/O Placement complete!\n"
    << "\033[0m";
  return true;
}

bool IoPlacer::AutoPlaceCmd(int argc, char **argv) {
  bool is_config_successful = ConfigCmd(argc, argv);
  if (!is_config_successful) {
    BOOST_LOG_TRIVIAL(fatal) << "Cannot successfully configure the IoPlacer\n";
    return false;
  }
  return AutoPlaceIoPin();
}

}
