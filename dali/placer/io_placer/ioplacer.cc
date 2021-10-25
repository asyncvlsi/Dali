//
// Created by Yihang Yang on 8/25/21.
//

#include "ioplacer.h"

#include "dali/common/logging.h"
#include "dali/common/timing.h"

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

/* A function for add IOPIN to PhyDB database. This function should be called
 * before ordinary placement.
 *
 * Inputs:
 *  iopin_name: the name of the IOPIN
 *  net_name: the net this IOPIN is connected to
 *  direction: the direction of this IOPIN
 *  use: the usage of this IOPIN
 *
 * Return:
 *  true if this operation can be done successfully
 * */
bool IoPlacer::AddIoPin(
    std::string &iopin_name,
    std::string &net_name,
    std::string &direction,
    std::string &use
) {
    // check if this IOPIN is in PhyDB or not
    bool is_iopin_existing = phy_db_ptr_->IsIoPinExisting(iopin_name);
    if (is_iopin_existing) {
        BOOST_LOG_TRIVIAL(warning)
            << "IOPIN name is in PhyDB, cannot add it again: "
            << iopin_name << "\n";
        return false;
    }

    // check if this NET is in PhyDB or not
    bool is_net_existing = phy_db_ptr_->IsNetExisting(net_name);
    if (!is_net_existing) {
        BOOST_LOG_TRIVIAL(warning)
            << "NET name does not exist in PhyDB, cannot connect an IOPIN to it: "
            << net_name << "\n";
        return false;
    }

    // convert strings to direction and use
    phydb::SignalDirection signal_direction =
        phydb::StrToSignalDirection(direction);
    phydb::SignalUse signal_use = phydb::StrToSignalUse(use);

    // add this IOPIN to PhyDB
    phydb::IOPin *phydb_iopin =
        phy_db_ptr_->AddIoPin(iopin_name, signal_direction, signal_use);
    phydb_iopin->SetPlacementStatus(phydb::UNPLACED);
    phy_db_ptr_->AddIoPinToNet(iopin_name, net_name);

    // add it to Dali::Circuit
    p_ckt_->AddIoPinFromPhyDB(*phydb_iopin);

    return true;
}

/* A function for add IOPIN to PhyDB database interactively. This function should
 * be called before ordinary placement.
 *
 * Inputs:
 *  a list of argument from a user
 *
 * Return:
 *  true if this operation can be done successfully
 * */
bool IoPlacer::AddCmd(int argc, char **argv) {
    if (argc < 4) {
        BOOST_LOG_TRIVIAL(info)
            << "\033[0;36m"
            << "Add an IOPIN\n"
            << "Usage: place-io -a/--add\n"
            << "    <iopin_name> : name of the new IOPIN\n"
            << "    <net_name>   : name of the net this IOPIN will connect to\n"
            << "    <direction>  : specifies the pin type: {INPUT | OUTPUT | INOUT | FEEDTHRU}\n"
            << "    <use>        : specifies how the pin is used: {ANALOG | CLOCK | GROUND | POWER | RESET | SCAN | SIGNAL | TIEOFF}\n"
            << "\033[0m\n";
        return false;
    }

    std::string iopin_name(argv[0]);
    std::string net_name(argv[1]);
    std::string direction(argv[2]);
    std::string use(argv[3]);

    return AddIoPin(iopin_name, net_name, direction, use);
}

/* A function for placing IOPINs interactively.
 *
 * Inputs:
 *  iopin_name: the name of the IOPIN
 *  metal_name: the metal layer to create its physical geometry
 *  shape_lx: lower left x of this IOPIN with respect to its location
 *  shape_ly: lower left y of this IOPIN with respect to its location
 *  shape_ux: upper right x of this IOPIN with respect to its location
 *  shape_uy: upper right y of this IOPIN with respect to its location
 *  place_status: placement status
 *  loc_x_: x location of this IOPIN on a boundary
 *  loc_y: y location of this IOPIN on a boundary
 *  orient: orientation of this IOPIN
 *
 * Return:
 *  true if this operation can be done successfully
 * */
bool IoPlacer::PlaceIoPin(std::string &iopin_name,
                          std::string &metal_name,
                          int shape_lx,
                          int shape_ly,
                          int shape_ux,
                          int shape_uy,
                          std::string &place_status,
                          int loc_x,
                          int loc_y,
                          std::string &orient) {
    // check if this IOPIN is in PhyDB or not
    bool is_iopin_existing_phydb = phy_db_ptr_->IsIoPinExisting(iopin_name);
    if (!is_iopin_existing_phydb) {
        BOOST_LOG_TRIVIAL(warning)
            << "IOPIN is not in PhyDB, cannot set its placement status: "
            << iopin_name << "\n";
        return false;
    }

    // get metal layer name
    int is_metal_layer_existing =
        p_ckt_->IsMetalLayerExisting(metal_name);
    if (!is_metal_layer_existing) {
        BOOST_LOG_TRIVIAL(warning)
            << "The given metal layer index does not exist: "
            << metal_name << "\n";
        return false;
    }

    // set IOPIN placement status in PhyDB
    phydb::IOPin *phydb_iopin_ptr = phy_db_ptr_->GetIoPinPtr(iopin_name);
    phydb_iopin_ptr->SetShape(
        metal_name,
        shape_lx,
        shape_ly,
        shape_ux,
        shape_uy
    );
    phydb_iopin_ptr->SetPlacement(
        phydb::StrToPlaceStatus(place_status),
        loc_x,
        loc_y,
        phydb::StrToCompOrient(orient)
    );

    // check if this IOPIN is in Dali::Circuit or not (it should)
    bool is_iopin_existing_dali = p_ckt_->IsIoPinExisting(iopin_name);
    DaliExpects(is_iopin_existing_dali, "IOPIN in PhyDB but not in Dali?");
    IoPin *iopin_ptr_dali = p_ckt_->GetIoPinPtr(iopin_name);

    // set IOPIN placement status in Dali
    iopin_ptr_dali->SetLoc(p_ckt_->LocPhydb2DaliX(loc_x),
                           p_ckt_->LocPhydb2DaliY(loc_y),
                           StrToPlaceStatus(place_status));
    MetalLayer *metal_layer_ptr = p_ckt_->GetMetalLayerPtr(metal_name);
    iopin_ptr_dali->SetLayerPtr(metal_layer_ptr);

    return true;
}

bool IoPlacer::PlaceCmd(int argc, char **argv) {
    if (argc < 10) {
        BOOST_LOG_TRIVIAL(info)
            << "\033[0;36m"
            << "Add an IOPIN\n"
            << "Usage: place-io -p/--place \n"
            << "    <iopin_name>  : name of the new IOPIN\n"
            << "    <metal_name>  : name of the metal layer to create its physical geometry\n"
            << "    <shape_lx>    : the pin geometry on that layer\n"
            << "    <shape_ly>    : the pin geometry on that layer\n"
            << "    <shape_ux>    : the pin geometry on that layer\n"
            << "    <shape_uy>    : the pin geometry on that layer\n"
            << "    <place_status>: placement status of this IOPIN: { COVER | FIXED | PLACED }\n"
            << "    <loc_x_>       : x location of this IOPIN\n"
            << "    <loc_y>       : y location of this IOPIN\n"
            << "    <orient>      : orientation of this IOPIN: { N | S | W | E | FN | FS | FW | FE }\n"
            << "\033[0m";
        return false;
    }

    std::string iopin_name(argv[0]);
    std::string metal_name(argv[1]);
    std::string shape_lx_str(argv[2]);
    std::string shape_ly_str(argv[3]);
    std::string shape_ux_str(argv[4]);
    std::string shape_uy_str(argv[5]);
    std::string place_status(argv[6]);
    std::string loc_x_str(argv[7]);
    std::string loc_y_str(argv[8]);
    std::string orient(argv[9]);

    int shape_lx = 0, shape_ly = 0, shape_ux = 0, shape_uy = 0;
    int loc_x = 0, loc_y = 0;
    try {
        shape_lx = std::stoi(shape_lx_str);
        shape_ly = std::stoi(shape_ly_str);
        shape_ux = std::stoi(shape_ux_str);
        shape_uy = std::stoi(shape_uy_str);
        loc_x = std::stoi(loc_x_str);
        loc_y = std::stoi(loc_y_str);
    } catch (...) {
        DaliExpects(false, "invalid IOPIN geometry or location");
    }

    return PlaceIoPin(iopin_name,
                      metal_name,
                      shape_lx,
                      shape_ly,
                      shape_ux,
                      shape_uy,
                      place_status,
                      loc_x,
                      loc_y,
                      orient);
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
    MetalLayer *metal_layer =
        &(p_ckt_->Metals()[metal_layer_index]);
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
    BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
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
            MetalLayer *metal_layer =
                p_ckt_->GetMetalLayerPtr(option_str);
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
        std::sort(used_segments.begin(),
                  used_segments.end(),
                  [](const Seg<double> &lhs, const Seg<double> &rhs) {
                      return (lhs.lo < rhs.lo);
                  });
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

    BOOST_LOG_TRIVIAL(info)
        << "\033[0;36m"
        << "I/O Placement complete!\n"
        << "\033[0m";
    return true;
}

bool IoPlacer::AutoPlaceCmd(int argc, char **argv) {
    bool is_config_successful = ConfigCmd(argc, argv);
    if (!is_config_successful) {
        BOOST_LOG_TRIVIAL(fatal)
            << "Cannot successfully configure the IoPlacer\n";
        return false;
    }
    return AutoPlaceIoPin();
}

}