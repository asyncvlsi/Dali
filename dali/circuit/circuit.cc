//
// Created by Yihang Yang on 3/23/19.
//

#include "circuit.h"

#include <chrono>
#include <climits>
#include <cmath>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "dali/common/helper.h"
#include "dali/common/optregdist.h"
#include "dali/circuit/status.h"

#define EPSILON 1e-6

namespace dali {

Circuit::Circuit() {
    AddDummyIOPinBlockType();
}

/****
 * Creates fake NP-well information for testing purposes
 * create fake N/P-well info for cells
 * ****/
void Circuit::LoadImaginaryCellFile() {
    // 1. create fake well tap cell
    std::string tap_cell_name("welltap_svt");
    BlockType *tmp_well_tap_cell = AddBlockTypeWithGridUnit(
        tap_cell_name,
        MinBlkWidth(),
        MinBlkHeight()
    );
    tech_.WellTapCellPtrs().push_back(tmp_well_tap_cell);

    // 2. create fake well parameters
    double fake_same_diff_spacing = 0;
    double fake_any_diff_spacing = 0;
    SetLegalizerSpacing(fake_same_diff_spacing, fake_any_diff_spacing);

    double width = MinBlkHeight() / 2.0 * GridValueY();
    double spacing = MinBlkWidth() * GridValueX();
    double op_spacing = MinBlkWidth() * GridValueX();
    double max_plug_dist = AveMovBlkWidth() * 10 * GridValueX();
    double overhang = 0;
    SetNWellParams(width, spacing, op_spacing, max_plug_dist, overhang);
    SetNWellParams(width, spacing, op_spacing, max_plug_dist, overhang);

    // 3. create fake NP-well geometries for each BlockType
    for (auto &pair: tech_.block_type_map_) {
        auto *blk_type = pair.second;
        BlockTypeWell *well = AddBlockTypeWell(blk_type);
        int np_edge = blk_type->Height() / 2;
        well->SetNwellRect(0, np_edge, blk_type->Width(), blk_type->Height());
        well->SetPwellRect(0, 0, blk_type->Width(), np_edge);
    }
}

// custom residual function, return: x - round(x/y) * y
double Circuit::Residual(double x, double y) {
    return x - std::round(x / y) * y;
}

#ifdef USE_OPENDB
Circuit::Circuit(odb::dbDatabase *db_ptr) {
  AddDummyIOPinBlockType();
  db_ptr_ = db_ptr;
  InitializeFromDB(db_ptr);
}

void Circuit::InitializeFromDB(odb::dbDatabase *db_ptr) {
  db_ptr_ = db_ptr;
  auto tech = db_ptr->getTech();
  Assert(tech != nullptr, "No tech info specified!\n");
  auto lib = db_ptr->getLibs().begin();
  Assert(lib != db_ptr->getLibs().end(), "No lib info specified!");

  // 1. lef database microns
  setDatabaseMicron(tech->getDbUnitsPerMicron());
  //BOOST_LOG_TRIVIAL(info)   << tech->getDbUnitsPerMicron() << "\n";
  //BOOST_LOG_TRIVIAL(info)   << tech->getLefUnits() << "\n";
  //BOOST_LOG_TRIVIAL(info)   << top_level->getDefUnits() << "\n";

  // 2. manufacturing grid and metals
  if (tech->hasmanufacturing_grid_) {
    //BOOST_LOG_TRIVIAL(info)   << "Mangrid" << tech->GetManufacturingGrid() << "\n";
    setManufacturingGrid(tech->GetManufacturingGrid() / double(tech_.database_microns_));
  } else {
    setManufacturingGrid(1.0 / tech_.database_microns_);
  }
  for (auto &&layer: tech->getLayers()) {
    if (layer->getType() == 0) {
      //BOOST_LOG_TRIVIAL(info)   << layer->getNumber() << "  " << layer->getName() << "  " << layer->getType() << " ";
      std::string metal_layer_name(layer->getName());
      double min_width = layer->getWidth() / (double) tech_.database_microns_;
      double min_spacing = layer->getSpacing() / (double) tech_.database_microns_;
      //BOOST_LOG_TRIVIAL(info)   << min_width << "  " << min_spacing << "  ";
      std::string str_direct(layer->getDirection().getString());
      MetalDirection direct = StrToMetalDirection(str_direct);
      double min_area = 0;
      if (layer->hasArea()) {
        //BOOST_LOG_TRIVIAL(info)   << layer->getArea();
        min_area = layer->getArea();
      }
      AddMetalLayer(
          metal_layer_name,
          min_width,
          min_spacing,
          min_area,
          min_width + min_spacing,
          min_width + min_spacing,
          direct
          );
      //BOOST_LOG_TRIVIAL(info)   << "\n";
    }
  }

  // 3. set grid value using metal pitches, and set row height
  setGridUsingMetalPitch();
  auto site = lib->getSites().begin();
  double row_height = site->getHeight() / double(tech_.database_microns_);
  setRowHeight(row_height);
  //BOOST_LOG_TRIVIAL(info)   << site->getName() << "  " << site->getWidth() / double(tech_.database_microns_) << "  " << tech_.row_height_ << "\n";

  // 4. load all macros, aka gate types, block types, cell types
  //BOOST_LOG_TRIVIAL(info)   << lib->getName() << " lib\n";
  double lx = 0, lly = 0, urx = 0, ury = 0;
  double width = 0, height = 0;
  for (auto &&macro: lib->getMasters()) {
    std::string macro_name(macro->getName());
    width = macro->getWidth() / (double) tech_.database_microns_;
    height = macro->getHeight() / (double) tech_.database_microns_;
    BlockType *blk_type = nullptr;
    if (macro_name.find("welltap") != std::string::npos) {
      blk_type = AddWellTapBlockType(macro_name, width, height);
    } else {
      blk_type = AddBlockType(macro_name, width, height);
    }
    //BOOST_LOG_TRIVIAL(info)   << macro->getName() << "\n";
    //BOOST_LOG_TRIVIAL(info)   << macro->getWidth()/grid_value_x_/lef_database_microns << "  " << macro->getHeight()/grid_value_y_/lef_database_microns << "\n";
    for (auto &&terminal: macro->getMTerms()) {
      std::string pin_name(terminal->getName());
      //if (pin_name == "Vdd" || pin_name == "GND") continue;
      //BOOST_LOG_TRIVIAL(info)   << terminal->getName() << " " << terminal->getMPins().begin()->getGeometry().begin()->xMax()/grid_value_x/lef_database_microns << "\n";
      Assert(!terminal->getMPins().empty(), "No physical pins, Macro: " + *blk_type->NamePtr() + ", pin: " + pin_name);
      Assert(!terminal->getMPins().begin()->getGeometry().empty(), "No geometries provided for pin");
      auto geo_shape = terminal->getMPins().begin()->getGeometry().begin();
      lx = geo_shape->xMin() / GridValueX() / tech_.database_microns_;
      urx = geo_shape->xMax() / GridValueX() / tech_.database_microns_;
      lly = geo_shape->yMin() / GridValueY() / tech_.database_microns_;
      ury = geo_shape->yMax() / GridValueY() / tech_.database_microns_;

      bool is_input = true;
      if (terminal->getIoType() == odb::dbIoType::INPUT) {
        is_input = true;
      } else if (terminal->getIoType() == odb::dbIoType::OUTPUT) {
        is_input = false;
      } else {
        is_input = false;
        //Assert(false, "Unsupported terminal IO type\n");
      }
      Pin *new_pin = AddBlkTypePin(blk_type, pin_name, is_input);
      AddBlkTypePinRect(new_pin, lx, lly, urx, ury);
    }
  }
  //tech_.well_tap_cell_->Report();

  auto chip = db_ptr->getChip();
  if (chip == nullptr) return;
  auto top_level = chip->getBlock();
  int components_count = 0, pins_count = 0, nets_count = 0;
  components_count = top_level->getInsts().size();
  pins_count = top_level->getBTerms().size();
  nets_count = top_level->getNets().size();

  setListCapacity(components_count, pins_count, nets_count);

  if (globalVerboseLevel >= LOG_CRITICAL) {
    BOOST_LOG_TRIVIAL(info)   << "components count: " << components_count << "\n"
              << "pins count:        " << pins_count << "\n"
              << "nets count:       " << nets_count << "\n";
  }

  for (auto &&track_set: top_level->getTrackGrids()) {
    BOOST_LOG_TRIVIAL(info)   << track_set->getTechLayer()->getName() << "\n";
    int x_grid_pattern = 0;
    int y_grid_pattern = 0;
    x_grid_pattern = track_set->getNumGridPatternsX();
    y_grid_pattern = track_set->getNumGridPatternsY();
    BOOST_LOG_TRIVIAL(info)   << "x grid pattern: " << x_grid_pattern << "\n";
    BOOST_LOG_TRIVIAL(info)   << "y grid pattern: " << y_grid_pattern << "\n";
  }

  // 5. load all gates
  int llx_int = 0, lly_int = 0;
  setUnitsDistanceMicrons(top_level->getDefUnits());
  odb::adsRect die_area;
  top_level->getDieArea(die_area);
  //BOOST_LOG_TRIVIAL(info)   << die_area.xMin() << "\n"
  //          << die_area.xMax() << "\n"
  //          << die_area.yMin() << "\n"
  //          << die_area.yMax() << "\n";
  //design_.die_area_offset_x_ = die_area.xMin()/10;
  //design_.die_area_offset_y_ = die_area.yMin()/10;
  //setDieArea((die_area.xMin() - die_area.xMin())/10,
  //           (die_area.yMin() - die_area.yMin())/10,
  //           (die_area.xMax() - die_area.xMin())/10,
  //           (die_area.yMax() - die_area.yMin())/10);
  design_.die_area_offset_x_ = 0;
  design_.die_area_offset_y_ = 0;
  setDieArea(die_area.xMin()/10,
             die_area.yMin()/10,
             die_area.xMax()/10,
             die_area.yMax()/10);
  for (auto &&blk: top_level->getInsts()) {
    //BOOST_LOG_TRIVIAL(info)   << blk->getName() << "  " << blk->getMaster()->getName() << "\n";
    std::string blk_name(blk->getName());
    std::string blk_type_name(blk->getMaster()->getName());
    blk->getLocation(llx_int, lly_int);
    llx_int /= 10;
    lly_int /= 10;
    llx_int = (int) std::round(llx_int / GridValueX() / design_.def_distance_microns);
    lly_int = (int) std::round(lly_int / GridValueY() / design_.def_distance_microns);
    std::string place_status(blk->getPlacementStatus().getString());
    std::string orient(blk->getOrient().getString());
    AddBlock(blk_name, blk_type_name, llx_int, lly_int, StrToPlaceStatus(place_status), StrToOrient(orient));
  }

  // 6. load all IOPINs
  for (auto &&iopin: top_level->getBTerms()) {
    //BOOST_LOG_TRIVIAL(info)   << iopin->getName() << "\n";
    std::string iopin_name(iopin->getName());
    int iopin_x = 0;
    int iopin_y = 0;
    bool is_loc_set = iopin->getFirstPinLocation(iopin_x, iopin_y);
    std::string str_sig_use(iopin->getSigType().getString());
    SignalUse sig_use = StrToSignalUse(str_sig_use);

    std::string str_sig_dir(iopin->getIoType().getString());
    SignalDirection sig_dir = StrToSignalDirection(str_sig_dir);

    if (is_loc_set) {
      AddIOPin(iopin_name, PLACED, sig_use, sig_dir, iopin_x, iopin_y);
    } else {
      AddIOPin(iopin_name, UNPLACED, sig_use, sig_dir);
    }
  }

  // 7. load all NETs
  //BOOST_LOG_TRIVIAL(info)   << "Nets:\n";
  for (auto &&net: top_level->getNets()) {
    //BOOST_LOG_TRIVIAL(info)   << net->getName() << "\n";
    std::string net_name(net->getName());
    int net_capacity = int(net->getITermCount() + net->getBTermCount());
    AddNet(net_name, net_capacity, design_.normal_signal_weight);
    for (auto &&bterm: net->getBTerms()) {
      //BOOST_LOG_TRIVIAL(info)   << "  ( PIN " << bterm->getName() << ")  \t";
      std::string iopin_name(bterm->getName());
      AddIOPinToNet(iopin_name, net_name);
    }
    for (auto &&iterm: net->getITerms()) {
      //BOOST_LOG_TRIVIAL(info)   << "  (" << iterm->getInst()->getName() << "  " << iterm->getMTerm()->getName() << ")  \t";
      std::string blk_name(iterm->getInst()->getName());
      std::string pin_name(iterm->getMTerm()->getName());
      AddBlkPinToNet(blk_name, pin_name, net_name);
    }
    //BOOST_LOG_TRIVIAL(info)   << "\n";
  }
}
#endif

void Circuit::LoadTech(phydb::PhyDB *phy_db_ptr) {
    auto &phy_db_tech = *(phy_db_ptr->GetTechPtr());

    // 1. lef database microns and manufacturing grid
    DaliExpects(phy_db_tech.GetDatabaseMicron() > 0,
                "Bad DATABASE MICRONS from PhyDB");
    SetDatabaseMicron(phy_db_tech.GetDatabaseMicron());
    BOOST_LOG_TRIVIAL(info)
        << "DATABASE MICRONS " << tech_.database_microns_ << "\n";
    if (phy_db_tech.GetManufacturingGrid() > EPSILON) {
        SetManufacturingGrid(phy_db_tech.GetManufacturingGrid());
    } else {
        SetManufacturingGrid(1.0 / tech_.database_microns_);
    }
    BOOST_LOG_TRIVIAL(info)
        << "MANUFACTURINGGRID " << tech_.manufacturing_grid_ << "\n";

    // 2. placement grid and metal layers
    double grid_value_x = 0;
    double grid_value_y = 0;
    bool is_placement_grid_set = phy_db_tech.GetPlacementGrids(
        grid_value_x,
        grid_value_y
    );
    if (is_placement_grid_set) {
        SetGridValue(grid_value_x, grid_value_y);
    } else {
        BOOST_LOG_TRIVIAL(info) << "Placement grid not set in PhyDB\n";
        BOOST_LOG_TRIVIAL(info) << "Checking Sites\n";
        auto &sites = phy_db_tech.GetSitesRef();
        if (!sites.empty()) {
            grid_value_x = sites[0].GetWidth();
            grid_value_y = sites[0].GetHeight();
            SetGridValue(grid_value_x, grid_value_y);
            SetRowHeight(grid_value_y);
        } else {
            BOOST_LOG_TRIVIAL(info) << "No Sites found\n";
        }
    }

    for (auto &layer: phy_db_tech.GetLayersRef()) {
        if (layer.GetType() == phydb::ROUTING) {
            std::string layer_name = layer.GetName();

            double pitch_x = -1, pitch_y = -1;
            layer.GetPitch(pitch_x, pitch_y);

            double min_width = layer.GetMinWidth();
            if (min_width <= 0) {
                min_width = layer.GetWidth();
            }
            DaliExpects(min_width > 0,
                        "MinWidth and Width not found in PhyDB for layer: "
                            + layer_name);

            double min_spacing = 0;
            auto &spacing_table = *(layer.GetSpacingTable());
            if (spacing_table.GetNRow() >= 1 && spacing_table.GetNCol() >= 1) {
                min_spacing = spacing_table.GetSpacingAt(0, 0);
            } else {
                min_spacing = layer.GetSpacing();
            }
            DaliExpects(min_spacing > 0,
                        "MinSpacing not found in PhyDB for layer: "
                            + layer_name);

            double min_area = layer.GetArea();
            DaliExpects(min_area >= 0,
                        "Dali expects a non-negative MinArea for layer: "
                            + layer_name);

            auto direction = MetalDirection(layer.GetDirection());
            AddMetalLayer(
                layer_name,
                min_width,
                min_spacing,
                min_area,
                pitch_x,
                pitch_y,
                direction
            );
        }
    }
    if (!tech_.is_grid_set_) {
        SetGridFromMetalPitch();
    }

    // 3. load all macros
    for (auto &macro: phy_db_tech.GetMacrosRef()) {
        std::string macro_name(macro.GetName());
        double width = macro.GetWidth();
        double height = macro.GetHeight();
        BlockType *blk_type = nullptr;
        if (macro.GetClass() == phydb::CORE_WELLTAP) {
            blk_type = AddWellTapBlockType(macro_name, width, height);
        } else {
            blk_type = AddBlockType(macro_name, width, height);
        }
        auto &macro_pins = macro.GetPinsRef();
        for (auto &pin: macro_pins) {
            std::string pin_name(pin.GetName());
            //if (pin_name == "Vdd" || pin_name == "GND") continue;

            bool is_input = true;
            auto pin_direction = pin.GetDirection();
            is_input = (pin_direction == phydb::INPUT);
            Pin *new_pin = blk_type->AddPin(pin_name, is_input);

            auto &layer_rects = pin.GetLayerRectRef();
            if (layer_rects.empty()) {
                DaliExpects(false,
                            "No physical pins, Macro: " + blk_type->Name()
                                + ", pin: " + pin_name);
            }
            for (auto &layer_rect: layer_rects) {
                auto &rects = layer_rect.GetRects();
                for (auto &rect: rects) {
                    double llx = rect.LLX() / GridValueX();
                    double urx = rect.URX() / GridValueX();
                    double lly = rect.LLY() / GridValueY();
                    double ury = rect.URY() / GridValueY();
                    new_pin->AddRectOnly(llx, lly, urx, ury);
                }
            }
            if (new_pin->IsRectEmpty()) {
                DaliExpects(false,
                            "No geometries provided for pin " + pin_name
                                + " in Macro: " + blk_type->Name());
            }
        }
        for (auto &pin: blk_type->PinList()) {
            pin.InitOffset();
        }
    }
}

void Circuit::SetDieArea(
    int lower_x,
    int lower_y,
    int upper_x,
    int upper_y
) { // unit in manufacturing grid
    DaliExpects(GridValueX() > 0 && GridValueY() > 0,
                "Need to set positive grid values before setting placement boundary");
    DaliExpects(design_.distance_microns_ > 0,
                "Need to set def_distance_microns before setting placement boundary using Circuit::SetDieArea()");
    double factor_x = GridValueX() * design_.distance_microns_;
    double factor_y = GridValueY() * design_.distance_microns_;

    // TODO, extract placement boundary from rows if they are any rows
    DaliWarns((lower_x % (int) std::round(factor_x)) != 0,
              "expects the left boundary coordinate is integer multiple of grid_value_x");
    DaliWarns((lower_y % (int) std::round(factor_y)) != 0,
              "expects the bottom boundary coordinate is integer multiple of grid_value_y");
    DaliWarns((upper_x % (int) std::round(factor_x)) != 0,
              "expects the right boundary coordinate is integer multiple of grid_value_x");
    DaliWarns((upper_y % (int) std::round(factor_y)) != 0,
              "expects the top boundary coordinate is integer multiple of grid_value_y");

    design_.die_area_offset_x_ = lower_x % (int) std::round(factor_x);
    design_.die_area_offset_y_ = lower_y % (int) std::round(factor_y);

    int left = lower_x / (int) std::round(factor_x);
    int bottom = lower_y / (int) std::round(factor_y);
    int right =
        (upper_x - design_.die_area_offset_x_) / (int) std::round(factor_x);
    int top =
        (upper_y - design_.die_area_offset_y_) / (int) std::round(factor_y);

    SetBoundary(left, bottom, right, top);
}

// return lower x of the placement region, unit is grid value in x.
int Circuit::RegionLLX() const {
    return design_.region_left_;
}

// return upper x of the placement region, unit is grid value in x.
int Circuit::RegionURX() const {
    return design_.region_right_;
}

// return lower y of the placement region, unit is grid value in y.
int Circuit::RegionLLY() const {
    return design_.region_bottom_;
}

// return upper y of the placement region, unit is grid value in y.
int Circuit::RegionURY() const {
    return design_.region_top_;
}

// return width of the placement region, unit is grid value in x.
int Circuit::RegionWidth() const {
    return design_.region_right_ - design_.region_left_;
}

// return height of the placement region, unit is grid value in y.
int Circuit::RegionHeight() const {
    return design_.region_top_ - design_.region_bottom_;
}

int Circuit::Micron2DatabaseUnit(double x) {
    return (int) std::round(x * DatabaseMicron());
}

double Circuit::DatabaseUnit2Micron(int x) {
    return ((double) x) / DatabaseMicron();
}

int Circuit::LocDali2PhydbX(double loc) {
    double factor_x = DistanceMicrons() * GridValueX();
    return (int) (loc * factor_x) + design().die_area_offset_x_;
}

int Circuit::LocDali2PhydbY(double loc) {
    double factor_y = DistanceMicrons() * GridValueY();
    return (int) (loc * factor_y) + design().die_area_offset_y_;
}

double Circuit::LocPhydb2DaliX(int loc) {
    double factor_x = DistanceMicrons() * GridValueX();
    return (loc - design().die_area_offset_x_) / factor_x;
}

double Circuit::LocPhydb2DaliY(int loc) {
    double factor_y = DistanceMicrons() * GridValueY();
    return (loc - design().die_area_offset_y_) / factor_y;
}

void Circuit::AddIoPinFromPhyDB(phydb::IOPin &iopin) {
    std::string iopin_name(iopin.GetName());
    auto location = iopin.GetLocation();
    int iopin_x = LocPhydb2DaliX(location.x);
    int iopin_y = LocPhydb2DaliY(location.y);
    bool is_loc_set = (iopin.GetPlacementStatus() != phydb::UNPLACED);
    auto sig_use = SignalUse(iopin.GetUse());
    auto sig_dir = SignalDirection(iopin.GetDirection());

    if (is_loc_set) {
        IoPin *pin =
            AddIoPin(iopin_name, PLACED, sig_use, sig_dir, iopin_x, iopin_y);
        std::string layer_name = iopin.GetLayerName();
        MetalLayer *metal_layer = GetMetalLayerPtr(layer_name);
        pin->SetLayerPtr(metal_layer);
    } else {
        AddIoPin(iopin_name, UNPLACED, sig_use, sig_dir);
    }
}

void Circuit::LoadDesign(phydb::PhyDB *phy_db_ptr) {
    auto &phy_db_design = *(phy_db_ptr->GetDesignPtr());

    // 1. reserve space for COMPONENTS, IOPINS, and NETS
    auto &components = phy_db_design.GetComponentsRef();
    int components_count = (int) components.size();

    auto &iopins = phy_db_design.GetIoPinsRef();
    int pins_count = (int) iopins.size();

    auto &nets = phy_db_design.GetNetsRef();
    int nets_count = (int) nets.size();
    SetListCapacity(components_count, pins_count, nets_count);

    BOOST_LOG_TRIVIAL(info) << "components count: " << components_count << "\n";
    BOOST_LOG_TRIVIAL(info) << "pins count:       " << pins_count << "\n";
    BOOST_LOG_TRIVIAL(info) << "nets count:       " << nets_count << "\n";


    // 2. load UNITS DISTANCE MICRONS and DIEAREA
    SetUnitsDistanceMicrons(phy_db_design.GetUnitsDistanceMicrons());
    auto die_area = phy_db_design.GetDieArea();
    SetDieArea(die_area.LLX(), die_area.LLY(), die_area.URX(), die_area.URY());

    // 3. load all components
    for (auto &comp: components) {
        std::string blk_name(comp.GetName());
        std::string blk_type_name(comp.GetMacro()->GetName());
        auto location = comp.GetLocation();
        int llx = location.x;
        int lly = location.y;
        double lx = std::round(LocPhydb2DaliX(llx));
        double ly = std::round(LocPhydb2DaliY(lly));
        auto place_status = PlaceStatus(comp.GetPlacementStatus());
        auto orient = BlockOrient(comp.GetOrientation());
        AddBlock(blk_name, blk_type_name, lx, ly, place_status, orient);
    }

    // 4. load all IOPINs
    for (auto &iopin: iopins) {
        AddIoPinFromPhyDB(iopin);
    }

    // 5. load all NETs
    for (auto &net: nets) {
        std::string net_name(net.GetName());
        auto &net_pins = net.GetPinsRef();
        auto &iopin_names = net.GetIoPinNamesRef();
        int net_capacity = int(net_pins.size() + iopin_names.size());
        AddNet(net_name, net_capacity, design_.normal_signal_weight_);

        for (auto &iopin_name: iopin_names) {
            AddIOPinToNet(iopin_name, net_name);
        }
        int sz = (int) net_pins.size();
        for (int i = 0; i < sz; ++i) {
            int comp_id = net_pins[i].comp_id;
            std::string comp_name =
                phy_db_ptr_->GetDesignPtr()->GetComponentsRef()[comp_id].GetName();
            int pin_id = net_pins[i].pin_id;
            std::string pin_name =
                phy_db_ptr_->GetDesignPtr()->GetComponentsRef()[comp_id].GetMacro()->GetPinsRef()[pin_id].GetName();
            AddBlkPinToNet(comp_name, pin_name, net_name);
        }
    }
}

void Circuit::LoadWell(phydb::PhyDB *phy_db_ptr) {
    auto &phy_db_tech = *(phy_db_ptr->GetTechPtr());
    if (!phy_db_tech.IsWellInfoSet()) {
        BOOST_LOG_TRIVIAL(info) << "N/P-Well layer info not found in PhyDB\n";
        return;
    }

    double same_diff_spacing = 0, any_diff_spacing = 0;
    phy_db_tech.GetDiffWellSpacing(same_diff_spacing, any_diff_spacing);
    SetLegalizerSpacing(same_diff_spacing, any_diff_spacing);

    auto *n_well_layer = phy_db_tech.GetNwellLayerPtr();
    if (n_well_layer != nullptr) {
        double width = n_well_layer->GetWidth();
        double spacing = n_well_layer->GetSpacing();
        double op_spacing = n_well_layer->GetOpSpacing();
        double max_plug_dist = n_well_layer->GetMaxPlugDist();
        double overhang = n_well_layer->GetOverhang();
        SetNWellParams(width, spacing, op_spacing, max_plug_dist, overhang);
    }

    auto *p_well_layer = phy_db_tech.GetPwellLayerPtr();
    if (p_well_layer != nullptr) {
        double width = p_well_layer->GetWidth();
        double spacing = p_well_layer->GetSpacing();
        double op_spacing = p_well_layer->GetOpSpacing();
        double max_plug_dist = p_well_layer->GetMaxPlugDist();
        double overhang = p_well_layer->GetOverhang();
        SetPWellParams(width, spacing, op_spacing, max_plug_dist, overhang);
    }

    for (auto &macro: phy_db_tech.GetMacrosRef()) {
        std::string macro_name(macro.GetName());
        double width = macro.GetWidth();
        double height = macro.GetHeight();
        BlockType *blk_type = GetBlockTypePtr(macro_name);
        auto *macro_well = macro.GetWellPtr();
        DaliExpects(macro_well != nullptr,
                    "No well info provided for MACRO: " + macro_name);
        BlockTypeWell *well = AddBlockTypeWell(macro_name);

        auto *n_rect = macro_well->GetNwellRectPtr();
        auto *p_rect = macro_well->GetPwellRectPtr();
        DaliExpects(n_rect != nullptr || p_rect != nullptr,
                    "N/P-well geometries not provided for MACRO: "
                        + macro_name);
        if (n_rect != nullptr) {
            setWellRect(
                macro_name,
                true,
                n_rect->LLX(),
                n_rect->LLY(),
                n_rect->URX(),
                n_rect->URY()
            );
        }
        if (p_rect != nullptr) {
            setWellRect(
                macro_name,
                false,
                p_rect->LLX(),
                p_rect->LLY(),
                p_rect->URX(),
                p_rect->URY()
            );
        }
    }
}

void Circuit::InitializeFromPhyDB(phydb::PhyDB *phy_db_ptr) {
    DaliExpects(phy_db_ptr != nullptr,
                "Dali cannot initialize from a PhyDB which is a nullptr!");
    phy_db_ptr_ = phy_db_ptr;

    LoadTech(phy_db_ptr);
    LoadDesign(phy_db_ptr);
    LoadWell(phy_db_ptr);
}

Tech &Circuit::tech() {
    return tech_;
}

Design &Circuit::design() {
    return design_;
}

void Circuit::SetDatabaseMicron(int database_micron) {
    DaliExpects(database_micron > 0,
                "Cannot set negative database microns: Circuit::SetDatabaseMicron()");
    tech_.database_microns_ = database_micron;
}

int Circuit::DatabaseMicron() const {
    return tech_.database_microns_;
}

// set manufacturing grid
void Circuit::SetManufacturingGrid(double manufacture_grid) {
    DaliExpects(manufacture_grid > 0,
                "Cannot set negative manufacturing grid: Circuit::setmanufacturing_grid_");
    tech_.manufacturing_grid_ = manufacture_grid;
}

// get manufacturing grid
double Circuit::ManufacturingGrid() const {
    return tech_.manufacturing_grid_;
}

// set grid values in x and y direction, unit in micron
void Circuit::SetGridValue(double grid_value_x, double grid_value_y) {
    DaliExpects(!tech_.is_grid_set_,
                "once set, grid_value cannot be changed!");
    DaliExpects(grid_value_x > 0,
                "grid_value_x must be a positive real number!");
    DaliExpects(grid_value_y > 0,
                "grid_value_y must be a positive real number!");
    //BOOST_LOG_TRIVIAL(info) << "  grid value x: " << grid_value_x << ", grid value y: " << grid_value_y << "\n";
    tech_.grid_value_x_ = grid_value_x;
    tech_.grid_value_y_ = grid_value_y;
    tech_.is_grid_set_ = true;
}

// set grid values using the first horizontal and vertical metal layer pitches
void Circuit::SetGridFromMetalPitch() {
    DaliExpects(tech_.metal_list_.size() >= 2,
                "No enough metal layer for determining grid value in x and y! Circuit::SetGridFromMetalPitch()");
    MetalLayer *hor_layer = nullptr;
    MetalLayer *ver_layer = nullptr;
    for (auto &metal_layer: tech_.metal_list_) {
        if (hor_layer == nullptr && metal_layer.Direction() == HORIZONTAL) {
            hor_layer = &metal_layer;
        }
        if (ver_layer == nullptr && metal_layer.Direction() == VERTICAL) {
            ver_layer = &metal_layer;
        }
    }
    DaliExpects(hor_layer != nullptr,
                "Cannot find a horizontal metal layer! Circuit::SetGridFromMetalPitch()");
    DaliExpects(ver_layer != nullptr,
                "Cannot find a vertical metal layer! Circuit::SetGridFromMetalPitch()");
    //BOOST_LOG_TRIVIAL(info)   << "vertical layer: " << *ver_layer->Name() << "  " << ver_layer->PitchX() << "\n";
    //BOOST_LOG_TRIVIAL(info)   << "horizontal layer: " << *hor_layer->Name() << "  " << hor_layer->PitchY() << "\n";
    SetGridValue(ver_layer->PitchX(), hor_layer->PitchY());
}

// get the grid value in x direction, unit is usually in micro
double Circuit::GridValueX() const {
    DaliExpects(tech_.is_grid_set_, "Need to set grid value before use");
    return tech_.grid_value_x_;
}

// get the grid value in y direction, unit is usually in micro
double Circuit::GridValueY() const {
    DaliExpects(tech_.is_grid_set_, "Need to set grid value before use");
    return tech_.grid_value_y_;
}

// set the row height, unit in micron
void Circuit::SetRowHeight(double row_height) {
    DaliExpects(row_height > 0, "Setting row height to a negative value?");
    //BOOST_LOG_TRIVIAL(info) << row_height << "  " << GridValueY() << std::endl;
    double residual = Residual(row_height, GridValueY());
    DaliExpects(std::fabs(residual) < EPSILON,
                "Site height is not integer multiple of grid value in Y");
    tech_.row_height_set_ = true;
    tech_.row_height_ = row_height;
}

// get the row height in micro
double Circuit::RowHeightMicron() const {
    return tech_.row_height_;
}

// get the row height in grid value y
int Circuit::RowHeightGrid() const {
    DaliExpects(tech_.row_height_set_,
                "Row height not set, cannot retrieve its value: Circuit::RowHeightGrid()\n");
    return (int) std::round(tech_.row_height_ / GridValueY());
}

std::vector<MetalLayer> &Circuit::Metals() {
    return tech_.metal_list_;
}

std::unordered_map<std::string, int> &Circuit::MetalNameMap() {
    return tech_.metal_name_map_;
}

// check if a metal layer with given name exists or not
bool Circuit::IsMetalLayerExisting(std::string const &metal_name) {
    return tech_.metal_name_map_.find(metal_name)
        != tech_.metal_name_map_.end();
}

// get the index of a metal layer
int Circuit::GetMetalLayerId(std::string const &metal_name) {
    DaliExpects(IsMetalLayerExisting(metal_name),
                "MetalLayer does not exist, cannot find it: " + metal_name);
    return tech_.metal_name_map_.find(metal_name)->second;
}

// get a pointer to the metal layer with a given name
MetalLayer *Circuit::GetMetalLayerPtr(std::string const &metal_name) {
    DaliExpects(IsMetalLayerExisting(metal_name),
                "MetalLayer does not exist, cannot find it: " + metal_name);
    return &tech_.metal_list_[GetMetalLayerId(metal_name)];
}

// add a metal layer with name and size, unit in micron.
MetalLayer *Circuit::AddMetalLayer(
    std::string const &metal_name,
    double width,
    double spacing
) {
    DaliExpects(!IsMetalLayerExisting(metal_name),
                "MetalLayer exist, cannot create this MetalLayer again: "
                    + metal_name);
    int map_size = tech_.metal_name_map_.size();
    auto ret = tech_.metal_name_map_.insert(
        std::pair<std::string, int>(metal_name, map_size)
    );
    std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
    tech_.metal_list_.emplace_back(width, spacing, name_id_pair_ptr);
    return &(tech_.metal_list_.back());
}

// add a metal layer with name;
MetalLayer *Circuit::AddMetalLayer(std::string &metal_name) {
    return AddMetalLayer(metal_name, 0, 0);
}

// set the boundary of the placement region, unit is in corresponding grid value.
void Circuit::SetBoundary(int left, int bottom, int right, int top) {
    DaliExpects(right > left,
                "Right boundary is not larger than Left boundary?");
    DaliExpects(top > bottom,
                "Top boundary is not larger than Bottom boundary?");
    design_.region_left_ = left;
    design_.region_right_ = right;
    design_.region_bottom_ = bottom;
    design_.region_top_ = top;
    design_.die_area_set_ = true;
}

// add a metal layer, unit in micron.
void Circuit::AddMetalLayer(
    std::string const &metal_name,
    double width,
    double spacing,
    double min_area,
    double pitch_x,
    double pitch_y,
    MetalDirection metal_direction
) {
    DaliExpects(!IsMetalLayerExisting(metal_name),
                "MetalLayer exist, cannot create this MetalLayer again: "
                    + metal_name);
    int map_size = tech_.metal_name_map_.size();
    auto ret = tech_.metal_name_map_.insert(
        std::pair<std::string, int>(metal_name, map_size)
    );
    std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
    tech_.metal_list_.emplace_back(width, spacing, name_id_pair_ptr);
    tech_.metal_list_.back().SetMinArea(min_area);
    tech_.metal_list_.back().SetPitch(pitch_x, pitch_y);
    tech_.metal_list_.back().SetDirection(metal_direction);
}

// report metal layer information for debugging purposes
void Circuit::ReportMetalLayers() {
    BOOST_LOG_TRIVIAL(info)
        << "Total MetalLayer: " << tech_.metal_list_.size() << "\n";
    for (auto &metal_layer: tech_.metal_list_) {
        metal_layer.Report();
    }
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// get the pointer to the unordered BlockType map.
std::unordered_map<std::string, BlockType *> &Circuit::BlockTypeMap() {
    return tech_.block_type_map_;
}

// check if a BlockType with a given name exists or not
bool Circuit::IsBlockTypeExisting(std::string const &block_type_name) {
    return tech_.block_type_map_.find(block_type_name)
        != tech_.block_type_map_.end();
}

// get the pointer to the BlockType with a given name, if not exist, return nullptr.
BlockType *Circuit::GetBlockTypePtr(std::string const &block_type_name) {
    auto res = tech_.block_type_map_.find(block_type_name);
    DaliExpects(res != tech_.block_type_map_.end(),
                "Cannot find block_type_name: " + block_type_name);
    return res->second;
}

// create well information container for a given BlockType.
BlockTypeWell *Circuit::AddBlockTypeWell(BlockType *blk_type) {
    tech_.well_list_.emplace_back(blk_type);
    blk_type->SetWell(&(tech_.well_list_.back()));
    return blk_type->WellPtr();
}

// set N-well layer parameters
void Circuit::SetNWellParams(
    double width,
    double spacing,
    double op_spacing,
    double max_plug_dist,
    double overhang
) {
    tech_.nwell_layer_.SetParams(
        width, spacing, op_spacing, max_plug_dist, overhang
    );
    tech_.n_set_ = true;
}

// set P-well layer parameters
void Circuit::SetPWellParams(
    double width,
    double spacing,
    double op_spacing,
    double max_plug_dist,
    double overhang
) {
    tech_.pwell_layer_.SetParams(
        width, spacing, op_spacing, max_plug_dist, overhang
    );
    tech_.p_set_ = true;
}

// TODO: discuss with Rajit about the necessity of the parameter ANY_SPACING in CELL file.
// set same_spacing (NN and PP) and any_spacing (NP). This member function will be depreciated
void Circuit::SetLegalizerSpacing(double same_spacing, double any_spacing) {
    tech_.same_diff_spacing_ = same_spacing;
    tech_.any_diff_spacing_ = any_spacing;
}

// create well information container for a given BlockType.
BlockTypeWell *Circuit::AddBlockTypeWell(std::string &blk_type_name) {
    BlockType *blk_type_ptr = GetBlockTypePtr(blk_type_name);
    return AddBlockTypeWell(blk_type_ptr);
}

// TODO: discuss with Rajit about the necessity of having N/P-wells not fully covering the prBoundary of a given cell.
// set the N/P-well shape of a given BlockType, unit in micron.
void Circuit::setWellRect(
    std::string &blk_type_name,
    bool is_N,
    double lx,
    double ly,
    double ux,
    double uy
) {
    BlockType *blk_type_ptr = GetBlockTypePtr(blk_type_name);
    DaliExpects(blk_type_ptr != nullptr,
                "Cannot find BlockType with name: " + blk_type_name);
    int lx_grid = int(std::round(lx / GridValueX()));
    int ly_grid = int(std::round(ly / GridValueY()));
    int ux_grid = int(std::round(ux / GridValueX()));
    int uy_grid = int(std::round(uy / GridValueY()));
    BlockTypeWell *well = blk_type_ptr->WellPtr();
    DaliExpects(well != nullptr,
                "Well uninitialized for BlockType: " + blk_type_name);
    well->SetWellRect(is_N, lx_grid, ly_grid, ux_grid, uy_grid);
}

// report the well shape for each BlockType for debugging purposes.
void Circuit::ReportWellShape() {
    for (auto &blk_type_well: tech_.well_list_) {
        blk_type_well.Report();
    }
}

// add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in grid value.
BlockType *Circuit::AddBlockTypeWithGridUnit(
    std::string const &block_type_name,
    int width,
    int height
) {
    DaliExpects(!IsBlockTypeExisting(block_type_name),
                "BlockType exist, cannot create this block type again: "
                    + block_type_name);
    auto ret = tech_.block_type_map_.insert(
        std::pair<std::string, BlockType *>(block_type_name, nullptr)
    );
    auto tmp_ptr = new BlockType(&(ret.first->first), width, height);
    ret.first->second = tmp_ptr;
    if (tmp_ptr->Area() > INT_MAX) tmp_ptr->Report();
    return tmp_ptr;
}

void Circuit::BlockTypeSizeMicrometerToGridValue(
    std::string const &block_type_name,
    double width,
    double height,
    int &gridded_width,
    int &gridded_height
) {
    double residual_x = fabs(Residual(width, GridValueX()));
    gridded_width = -1;
    if (residual_x < EPSILON) {
        gridded_width = (int) std::round(width / GridValueX());
    } else {
        gridded_width = (int) std::ceil(width / GridValueX());
        BOOST_LOG_TRIVIAL(warning)
            << "BlockType width is not integer multiple of the grid value along X: "
            << block_type_name << "\n"
            << "    width: " << width << " um\n"
            << "    grid value x: " << GridValueX() << " um\n"
            << "    residual: " << residual_x << "\n"
            << "    adjusted up to: " << gridded_width << " * "
            << GridValueX() << " um\n";
    }

    double residual_y = fabs(Residual(height, GridValueY()));
    gridded_height = -1;
    if (residual_y < EPSILON) {
        gridded_height = (int) std::round(height / GridValueY());
    } else {
        gridded_height = (int) std::ceil(height / GridValueY());
        BOOST_LOG_TRIVIAL(warning)
            << "BlockType height is not integer multiple of the grid value along Y: "
            << block_type_name << "\n"
            << "    height: " << height << " um\n"
            << "    grid value y: " << GridValueY() << " um\n"
            << "    residual: " << residual_y << "\n"
            << "    adjusted up to: " << gridded_height << " * "
            << GridValueY() << " um\n";
    }
}

BlockType *Circuit::AddBlockType(
    std::string const &block_type_name,
    double width,
    double height
) {
    int gridded_width = 0;
    int gridded_height = 0;
    BlockTypeSizeMicrometerToGridValue(
        block_type_name,
        width,
        height,
        gridded_width,
        gridded_height
    );
    return AddBlockTypeWithGridUnit(
        block_type_name, gridded_width, gridded_height
    );
}

void Circuit::SetBlockTypeSize(
    BlockType *blk_type_ptr,
    double width,
    double height
) {
    int gridded_width = 0;
    int gridded_height = 0;
    BlockTypeSizeMicrometerToGridValue(
        blk_type_ptr->Name(), width, height, gridded_width, gridded_height
    );
    blk_type_ptr->SetSize(gridded_width, gridded_height);
}

// add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in grid value.
BlockType *Circuit::AddWellTapBlockTypeWithGridUnit(
    std::string const &block_type_name,
    int width,
    int height
) {
    BlockType *well_tap_ptr =
        AddBlockTypeWithGridUnit(block_type_name, width, height);
    tech_.well_tap_cell_ptrs_.push_back(well_tap_ptr);
    return well_tap_ptr;
}

// add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in micron.
BlockType *Circuit::AddWellTapBlockType(
    std::string const &block_type_name,
    double width,
    double height
) {
    double residual = fabs(Residual(width, GridValueX()));
    DaliExpects(residual < EPSILON,
                "BlockType width is not integer multiple of grid value in X: "
                    + block_type_name);

    residual = fabs(Residual(height, GridValueY()));
    DaliExpects(residual < EPSILON,
                "BlockType height is not integer multiple of grid value in Y: "
                    + block_type_name);

    int gridded_width = (int) std::round(width / GridValueX());
    int gridded_height = (int) std::round(height / GridValueY());
    return AddWellTapBlockTypeWithGridUnit(
        block_type_name, gridded_width, gridded_height
    );
}

// add a cell pin with a given name to a BlockType, this method is not the optimal one, but it is very safe to use.
Pin *Circuit::AddBlkTypePin(
    std::string const &block_type_name,
    std::string const &pin_name,
    bool is_input
) {
    BlockType *blk_type_ptr = GetBlockTypePtr(block_type_name);
    DaliExpects(blk_type_ptr != nullptr,
                "Cannot add BlockType pins because there is no such a BlockType: "
                    + block_type_name);
    return blk_type_ptr->AddPin(pin_name, is_input);
}

// add a cell pin with a given name to a BlockType, users must guarantee the pointer @param is valid.
Pin *Circuit::AddBlkTypePin(
    BlockType *blk_type_ptr,
    std::string const &pin_name,
    bool is_input
) {
    return blk_type_ptr->AddPin(pin_name, is_input);
}

// add a rectangle to a block pin, this method is not the optimal one, but it is very safe to use. Unit in grid value.
void Circuit::AddBlkTypePinRect(
    std::string const &block_type_name,
    std::string const &pin_name,
    double llx,
    double lly,
    double urx,
    double ury
) {
    BlockType *blk_type_ptr = GetBlockTypePtr(block_type_name);
    DaliExpects(blk_type_ptr != nullptr,
                "Cannot add BlockType pins because there is no such a BlockType: "
                    + block_type_name);
    Pin *pin_ptr = blk_type_ptr->GetPinPtr(pin_name);
    DaliExpects(pin_ptr != nullptr,
                "Cannot add BlockType pins because there is no such a pin: "
                    + block_type_name + "::" + pin_name);
    pin_ptr->AddRect(llx, lly, urx, ury);
}

// add a rectangle to a block pin, users must guarantee the pointer @param is valid. Unit in grid value.
void Circuit::AddBlkTypePinRect(
    Pin *pin_ptr,
    double llx,
    double lly,
    double urx,
    double ury
) {
    pin_ptr->AddRect(llx, lly, urx, ury);
}

void Circuit::SetListCapacity(
    int components_count,
    int pins_count,
    int nets_count
) {
    DaliExpects(components_count >= 0, "Negative number of components?");
    DaliExpects(pins_count >= 0, "Negative number of IOPINs?");
    DaliExpects(nets_count >= 0, "Negative number of NETs?");
    design_.blocks_.reserve(components_count + pins_count);
    design_.iopins_.reserve(pins_count);
    design_.nets_.reserve(nets_count);
}

std::vector<Block> &Circuit::Blocks() {
    return design_.blocks_;
}

// check if a block with the given name exists or not.
bool Circuit::IsBlockExisting(std::string const &block_name) {
    return !(design_.blk_name_id_map_.find(block_name)
        == design_.blk_name_id_map_.end());
}

// returns the index of a block with a given name. Users must guarantee the given name is valid.
int Circuit::GetBlockId(std::string const &block_name) {
    return design_.blk_name_id_map_.find(block_name)->second;
}

// returns a pointer to the block with a given name. Users must guarantee the given name is valid.
Block *Circuit::GetBlockPtr(std::string const &block_name) {
    return &design_.blocks_[GetBlockId(block_name)];
}

// report the whole BlockType list for debugging purposes.
void Circuit::ReportBlockType() {
    BOOST_LOG_TRIVIAL(info) << "Total BlockType: "
                            << tech_.block_type_map_.size() << std::endl;
    for (auto &pair: tech_.block_type_map_) {
        pair.second->Report();
    }
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// create BlockTypes by copying from another Circuit instance.
void Circuit::CopyBlockType(Circuit &circuit) {
    BlockType *blk_type = nullptr;
    BlockType *blk_type_new = nullptr;
    for (auto &item: circuit.tech_.block_type_map_) {
        blk_type = item.second;
        auto type_name = blk_type->Name();
        if (type_name == "PIN") continue;
        blk_type_new = AddBlockTypeWithGridUnit(
            type_name,
            blk_type->Width(),
            blk_type->Height()
        );
        for (auto &pin: blk_type->PinList()) {
            blk_type_new->AddPin(pin.Name(), pin.OffsetX(), pin.OffsetY());
        }
    }
}

// set DEF UNITS DISTANCE MICRONS
void Circuit::SetUnitsDistanceMicrons(int distance_microns) {
    DaliExpects(distance_microns > 0, "Negative distance micron?");
    design_.distance_microns_ = distance_microns;
}

// return DEF UNITS DISTANCE MICRONS
int Circuit::DistanceMicrons() const {
    return design_.distance_microns_;
}

// create a block instance using a pointer to its type
void Circuit::AddBlock(
    std::string const &block_name,
    BlockType *block_type_ptr,
    double llx,
    double lly,
    PlaceStatus place_status,
    BlockOrient orient,
    bool is_real_cel
) {
    DaliExpects(design_.nets_.empty(),
                "Cannot add new Block, because net_list now is not empty");
    DaliExpects(design_.blocks_.size() < design_.blocks_.capacity(),
                "Cannot add new Block, because block list is full");
    DaliExpects(!IsBlockExisting(block_name),
                "Block exists, cannot create this block again: " + block_name);
    int map_size = design_.blk_name_id_map_.size();
    auto ret = design_.blk_name_id_map_.insert(std::pair<std::string, int>(
        block_name,
        map_size));
    std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
    design_.blocks_.emplace_back(
        block_type_ptr, name_id_pair_ptr, llx, lly, place_status, orient
    );

    // update statistics of blocks
    long int old_tot_area = design_.tot_blk_area_;
    design_.tot_blk_area_ += design_.blocks_.back().Area();
    DaliExpects(old_tot_area <= design_.tot_blk_area_,
                "Total Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
    design_.tot_width_ += design_.blocks_.back().Width();
    design_.tot_height_ += design_.blocks_.back().Height();
    if (is_real_cel) {
        ++design_.real_block_count_;
    }
    if (design_.blocks_.back().IsMovable()) {
        ++design_.tot_mov_blk_num_;
        old_tot_area = design_.tot_mov_block_area_;
        design_.tot_mov_block_area_ += design_.blocks_.back().Area();
        DaliExpects(old_tot_area <= design_.tot_mov_block_area_,
                    "Total Movable Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
        design_.tot_mov_width_ += design_.blocks_.back().Width();
        design_.tot_mov_height_ += design_.blocks_.back().Height();
    }
    if (design_.blocks_.back().Height() < design_.blk_min_height_) {
        design_.blk_min_height_ = design_.blocks_.back().Height();
    }
    if (design_.blocks_.back().Height() > design_.blk_max_height_) {
        design_.blk_max_height_ = design_.blocks_.back().Height();
    }
    if (design_.blocks_.back().Width() < design_.blk_min_width_) {
        design_.blk_min_width_ = design_.blocks_.back().Width();
    }
    if (design_.blocks_.back().Width() > design_.blk_min_width_) {
        design_.blk_max_width_ = design_.blocks_.back().Width();
    }
}

// create a block instance using the name of its type
void Circuit::AddBlock(
    std::string const &block_name,
    std::string const &block_type_name,
    double llx,
    double lly,
    PlaceStatus place_status,
    BlockOrient orient,
    bool is_real_cel
) {
    BlockType *block_type = GetBlockTypePtr(block_type_name);
    AddBlock(
        block_name, block_type, llx, lly, place_status, orient, is_real_cel
    );
}

// create a dummy BlockType for IOPins.
void Circuit::AddDummyIOPinBlockType() {
    /****
     * This member function adds a dummy BlockType for IOPINs.
     * The name of this dummy BlockType is "PIN", and it contains one cell pin with name "pin".
     * The size of "PIN" BlockType is 0 (width) and 0(height).
     * The relative location of the only cell pin "pin" is (0,0) with size 0.
     * ****/
    std::string iopin_type_name("__PIN__");
    auto io_pin_type = AddBlockTypeWithGridUnit(iopin_type_name, 0, 0);
    std::string tmp_pin_name("pin");
    // TO-DO, the value of @param is_input may not be true
    Pin *pin = io_pin_type->AddPin(tmp_pin_name, true);
    pin->AddRect(0, 0, 0, 0);
    tech_.io_dummy_blk_type_ptr_ = io_pin_type;
}

// add an unplaced IOPin.
IoPin *Circuit::AddUnplacedIOPin(std::string const &iopin_name) {
    DaliExpects(design_.nets_.empty(),
                "Cannot add new IOPIN, because net_list now is not empty");
    DaliExpects(!IsIoPinExisting(iopin_name),
                "IOPin exists, cannot create this IOPin again: " + iopin_name);
    int map_size = design_.iopin_name_id_map_.size();
    auto ret = design_.iopin_name_id_map_.insert(
        std::pair<std::string, int>(iopin_name, map_size)
    );
    std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
    design_.iopins_.emplace_back(name_id_pair_ptr);
    return &(design_.iopins_.back());
}

// add a placed IOPin.
IoPin *Circuit::AddPlacedIOPin(
    std::string const &iopin_name,
    double lx,
    double ly
) {
    DaliExpects(design_.nets_.empty(),
                "Cannot add new IOPIN, because net_list now is not empty");
    DaliExpects(!IsIoPinExisting(iopin_name),
                "IOPin exists, cannot create this IOPin again: " + iopin_name);
    int map_size = (int) design_.iopin_name_id_map_.size();
    auto ret = design_.iopin_name_id_map_.insert(
        std::pair<std::string, int>(iopin_name, map_size)
    );
    std::pair<const std::string, int> *name_id_pair_ptr = &(*ret.first);
    design_.iopins_.emplace_back(name_id_pair_ptr, lx, ly);
    design_.pre_placed_io_count_ += 1;

    // add a dummy cell corresponding to this IOPIN to block_list.
    AddBlock(
        iopin_name, tech_.io_dummy_blk_type_ptr_, lx, ly, PLACED, N, false
    );

    return &(design_.iopins_.back());
}

// get the pointer to the IOPin list.
std::vector<IoPin> &Circuit::IoPins() {
    return design_.iopins_;
}

// check if an IOPin with a given name exists or not.
bool Circuit::IsIoPinExisting(std::string const &iopin_name) {
    return !(design_.iopin_name_id_map_.find(iopin_name)
        == design_.iopin_name_id_map_.end());
}

// returns the index of the IOPin with a given name. Users must guarantee the given name is valid.
int Circuit::GetIoPinId(std::string const &iopin_name) {
    return design_.iopin_name_id_map_.find(iopin_name)->second;
}

// returns a pointer to the IOPin with a given name. Users must guarantee the given name is valid.
IoPin *Circuit::GetIoPinPtr(std::string const &iopin_name) {
    return &design_.iopins_[GetIoPinId(iopin_name)];
}

// add an INPin
IoPin *Circuit::AddIoPin(
    std::string const &iopin_name,
    PlaceStatus place_status,
    SignalUse signal_use,
    SignalDirection signal_direction,
    double lx,
    double ly
) {
    IoPin *io_pin = nullptr;
    if (place_status == UNPLACED) {
        io_pin = AddUnplacedIOPin(iopin_name);
    } else {
        io_pin = AddPlacedIOPin(iopin_name, lx, ly);
        io_pin->SetInitPlaceStatus(place_status);
    }
    io_pin->SetSigUse(signal_use);
    io_pin->SetSigDirection(signal_direction);
    return io_pin;
}

// report the whole IOPin list for debugging purposes.
void Circuit::ReportIOPin() {
    BOOST_LOG_TRIVIAL(info)
        << "Total IOPin: " << design_.iopins_.size() << "\n";
    for (auto &iopin: design_.iopins_) {
        iopin.Report();
    }
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// get the pointer to the net list.
std::vector<Net> *Circuit::getNetList() {
    return &(design_.nets_);
}

std::vector<Net> &Circuit::NetListRef() {
    return design_.nets_;
}

// check if a Net with a given name exists or not.
bool Circuit::IsNetExist(std::string &net_name) {
    return !(design_.net_name_id_map_.find(net_name)
        == design_.net_name_id_map_.end());
}

// returns the index of the Net with a given name. Users must guarantee the given name is valid.
int Circuit::NetIndex(std::string &net_name) {
    return design_.net_name_id_map_.find(net_name)->second;
}

// returns a pointer to the Net with a given name. Users must guarantee the given name is valid.
Net *Circuit::getNetPtr(std::string &net_name) {
    return &design_.nets_[NetIndex(net_name)];
}

// add a net with given name and capacity (number of cell pins), net weight is default 1.
/****
 * Returns a pointer to the newly created Net.
 * @param net_name: name of the net
 * @param capacity: maximum number of possible pins in this net
 * @param weight:   weight of this net
 * ****/
Net *Circuit::AddNet(std::string &net_name, int capacity, double weight) {
    DaliExpects(!IsNetExist(net_name),
                "Net exists, cannot create this net again: " + net_name);
    int map_size = (int) design_.net_name_id_map_.size();
    design_.net_name_id_map_.insert(
        std::pair<std::string, int>(net_name, map_size)
    );
    std::pair<const std::string, int> *name_id_pair_ptr =
        &(*design_.net_name_id_map_.find(net_name));
    design_.nets_.emplace_back(name_id_pair_ptr, capacity, weight);
    return &design_.nets_.back();
}

// add a IOPin to a net.
void Circuit::AddIOPinToNet(std::string &iopin_name, std::string &net_name) {
    IoPin *iopin = GetIoPinPtr(iopin_name);
    Net *io_net = getNetPtr(net_name);
    iopin->SetNetPtr(io_net);
    io_net->AddIoPin(iopin);
    if (iopin->IsPrePlaced()) {
        Block *blk_ptr = GetBlockPtr(iopin_name);
        Pin *pin = &(blk_ptr->TypePtr()->PinList()[0]);
        io_net->AddBlockPinPair(blk_ptr, pin);
    }
}

// add a block pin to a net.
void Circuit::AddBlkPinToNet(
    std::string &blk_name,
    std::string &pin_name,
    std::string &net_name
) {
    Block *blk_ptr = GetBlockPtr(blk_name);
    Pin *pin = blk_ptr->TypePtr()->GetPinPtr(pin_name);
    Net *net = getNetPtr(net_name);
    net->AddBlockPinPair(blk_ptr, pin);
}

// report the whole Block list for debugging purposes.
void Circuit::ReportBlockList() {
    BOOST_LOG_TRIVIAL(info) << "Total Block: " << design_.blocks_.size()
                            << "\n";
    for (auto &block: design_.blocks_) {
        block.Report();
    }
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// report the whole Block map for debugging purposes.
void Circuit::ReportBlockMap() {
    for (auto &it: design_.blk_name_id_map_) {
        BOOST_LOG_TRIVIAL(info) << it.first << " " << it.second << "\n";
    }
}

// report the net list for debugging purposes.
void Circuit::ReportNetList() {
    BOOST_LOG_TRIVIAL(info)
        << "Total Net: " << design_.nets_.size() << "\n";
    for (auto &net: design_.nets_) {
        BOOST_LOG_TRIVIAL(info)
            << "  " << net.Name() << "  " << net.Weight() << "\n";
        for (auto &block_pin: net.BlockPins()) {
            BOOST_LOG_TRIVIAL(info)
                << "\t" << " ("
                << block_pin.BlockName() << " "
                << block_pin.PinName() << ") "
                << "\n";
        }
    }
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// report the net map for debugging purposes.
void Circuit::ReportNetMap() {
    for (auto &it: design_.net_name_id_map_) {
        BOOST_LOG_TRIVIAL(info) << it.first << " " << it.second << "\n";
    }
}

// initialize data structure for net fanout histogram
void Circuit::InitNetFanoutHistogram(std::vector<int> *histo_x) {
    design_.InitNetFanoutHisto(histo_x);
    design_.net_histogram_.hpwl_unit_ = GridValueX();
}

// update HPWL values for the net fanout histogram
void Circuit::UpdateNetHPWLHistogram() {
    int bin_count = (int) design_.net_histogram_.bin_list_.size();
    design_.net_histogram_.sum_hpwl_.assign(bin_count, 0);
    design_.net_histogram_.ave_hpwl_.assign(bin_count, 0);
    design_.net_histogram_.min_hpwl_.assign(bin_count, DBL_MAX);
    design_.net_histogram_.max_hpwl_.assign(bin_count, DBL_MIN);

    for (auto &net: design_.nets_) {
        int net_size = net.PinCnt();
        double hpwl_x = net.WeightedHPWLX();
        double hpwl_y =
            net.WeightedHPWLY() * GridValueY() / GridValueX();
        design_.UpdateNetHPWLHisto(net_size, hpwl_x + hpwl_y);
    }

    design_.net_histogram_.tot_hpwl_ = 0;
    for (int i = 0; i < bin_count; ++i) {
        design_.net_histogram_.tot_hpwl_ += design_.net_histogram_.sum_hpwl_[i];
    }
}

// report the net fanout histogram
void Circuit::ReportNetFanoutHistogram() {
    UpdateNetHPWLHistogram();
    design_.ReportNetFanoutHisto();
}

// report brief summary of this circuit.
void Circuit::ReportBriefSummary() const {
    BOOST_LOG_TRIVIAL(info) << "  movable blocks: " << TotMovableBlockCount()
                            << "\n";
    BOOST_LOG_TRIVIAL(info) << "  blocks: " << TotBlkCount() << "\n";
    BOOST_LOG_TRIVIAL(info) << "  iopins: " << design_.iopins_.size()
                            << "\n";
    BOOST_LOG_TRIVIAL(info) << "  nets: " << design_.nets_.size() << "\n";
    BOOST_LOG_TRIVIAL(info) << "  grid size x/y: " << GridValueX()
                            << ", " << GridValueY() << " um\n";
    BOOST_LOG_TRIVIAL(info) << "  total block area: " << design_.tot_blk_area_
                            << "\n";
    BOOST_LOG_TRIVIAL(info) << "  total white space: "
                            << (long int) RegionWidth()
                                * (long int) RegionHeight() << "\n";
    BOOST_LOG_TRIVIAL(info) << "    left:   " << RegionLLX() << "\n";
    BOOST_LOG_TRIVIAL(info) << "    right:  " << RegionURX() << "\n";
    BOOST_LOG_TRIVIAL(info) << "    bottom: " << RegionLLY() << "\n";
    BOOST_LOG_TRIVIAL(info) << "    top:    " << RegionURY() << "\n";
    BOOST_LOG_TRIVIAL(info) << "  average movable width, height: "
                            << AveMovBlkWidth() << ", " << AveMovBlkHeight()
                            << "\n";
    BOOST_LOG_TRIVIAL(info) << "  white space utility: " << WhiteSpaceUsage()
                            << "\n";
}

// returns the minimum width of blocks.
int Circuit::MinBlkWidth() const { return design_.blk_min_width_; }

// returns the maximum width of blocks, for checking if the placement region is too narrow, and so on.
int Circuit::MaxBlkWidth() const { return design_.blk_max_width_; }

// returns the minimum height of blocks.
int Circuit::MinBlkHeight() const { return design_.blk_min_height_; }

// returns the maximum height of blocks, for checking if the placement region is too short, and so on.
int Circuit::MaxBlkHeight() const { return design_.blk_max_height_; }

// returns the total block area, for checking if the placement region is big enough.
long int Circuit::TotBlkArea() const { return design_.tot_blk_area_; }

// returns the total number of blocks.
int Circuit::TotBlkCount() const { return design_.real_block_count_; }

// returns the total number of movable blocks.
int Circuit::TotMovableBlockCount() const { return design_.tot_mov_blk_num_; }

// returns the total number of fixed blocks.
int Circuit::TotFixedBlkCnt() const {
    return (int) design_.blocks_.size() - design_.tot_mov_blk_num_;
}

// returns the average width of blocks.
double Circuit::AveBlkWidth() const {
    return double(design_.tot_width_) / double(TotBlkCount());
}

// returns the average height of blocks.
double Circuit::AveBlkHeight() const {
    return double(design_.tot_height_) / double(TotBlkCount());
}

// returns the average area of blocks.
double Circuit::AveBlkArea() const {
    return double(design_.tot_blk_area_) / double(TotBlkCount());
}

// returns the average width of movable blocks.
double Circuit::AveMovBlkWidth() const {
    return double(design_.tot_mov_width_) / TotMovableBlockCount();
}

// returns the average height of movable blocks.
double Circuit::AveMovBlkHeight() const {
    return double(design_.tot_mov_height_) / TotMovableBlockCount();
}

// returns the average area of movable blocks.
double Circuit::AveMovBlkArea() const {
    return double(design_.tot_mov_block_area_) / TotMovableBlockCount();
}

// returns the white space usage ratio.
double Circuit::WhiteSpaceUsage() const {
    return double(TotBlkArea()) / double(RegionURX() - RegionLLX())
        / double(RegionURY() - RegionLLY());
}

void Circuit::BuildBlkPairNets() {
    // for each net, we decompose it, and enumerate all driver-load pair
    for (auto &net: design_.nets_) {
        //BOOST_LOG_TRIVIAL(info)   << net.NameStr() << "\n";
        int sz = static_cast<int>(net.BlockPins().size());
        int driver_index = -1;
        // find if there is a driver pin in this net
        // if a net contains a unplaced IOPin, then there might be no driver pin, this case is ignored for now
        // we also assume there is only one driver pin
        for (int i = 0; i < sz; ++i) {
            BlkPinPair &blk_pin_pair = net.BlockPins()[i];
            if (!(blk_pin_pair.PinPtr()->IsInput())) {
                driver_index = i;
                break;
            }
        }
        if (driver_index == -1) continue;

        int driver_blk_num = net.BlockPins()[driver_index].BlkId();
        for (int i = 0; i < sz; ++i) {
            if (i == driver_index) continue;
            int load_blk_num = net.BlockPins()[i].BlkId();
            if (driver_blk_num == load_blk_num) continue;
            int num0 = std::max(driver_blk_num, load_blk_num);
            int num1 = std::min(driver_blk_num, load_blk_num);
            std::pair<int, int> key = std::make_pair(num0, num1);
            if (blk_pair_map_.find(key) == blk_pair_map_.end()) {
                int val = int(blk_pair_net_list_.size());
                blk_pair_map_.insert({key, val});
                blk_pair_net_list_.emplace_back(num0, num1);
            }
            int pair_index = blk_pair_map_[key];
            int load_index = i;
            blk_pair_net_list_[pair_index].edges.emplace_back(
                &net, driver_index, load_index
            );
        }
    }

    // sort driver-load pair for better memory access
//  std::sort(blk_pair_net_list_.begin(),
//            blk_pair_net_list_.end(),
//            [](const BlkPairNets &blk_pair0, const BlkPairNets &blk_pair1) {
//              if ((blk_pair0.blk_num0 < blk_pair1.blk_num0) ||
//                  (blk_pair0.blk_num0 == blk_pair1.blk_num0 && blk_pair0.blk_num1 < blk_pair1.blk_num1)) {
//                return true;
//              }
//            });
//  blk_pair_map_.clear();
//  int pair_net_sz = blk_pair_net_list_.size();
//  for (int i = 0; i < pair_net_sz; ++i) {
//    int num0 = blk_pair_net_list_[i].blk_num0;
//    int num1 = blk_pair_net_list_[i].blk_num1;
//    std::pair<int, int> key = std::make_pair(num0, num1);
//    blk_pair_map_.insert({key, i});
//  }

//  std::ofstream ost("test.txt");
//  //int pair_net_sz = blk_pair_net_list_.size();
//  for (int i=0; i<pair_net_sz; ++i) {
//    int num0 = blk_pair_net_list_[i].blk_num0;
//    int num1 = blk_pair_net_list_[i].blk_num1;
//    ost << "-(" << num0 << " " << design_.block_list[num0].Name() << ", "
//                << num1 << " " << design_.block_list[num1].Name() << ")" << "\n";
//    for (auto &edge: blk_pair_net_list_[i].edges) {
//      Net *net_ptr = edge.net;
//      int d_index = edge.d;
//      int l_index = edge.l;
//      ost << "\t" << *(net_ptr->blk_pin_list[d_index].BlockNamePtr())
//          << "  " << *(net_ptr->blk_pin_list[d_index].PinNamePtr())
//          << ", " << *(net_ptr->blk_pin_list[l_index].BlockNamePtr())
//          << "  " << *(net_ptr->blk_pin_list[l_index].PinNamePtr()) << "\n";
//    }
//    if (i>10) break;
//  }
}

void Circuit::NetSortBlkPin() {
    for (auto &net: design_.nets_) {
        net.SortBlkPinList();
    }
}

// returns HPWL in the x direction, considering cell pin offsets, unit in micron.
double Circuit::WeightedHPWLX() {
    double hpwlx = 0;
    for (auto &net: design_.nets_) {
        hpwlx += net.WeightedHPWLX();
    }
    return hpwlx * GridValueX();
}

// returns HPWL in the y direction, considering cell pin offsets, unit in micron.
double Circuit::WeightedHPWLY() {
    double hpwly = 0;
    for (auto &net: design_.nets_) {
        hpwly += net.WeightedHPWLY();
    }
    return hpwly * GridValueY();
}

// returns total HPWL, considering cell pin offsets, unit in micron.
double Circuit::WeightedHPWL() {
    return WeightedHPWLX() + WeightedHPWLY();
}

// simple function to report HPWL.
void Circuit::ReportHPWL() {
    BOOST_LOG_TRIVIAL(info)
        << "  Current weighted HPWL: " << WeightedHPWL() << " um\n";
}

// report the histogram of HPWL using linear bins.
void Circuit::ReportHPWLHistogramLinear(int bin_num) {
    std::vector<double> hpwl_list;
    double min_hpwl = DBL_MAX;
    double max_hpwl = DBL_MIN;
    hpwl_list.reserve(design_.nets_.size());
    double factor = GridValueY() / GridValueX();
    for (auto &net: design_.nets_) {
        double tmp_hpwl = net.WeightedHPWLX() + net.WeightedHPWLY() * factor;
        if (net.PinCnt() >= 1) {
            hpwl_list.push_back(tmp_hpwl);
            min_hpwl = std::min(min_hpwl, tmp_hpwl);
            max_hpwl = std::max(max_hpwl, tmp_hpwl);
        }
    }

    double step = (max_hpwl - min_hpwl) / bin_num;
    std::vector<int> count(bin_num, 0);
    for (auto &hpwl: hpwl_list) {
        int tmp_num = (int) std::floor((hpwl - min_hpwl) / step);
        if (tmp_num == bin_num) {
            tmp_num = bin_num - 1;
        }
        ++count[tmp_num];
    }

    int tot_count = design_.nets_.size();
    BOOST_LOG_TRIVIAL(info) << "\n";
    BOOST_LOG_TRIVIAL(info)
        << "                  HPWL histogram (linear scale bins)\n";
    BOOST_LOG_TRIVIAL(info)
        << "===================================================================\n";
    BOOST_LOG_TRIVIAL(info) << "   HPWL interval         Count\n";
    for (int i = 0; i < bin_num; ++i) {
        double lo = min_hpwl + step * i;
        double hi = lo + step;

        std::string buffer(1024, '\0');
        int written_length =
            sprintf(&buffer[0], "  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
        buffer.resize(written_length);

        int percent = std::ceil(50 * count[i] / (double) tot_count);
        for (int j = 0; j < percent; ++j) {
            buffer.push_back('*');
        }
        buffer.push_back('\n');
        BOOST_LOG_TRIVIAL(info) << buffer;
    }
    BOOST_LOG_TRIVIAL(info)
        << "===================================================================\n";
    BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: "
                            << GridValueX() << " um\n";
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// report the histogram of HPWL using logarithmic bins.
void Circuit::ReportHPWLHistogramLogarithm(int bin_num) {
    std::vector<double> hpwl_list;
    double min_hpwl = DBL_MAX;
    double max_hpwl = DBL_MIN;
    hpwl_list.reserve(design_.nets_.size());
    double factor = GridValueY() / GridValueX();
    for (auto &net: design_.nets_) {
        double tmp_hpwl = net.WeightedHPWLX() + net.WeightedHPWLY() * factor;
        if (tmp_hpwl > 0) {
            double log_hpwl = std::log10(tmp_hpwl);
            hpwl_list.push_back(log_hpwl);
            min_hpwl = std::min(min_hpwl, log_hpwl);
            max_hpwl = std::max(max_hpwl, log_hpwl);
        }
    }

    double step = (max_hpwl - min_hpwl) / bin_num;
    std::vector<int> count(bin_num, 0);
    for (auto &hpwl: hpwl_list) {
        int tmp_num = (int) std::floor((hpwl - min_hpwl) / step);
        if (tmp_num == bin_num) {
            tmp_num = bin_num - 1;
        }
        ++count[tmp_num];
    }

    int tot_count = design_.nets_.size();
    BOOST_LOG_TRIVIAL(info) << "\n";
    BOOST_LOG_TRIVIAL(info)
        << "                  HPWL histogram (log scale bins)\n";
    BOOST_LOG_TRIVIAL(info)
        << "===================================================================\n";
    BOOST_LOG_TRIVIAL(info) << "   HPWL interval         Count\n";
    for (int i = 0; i < bin_num; ++i) {
        double lo = std::pow(10, min_hpwl + step * i);
        double hi = std::pow(10, min_hpwl + step * (i + 1));

        std::string buffer(1024, '\0');
        int written_length =
            sprintf(&buffer[0], "  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
        buffer.resize(written_length);

        int percent = std::ceil(50 * count[i] / (double) tot_count);
        for (int j = 0; j < percent; ++j) {
            buffer.push_back('*');
        }
        buffer.push_back('\n');
        BOOST_LOG_TRIVIAL(info) << buffer;
    }
    BOOST_LOG_TRIVIAL(info)
        << "===================================================================\n";
    BOOST_LOG_TRIVIAL(info) << " * HPWL unit, grid value in X: "
                            << GridValueX() << " um\n";
    BOOST_LOG_TRIVIAL(info) << "\n";
}

// create a file to save distance to optimal region for each cell
void Circuit::SaveOptimalRegionDistance(std::string file_name) {
    OptRegDist distance_calculator;
    distance_calculator.circuit_ = this;
    distance_calculator.SaveFile(file_name);
}

// returns HPWL in the x direction, assuming cell pins are in the cell, unit in micron.
double Circuit::HPWLCtoCX() {
    double hpwl_c2c_x = 0;
    for (auto &net: design_.nets_) {
        hpwl_c2c_x += net.HPWLCtoCX();
    }
    return hpwl_c2c_x * GridValueX();
}

// returns HPWL in the y direction, assuming cell pins are in the cell, unit in micron.
double Circuit::HPWLCtoCY() {
    double hpwl_c2c_y = 0;
    for (auto &net: design_.nets_) {
        hpwl_c2c_y += net.HPWLCtoCY();
    }
    return hpwl_c2c_y * GridValueY();
}

// returns total HPWL, assuming cell pins are in the cell, unit in micron.
double Circuit::HPWLCtoC() {
    return HPWLCtoCX() + HPWLCtoCY();
}

// simple function to report HPWL.
void Circuit::ReportHPWLCtoC() {
    BOOST_LOG_TRIVIAL(info) << "  Current HPWL: " << HPWLCtoC() << " um\n";
}

void Circuit::GenMATLABTable(std::string const &name_of_file,
                             bool only_well_tap) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
    ost << RegionLLX() << "\t"
        << RegionURX() << "\t"
        << RegionURX() << "\t"
        << RegionLLX() << "\t"
        << RegionLLY() << "\t"
        << RegionLLY() << "\t"
        << RegionURY() << "\t"
        << RegionURY() << "\t"
        << 1 << "\t"
        << 1 << "\t"
        << 1 << "\n";
    if (!only_well_tap) {
        for (auto &block: design_.blocks_) {
            ost << block.LLX() << "\t"
                << block.URX() << "\t"
                << block.URX() << "\t"
                << block.LLX() << "\t"
                << block.LLY() << "\t"
                << block.LLY() << "\t"
                << block.URY() << "\t"
                << block.URY() << "\t"
                << 0 << "\t"
                << 1 << "\t"
                << 1 << "\n";
        }
    }
    for (auto &block: design_.welltaps_) {
        ost << block.LLX() << "\t"
            << block.URX() << "\t"
            << block.URX() << "\t"
            << block.LLX() << "\t"
            << block.LLY() << "\t"
            << block.LLY() << "\t"
            << block.URY() << "\t"
            << block.URY() << "\t"
            << 0 << "\t"
            << 1 << "\t"
            << 1 << "\n";
    }
    ost.close();
}

void Circuit::GenMATLABWellTable(std::string const &name_of_file,
                                 bool only_well_tap) {
    std::string frame_file = name_of_file + "_outline.txt";
    std::string unplug_file = name_of_file + "_unplug.txt";
    GenMATLABTable(frame_file, only_well_tap);

    std::ofstream ost(unplug_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + unplug_file);

    BlockTypeWell *well;
    if (!only_well_tap) {
        for (auto &block: design_.blocks_) {
            well = block.TypePtr()->WellPtr();
            if (well != nullptr) {
                auto &n_well_shape = well->NwellRect();
                auto &p_well_shape = well->PwellRect();
                if (block.Orient() == N) {
                    ost << block.LLX() + n_well_shape.LLX() << "\t"
                        << block.LLX() + n_well_shape.URX() << "\t"
                        << block.LLX() + n_well_shape.URX() << "\t"
                        << block.LLX() + n_well_shape.LLX() << "\t"
                        << block.LLY() + n_well_shape.LLY() << "\t"
                        << block.LLY() + n_well_shape.LLY() << "\t"
                        << block.LLY() + n_well_shape.URY() << "\t"
                        << block.LLY() + n_well_shape.URY() << "\t"

                        << block.LLX() + p_well_shape.LLX() << "\t"
                        << block.LLX() + p_well_shape.URX() << "\t"
                        << block.LLX() + p_well_shape.URX() << "\t"
                        << block.LLX() + p_well_shape.LLX() << "\t"
                        << block.LLY() + p_well_shape.LLY() << "\t"
                        << block.LLY() + p_well_shape.LLY() << "\t"
                        << block.LLY() + p_well_shape.URY() << "\t"
                        << block.LLY() + p_well_shape.URY() << "\n";
                } else if (block.Orient() == FS) {
                    ost << block.LLX() + n_well_shape.LLX() << "\t"
                        << block.LLX() + n_well_shape.URX() << "\t"
                        << block.LLX() + n_well_shape.URX() << "\t"
                        << block.LLX() + n_well_shape.LLX() << "\t"
                        << block.URY() - n_well_shape.LLY() << "\t"
                        << block.URY() - n_well_shape.LLY() << "\t"
                        << block.URY() - n_well_shape.URY() << "\t"
                        << block.URY() - n_well_shape.URY() << "\t"

                        << block.LLX() + p_well_shape.LLX() << "\t"
                        << block.LLX() + p_well_shape.URX() << "\t"
                        << block.LLX() + p_well_shape.URX() << "\t"
                        << block.LLX() + p_well_shape.LLX() << "\t"
                        << block.URY() - p_well_shape.LLY() << "\t"
                        << block.URY() - p_well_shape.LLY() << "\t"
                        << block.URY() - p_well_shape.URY() << "\t"
                        << block.URY() - p_well_shape.URY() << "\n";
                }
            }
        }
    }

    for (auto &block: design_.welltaps_) {
        well = block.TypePtr()->WellPtr();
        if (well != nullptr) {
            auto &n_well_shape = well->NwellRect();
            auto &p_well_shape = well->PwellRect();
            if (block.Orient() == N) {
                ost << block.LLX() + n_well_shape.LLX() << "\t"
                    << block.LLX() + n_well_shape.URX() << "\t"
                    << block.LLX() + n_well_shape.URX() << "\t"
                    << block.LLX() + n_well_shape.LLX() << "\t"
                    << block.LLY() + n_well_shape.LLY() << "\t"
                    << block.LLY() + n_well_shape.LLY() << "\t"
                    << block.LLY() + n_well_shape.URY() << "\t"
                    << block.LLY() + n_well_shape.URY() << "\t"

                    << block.LLX() + p_well_shape.LLX() << "\t"
                    << block.LLX() + p_well_shape.URX() << "\t"
                    << block.LLX() + p_well_shape.URX() << "\t"
                    << block.LLX() + p_well_shape.LLX() << "\t"
                    << block.LLY() + p_well_shape.LLY() << "\t"
                    << block.LLY() + p_well_shape.LLY() << "\t"
                    << block.LLY() + p_well_shape.URY() << "\t"
                    << block.LLY() + p_well_shape.URY() << "\n";
            } else if (block.Orient() == FS) {
                ost << block.LLX() + n_well_shape.LLX() << "\t"
                    << block.LLX() + n_well_shape.URX() << "\t"
                    << block.LLX() + n_well_shape.URX() << "\t"
                    << block.LLX() + n_well_shape.LLX() << "\t"
                    << block.URY() - n_well_shape.LLY() << "\t"
                    << block.URY() - n_well_shape.LLY() << "\t"
                    << block.URY() - n_well_shape.URY() << "\t"
                    << block.URY() - n_well_shape.URY() << "\t"

                    << block.LLX() + p_well_shape.LLX() << "\t"
                    << block.LLX() + p_well_shape.URX() << "\t"
                    << block.LLX() + p_well_shape.URX() << "\t"
                    << block.LLX() + p_well_shape.LLX() << "\t"
                    << block.URY() - p_well_shape.LLY() << "\t"
                    << block.URY() - p_well_shape.LLY() << "\t"
                    << block.URY() - p_well_shape.URY() << "\t"
                    << block.URY() - p_well_shape.URY() << "\n";
            }
        }
    }

    ost.close();
}

void Circuit::GenLongNetTable(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

    int multi_factor = 5;
    double threshold = multi_factor * AveBlkHeight();
    int count = 0;
    double ave_hpwl = 0;
    for (auto &net: design_.nets_) {
        double hpwl = net.WeightedHPWL();
        if (hpwl > threshold) {
            ave_hpwl += hpwl;
            ++count;
            for (auto &blk_pin: net.BlockPins()) {
                ost << blk_pin.AbsX() << "\t"
                    << blk_pin.AbsY() << "\t";
            }
            ost << "\n";
        }
    }
    ave_hpwl /= count;
    BOOST_LOG_TRIVIAL(info)
        << "Long net report: \n"
        << "  threshold: " << multi_factor << " "
        << threshold << "\n"
        << "  count:     " << count << "\n"
        << "  ave_hpwl:  " << ave_hpwl << "\n";

    ost.close();
}

void Circuit::SaveDefFile(
    std::string const &name_of_file,
    std::string const &def_file_name,
    bool is_complete_version
) {
    std::string file_name;
    if (is_complete_version) {
        file_name = name_of_file + ".def";
    } else {
        file_name = name_of_file + "_trim.def";
    }
    if (is_complete_version) {
        BOOST_LOG_TRIVIAL(info) << "Writing DEF file: " << file_name << "\n";
    } else {
        BOOST_LOG_TRIVIAL(info)
            << "Writing trimmed DEF file (for debugging): "
            << file_name << "\n";
    }

    std::ofstream ost(file_name.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + file_name);

    std::ifstream ist(def_file_name.c_str());
    DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

    std::string line;
    // 1. print file header, copy from def file
    while (true) {
        getline(ist, line);
        if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
            break;
        }
        ost << line << "\n";
    }

    // 2. print component
    double factor_x = design_.distance_microns_ * GridValueX();
    double factor_y = design_.distance_microns_ * GridValueY();
    if (is_complete_version) {
        ost << "COMPONENTS "
            << design_.blocks_.size() - design_.pre_placed_io_count_
                + design_.welltaps_.size()
            << " ;\n";
    } else {
        ost << "COMPONENTS "
            << design_.blocks_.size() - design_.pre_placed_io_count_
                + design_.welltaps_.size() + 1
            << " ;\n";
        ost << "- "
            << "npwells "
            << name_of_file + "well + "
            << PlaceStatusStr(COVER) << " "
            << "( "
            << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_
            << " "
            << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << "N"
            << " ;\n";
    }
    for (auto &block: design_.blocks_) {
        if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
        ost << "- "
            << block.Name() << " "
            << block.TypeName() << " + "
            << block.StatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_
            << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
    }
    for (auto &block: design_.welltaps_) {
        ost << "- "
            << block.Name() << " "
            << block.TypeName() << " + "
            << "PLACED" << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_
            << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
    }
    ost << "END COMPONENTS\n";
    // jump to the end of components
    while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
        getline(ist, line);
    }

    // 3. print PIN
    ost << "\n";
    ost << "PINS " << design_.iopins_.size() << " ;\n";
    DaliExpects(!tech_.metal_list_.empty(),
                "Need metal layer info to generate PIN location\n");
    std::string metal_name = tech_.metal_list_[0].Name();
    int half_width = std::ceil(
        tech_.metal_list_[0].MinHeight() / 2.0 * design_.distance_microns_);
    int height =
        std::ceil(tech_.metal_list_[0].Width() * design_.distance_microns_);
    for (auto &iopin: design_.iopins_) {
        ost << "- "
            << iopin.Name()
            << " + NET "
            << iopin.NetName()
            << " + DIRECTION INPUT + USE SIGNAL";
        if (iopin.IsPrePlaced()) {
            ost << "\n  + LAYER "
                << metal_name
                << " ( "
                << -half_width << " "
                << 0 << " ) "
                << " ( "
                << half_width << " "
                << height << " ) ";
            ost << "\n  + PLACED ( "
                << iopin.X() * factor_x + design_.die_area_offset_x_ << " "
                << iopin.Y() * factor_y + design_.die_area_offset_y_
                << " ) ";
            if (iopin.X() == design_.region_left_) {
                ost << "E";
            } else if (iopin.X() == design_.region_right_) {
                ost << "W";
            } else if (iopin.Y() == design_.region_bottom_) {
                ost << "N";
            } else {
                ost << "S";
            }
        }
        ost << " ;\n";
    }
    ost << "END PINS\n\n";

    // 4. print net, copy from def file
    while (true) {
        getline(ist, line);
        if (line.find("NETS") != std::string::npos || ist.eof()) {
            break;
        }
    }
    if (is_complete_version) {
        ost << line << "\n";
        while (!ist.eof()) {
            getline(ist, line);
            ost << line << "\n";
        }
    } else {
        ost << "END DESIGN\n";
    }

    ost.close();
    ist.close();

    BOOST_LOG_TRIVIAL(info) << "    DEF writing done\n";
}

// save def file for users.
/****
 * Universal function for saving DEF file
 * @param, output def file name, ".def" extension is added automatically
 * @param, input def file
 * @param, save_floorplan,
 *    case 0, floorplan not saved
 *    case 1, floorplan saved
 *    otherwise, report an error message
 * @param, save_cell,
 *    case 0, no cells are saved
 *    case 1, save all normal cells, regardless of the placement status
 *    case 2, save only well tap cells
 *    case 3, save all normal cells + dummy cell for well filling
 *    case 4, save all normal cells + dummy cell for well filling + dummy cell for n/p-plus filling
 *    case 5, save all placed cells
 *    otherwise, report an error message
 * @param, save_iopin
 *    case 0, no IOPINs are saved
 *    case 1, save all IOPINs
 *    case 2, save all IOPINs with status before IO placement
 *    otherwise, report an error message
 * @param, save_net
 *    case 0, no nets are saved
 *    case 1, save all nets
 *    case 2, save nets containing saved cells and IOPINs
 *    case 3, save power nets for well tap cell
 *    otherwise, report an error message
 * ****/
void Circuit::SaveDefFile(
    std::string const &base_name,
    std::string const &name_padding,
    std::string const &def_file_name,
    int save_floorplan,
    int save_cell,
    int save_iopin,
    int save_net
) {
    std::string file_name = base_name + name_padding + ".def";
    BOOST_LOG_TRIVIAL(info) << "Writing DEF file: " << file_name;
    std::ofstream ost(file_name.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + file_name);
    std::ifstream ist(def_file_name.c_str());
    DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

    using std::chrono::system_clock;
    system_clock::time_point today = system_clock::now();
    std::time_t tt = system_clock::to_time_t(today);
    ost << "##################################################\n";
    ost << "#  created by: Dali, build time: " << __DATE__ << " " << __TIME__
        << "\n";
    ost << "#  time: " << ctime(&tt);
    ost << "##################################################\n";

    std::string line;
    // 1. floorplanning
    while (true) {
        getline(ist, line);
        if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
            break;
        }
        ost << line << "\n";
    }

    // 2. COMPONENT
    double factor_x = design_.distance_microns_ * GridValueX();
    double factor_y = design_.distance_microns_ * GridValueY();
    switch (save_cell) {
        case 0: { // no cells are saved
            ost << "COMPONENTS 0 ;\n";
            break;
        }
        case 1: { // save all normal cells, regardless of the placement status
            int cell_count = 0;
            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                ++cell_count;
            }
            cell_count += design_.welltaps_.size();
            ost << "COMPONENTS " << cell_count << " ;\n";
            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            for (auto &block: design_.welltaps_) {
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            break;
        }
        case 2: { // save only well tap cells
            int cell_count = design_.welltaps_.size();
            ost << "COMPONENTS " << cell_count << " ;\n";
            for (auto &block: design_.welltaps_) {
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            break;
        }
        case 3: { // save all normal cells + dummy cell for well filling
            int cell_count = 0;
            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                ++cell_count;
            }
            cell_count += design_.welltaps_.size();
            cell_count += 1;
            ost << "COMPONENTS " << cell_count << " ;\n";
            ost << "- "
                << "npwells "
                << base_name + "well + "
                << PlaceStatusStr(COVER) << " "
                << "( "
                << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_
                << " "
                << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
                << " ) "
                << "N"
                << " ;\n";

            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            for (auto &block: design_.welltaps_) {
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            break;
        }
        case 4: { // save all normal cells + dummy cell for well filling + dummy cell for n/p-plus filling
            int cell_count = 0;
            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                ++cell_count;
            }
            cell_count += design_.welltaps_.size();
            cell_count += 2;
            ost << "COMPONENTS " << cell_count << " ;\n";
            ost << "- "
                << "npwells "
                << base_name + "well + "
                << PlaceStatusStr(COVER) << " "
                << "( "
                << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_
                << " "
                << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
                << " ) "
                << "N"
                << " ;\n";
            ost << "- "
                << "ppnps "
                << base_name + "ppnp + "
                << PlaceStatusStr(COVER) << " "
                << "( "
                << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_
                << " "
                << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
                << " ) "
                << "N"
                << " ;\n";

            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            for (auto &block: design_.welltaps_) {
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            break;
        }
        case 5: { // save all placed cells
            int cell_count = 0;
            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                if (block.Status() == UNPLACED) continue;
                ++cell_count;
            }
            cell_count += design_.welltaps_.size();
            ost << "COMPONENTS " << cell_count << " ;\n";
            for (auto &block: design_.blocks_) {
                if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
                if (block.Status() == UNPLACED) continue;
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            for (auto &block: design_.welltaps_) {
                ost << "- "
                    << block.Name() << " "
                    << block.TypeName() << " + "
                    << block.StatusStr() << " "
                    << "( "
                    << (int) (block.LLX() * factor_x)
                        + design_.die_area_offset_x_ << " "
                    << (int) (block.LLY() * factor_y)
                        + design_.die_area_offset_y_
                    << " ) "
                    << OrientStr(block.Orient())
                    << " ;\n";
            }
            break;
        }
        default: {
            DaliExpects(false,
                        "Invalid value setting for @param save_cell");
        }

    }
    ost << "END COMPONENTS\n\n";

    // 3. PIN
    switch (save_iopin) {
        case 0: { // no IOPINs are saved
            ost << "PINS 0 ;\n";
            break;
        }
        case 1: { // save all IOPINs
            ost << "PINS " << design_.iopins_.size() << " ;\n";
            DaliExpects(!tech_.metal_list_.empty(),
                        "Need metal layer info to generate PIN location\n");
            for (auto &iopin: design_.iopins_) {
                ost << "- "
                    << iopin.Name()
                    << " + NET "
                    << iopin.NetName()
                    << " + DIRECTION "
                    << iopin.SigDirectStr()
                    << " + USE " << iopin.SigUseStr();
                if (iopin.IsPlaced()) {
                    std::string metal_name = iopin.LayerName();
                    int half_width = std::ceil(
                        iopin.LayerPtr()->MinHeight() / 2.0
                            * design_.distance_microns_);
                    int height = std::ceil(
                        iopin.LayerPtr()->Width() * design_.distance_microns_);
                    ost << "\n  + LAYER "
                        << metal_name
                        << " ( "
                        << -half_width << " "
                        << 0 << " ) "
                        << " ( "
                        << half_width << " "
                        << height << " ) ";
                    ost << "\n  + PLACED ( "
                        << iopin.X() * factor_x + design_.die_area_offset_x_
                        << " "
                        << iopin.Y() * factor_y + design_.die_area_offset_y_
                        << " ) ";
                    if (iopin.X() == design_.region_left_) {
                        ost << "E";
                    } else if (iopin.X() == design_.region_right_) {
                        ost << "W";
                    } else if (iopin.Y() == design_.region_bottom_) {
                        ost << "N";
                    } else {
                        ost << "S";
                    }
                }
                ost << " ;\n";
            }
            break;
        }
        case 2: { // save all IOPINs with status before IO placement
            ost << "PINS " << design_.iopins_.size() << " ;\n";
            DaliExpects(!tech_.metal_list_.empty(),
                        "Need metal layer info to generate PIN location\n");
            std::string metal_name = tech_.metal_list_[0].Name();
            int half_width = std::ceil(tech_.metal_list_[0].MinHeight() / 2.0
                                           * design_.distance_microns_);
            int height = std::ceil(
                tech_.metal_list_[0].Width() * design_.distance_microns_);
            for (auto &iopin: design_.iopins_) {
                ost << "- "
                    << iopin.Name()
                    << " + NET "
                    << iopin.NetName()
                    << " + DIRECTION "
                    << iopin.SigDirectStr()
                    << " + USE " << iopin.SigUseStr();
                if (iopin.IsPrePlaced()) {
                    ost << "\n  + LAYER "
                        << metal_name
                        << " ( "
                        << -half_width << " "
                        << 0 << " ) "
                        << " ( "
                        << half_width << " "
                        << height << " ) ";
                    ost << "\n  + PLACED ( "
                        << iopin.X() * factor_x + design_.die_area_offset_x_
                        << " "
                        << iopin.Y() * factor_y + design_.die_area_offset_y_
                        << " ) ";
                    if (iopin.X() == design_.region_left_) {
                        ost << "E";
                    } else if (iopin.X() == design_.region_right_) {
                        ost << "W";
                    } else if (iopin.Y() == design_.region_bottom_) {
                        ost << "N";
                    } else {
                        ost << "S";
                    }
                }
                ost << " ;\n";
            }
            break;
        }
        default: {
            DaliExpects(false,
                        "Invalid value setting for @param save_iopin\n");
        }
    }
    ost << "END PINS\n\n";

    switch (save_net) {
        case 0: { // no nets are saved
            ost << "NETS 0 ;\n";
            break;
        }
        case 1: { // save all nets
            ost << "NETS " << design_.nets_.size() << " ;\n";
            for (auto &net: design_.nets_) {
                ost << "- " << net.Name() << "\n";
                ost << " ";
                for (auto &iopin: net.IoPinPtrs()) {
                    ost << " ( PIN " << iopin->Name() << " ) ";
                }
                for (auto &pin_pair: net.BlockPins()) {
                    if (pin_pair.BlkPtr()->TypePtr()
                        == tech_.io_dummy_blk_type_ptr_)
                        continue;
                    ost << " ( " << pin_pair.BlockName() << " "
                        << pin_pair.PinName() << " ) ";
                }
                ost << "\n" << " ;\n";
            }
            break;
        }
        case 2: { // save nets containing saved cells and IOPINs
            DaliExpects(false, "This part has not been implemented\n");
            break;
        }
        case 3: {// save power nets for well tap cell
            ost << "\nNETS 2 ;\n";
            // GND
            ost << "- ggnndd\n";
            ost << " ";
            for (auto &block: design_.welltaps_) {
                ost << " ( " << block.Name() << " g0 )";
            }
            ost << "\n" << " ;\n";
            //Vdd
            ost << "- vvdddd\n";
            ost << " ";
            for (auto &block: design_.welltaps_) {
                ost << " ( " << block.Name() << " v0 )";
            }
            ost << "\n" << " ;\n";
            break;
        }
        default: {
            DaliExpects(false,
                        "Invalid value setting for @param save_net\n");
        }
    }
    ost << "END NETS\n\n";

    ost << "END DESIGN\n";

    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void Circuit::SaveIODefFile(
    std::string const &name_of_file,
    std::string const &def_file_name
) {
    std::string file_name;
    file_name = name_of_file + "_io.def";
    BOOST_LOG_TRIVIAL(info) << "Writing IO DEF file: " << file_name;

    std::ofstream ost(file_name.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + file_name);

    std::ifstream ist(def_file_name.c_str());
    DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

    std::string line;
    // 1. print file header, copy from def file
    while (true) {
        getline(ist, line);
        if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
            break;
        }
        ost << line << "\n";
    }

    // 2. print component
    double factor_x = design_.distance_microns_ * GridValueX();
    double factor_y = design_.distance_microns_ * GridValueY();
    ost << "COMPONENTS "
        << design_.blocks_.size() - design_.pre_placed_io_count_
            + design_.welltaps_.size()
        << " ;\n";
    for (auto &block: design_.blocks_) {
        if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
        ost << "- "
            << block.Name() << " "
            << block.TypeName() << " + "
            << block.StatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_
            << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
    }
    for (auto &block: design_.welltaps_) {
        ost << "- "
            << block.Name() << " "
            << block.TypeName() << " + "
            << "PLACED" << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_
            << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
    }
    ost << "END COMPONENTS\n";
    // jump to the end of components
    while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
        getline(ist, line);
    }

    // 3. print PIN
    ost << "\n";
    ost << "PINS " << design_.iopins_.size() << " ;\n";
    DaliExpects(!tech_.metal_list_.empty(),
                "Need metal layer info to generate PIN location\n");
    std::string metal_name = tech_.metal_list_[0].Name();
    int half_width = std::ceil(
        tech_.metal_list_[0].MinHeight() / 2.0 * design_.distance_microns_);
    int height =
        std::ceil(tech_.metal_list_[0].Width() * design_.distance_microns_);
    for (auto &iopin: design_.iopins_) {
        ost << "- "
            << iopin.Name()
            << " + NET "
            << iopin.NetName()
            << " + DIRECTION INPUT + USE SIGNAL";
        if (iopin.IsPlaced()) {
            ost << "\n  + LAYER "
                << metal_name
                << " ( "
                << -half_width << " "
                << 0 << " ) "
                << " ( "
                << half_width << " "
                << height << " ) ";
            ost << "\n  + PLACED ( "
                << iopin.X() * factor_x + design_.die_area_offset_x_ << " "
                << iopin.Y() * factor_y + design_.die_area_offset_y_
                << " ) ";
            if (iopin.X() == design_.region_left_) {
                ost << "E";
            } else if (iopin.X() == design_.region_right_) {
                ost << "W";
            } else if (iopin.Y() == design_.region_bottom_) {
                ost << "N";
            } else {
                ost << "S";
            }
        }
        ost << " ;\n";
    }
    ost << "END PINS\n\n";

    // 4. print net, copy from def file
    while (true) {
        getline(ist, line);
        if (line.find("NETS") != std::string::npos || ist.eof()) {
            break;
        }
    }
    ost << line << "\n";
    while (!ist.eof()) {
        getline(ist, line);
        ost << line << "\n";
    }

    ost.close();
    ist.close();

    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void Circuit::SaveDefWell(
    std::string const &name_of_file,
    std::string const &def_file_name,
    bool is_no_normal_cell
) {
    BOOST_LOG_TRIVIAL(info)
        << "Writing WellTap Network DEF file (for debugging): " << name_of_file;
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);

    std::ifstream ist(def_file_name.c_str());
    DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

    std::string line;
    // 1. print file header, copy from def file
    while (true) {
        getline(ist, line);
        if (line.find("DESIGN") != std::string::npos || ist.eof()) {
            ost << "DESIGN WellTapNetwork ;\n";
            continue;
        }
        if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
            break;
        }
        ost << line << "\n";
    }

    // 2. print well tap cells
    double factor_x = design_.distance_microns_ * GridValueX();
    double factor_y = design_.distance_microns_ * GridValueY();
    if (is_no_normal_cell) {
        ost << "COMPONENTS " << design_.welltaps_.size() << " ;\n";
    } else {
        ost << "COMPONENTS "
            << design_.blocks_.size() + design_.welltaps_.size()
            << " ;\n";
    }
    for (auto &block: design_.welltaps_) {
        ost << "- "
            << block.Name() << " "
            << block.TypeName() << " + "
            << "PLACED" << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_
            << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
    }
    if (!is_no_normal_cell) {
        for (auto &block: design_.blocks_) {
            if (block.TypePtr() == tech_.io_dummy_blk_type_ptr_) continue;
            ost << "- "
                << block.Name() << " "
                << block.TypeName() << " + "
                << block.StatusStr() << " "
                << "( "
                << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_
                << " "
                << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
                << " ) "
                << OrientStr(block.Orient())
                << " ;\n";
        }
    }
    ost << "END COMPONENTS\n";
    // jump to the end of components
    while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
        getline(ist, line);
    }

    // 3. print net, copy from def file

    ost << "\nNETS 2 ;\n";
    // GND
    ost << "- ggnndd\n";
    ost << " ";
    for (auto &block: design_.welltaps_) {
        ost << " ( " << block.Name() << " g0 )";
    }
    ost << "\n" << " ;\n";
    //Vdd
    ost << "- vvdddd\n";
    ost << " ";
    for (auto &block: design_.welltaps_) {
        ost << " ( " << block.Name() << " v0 )";
    }
    ost << "\n" << " ;\n";

    ost << "END NETS\n\n";
    ost << "END DESIGN\n";

    ost.close();
    ist.close();

    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void Circuit::SaveDefPPNPWell(
    std::string const &name_of_file,
    std::string const &def_file_name
) {
    std::string file_name = name_of_file + "_ppnpwell.def";
    BOOST_LOG_TRIVIAL(info) << "Writing PPNPWell DEF file (for debugging): "
                            << file_name;
    std::ofstream ost(file_name.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + file_name);

    std::ifstream ist(def_file_name.c_str());
    DaliExpects(ist.is_open(), "Cannot open file " + def_file_name);

    std::string line;
    // 1. print file header, copy from def file
    while (true) {
        getline(ist, line);
        if (line.find("DESIGN") != std::string::npos || ist.eof()) {
            ost << "DESIGN PPNPWell ;\n";
            continue;
        }
        if (line.find("COMPONENTS") != std::string::npos || ist.eof()) {
            break;
        }
        ost << line << "\n";
    }

    // 2. print well tap cells
    double factor_x = design_.distance_microns_ * GridValueX();
    double factor_y = design_.distance_microns_ * GridValueY();
    ost << "COMPONENTS " << 2 << " ;\n";
    ost << "- "
        << "npwells "
        << name_of_file + "well + "
        << PlaceStatusStr(COVER) << " "
        << "( "
        << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
        << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
        << " ) "
        << "N"
        << " ;\n";
    ost << "- "
        << "ppnps "
        << name_of_file + "ppnp + "
        << PlaceStatusStr(COVER) << " "
        << "( "
        << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
        << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
        << " ) "
        << "N"
        << " ;\n";
    ost << "END COMPONENTS\n";
    // jump to the end of components
    while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
        getline(ist, line);
    }

    // 3. print net, copy from def file

    ost << "\nNETS 0 ;\n";
    ost << "END NETS\n\n";
    ost << "END DESIGN\n";

    ost.close();
    ist.close();

    BOOST_LOG_TRIVIAL(info) << ", done\n";
}

void Circuit::SaveInstanceDefFile(
    std::string const &name_of_file,
    std::string const &def_file_name
) {
    SaveDefFile(name_of_file, def_file_name, false);
}

void Circuit::SaveBookshelfNode(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
    ost << "# this line is here just for ntuplace to recognize this file \n\n";
    ost << "NumNodes : \t\t" << design_.tot_mov_blk_num_ << "\n"
        << "NumTerminals : \t\t"
        << design_.blocks_.size() - design_.tot_mov_blk_num_ << "\n";
    for (auto &block: design_.blocks_) {
        ost << "\t" << block.Name()
            << "\t" << block.Width() * design_.distance_microns_
                * GridValueX()
            << "\t" << block.Height() * design_.distance_microns_
                * GridValueY()
            << "\n";
    }
}

void Circuit::SaveBookshelfNet(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
    int num_pins = 0;
    for (auto &net: design_.nets_) {
        num_pins += net.BlockPins().size();
    }
    ost << "# this line is here just for ntuplace to recognize this file \n\n";
    ost << "NumNets : " << design_.nets_.size() << "\n"
        << "NumPins : " << num_pins << "\n\n";
    for (auto &net: design_.nets_) {
        ost << "NetDegree : " << net.BlockPins().size() << "   " << net.Name()
            << "\n";
        for (auto &pair: net.BlockPins()) {
            ost << "\t" << pair.BlockName() << "\t";
            if (pair.PinPtr()->IsInput()) {
                ost << "I : ";
            } else {
                ost << "O : ";
            }
            ost << (pair.PinPtr()->OffsetX()
                - pair.BlkPtr()->TypePtr()->Width() / 2.0)
                * design_.distance_microns_
                * GridValueX()
                << "\t"
                << (pair.PinPtr()->OffsetY()
                    - pair.BlkPtr()->TypePtr()->Height() / 2.0)
                    * design_.distance_microns_
                    * GridValueY()
                << "\n";
        }
    }
}

void Circuit::SaveBookshelfPl(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
    ost << "# this line is here just for ntuplace to recognize this file \n\n";
    for (auto &block: design_.blocks_) {
        ost << block.Name()
            << "\t"
            << int(
                block.LLX() * design_.distance_microns_
                    * GridValueX())
            << "\t"
            << int(block.LLY() * design_.distance_microns_
                       * GridValueY());
        if (block.IsMovable()) {
            ost << "\t:\tN\n";
        } else {
            ost << "\t:\tN\t/FIXED\n";
        }
    }
    ost.close();
}

void Circuit::SaveBookshelfScl(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
#ifdef USE_OPENDB
    if (db_ptr_ == nullptr) {
      BOOST_LOG_TRIVIAL(info)   << "During saving bookshelf .scl file. No ROW info has been found!";
      return;
    }
    auto rows = db_ptr_->getChip()->getBlock()->getRows();
    ost << "NumRows : " << rows.size() << "\n\n";
    for (auto &&row: rows) {
      int origin_x = 0, origin_y = 0;
      row->getOrigin(origin_x, origin_y);
      auto site = row->getSite();
      int height = site->getHeight();
      int width = site->getWidth();
      int num_sites = row->getSiteCount();
      int spacing = row->getSpacing();
      ost << "CoreRow Horizontal\n"
          << "  Coordinate    :   " << origin_y << "\n"
          << "  Height        :   " << height << "\n"
          << "  Sitewidth     :   " << width << "\n"
          << "  Sitespacing   :   " << spacing << "\n"
          << "  Siteorient    :    1\n"
          << "  Sitesymmetry  :    1\n"
          << "  SubrowOrigin  :   " << origin_x << "\tNumSites  :  " << num_sites << "\n"
          << "End\n";
    }
#endif
}

void Circuit::SaveBookshelfWts(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + name_of_file);
}

void Circuit::SaveBookshelfAux(std::string const &name_of_file) {
    std::string aux_name = name_of_file + ".aux";
    std::ofstream ost(aux_name.c_str());
    DaliExpects(ost.is_open(), "Cannot open file " + aux_name);
    ost << "RowBasedPlacement :  "
        << name_of_file << ".nodes  "
        << name_of_file << ".nets  "
        << name_of_file << ".wts  "
        << name_of_file << ".pl  "
        << name_of_file << ".scl";
}

void Circuit::LoadBookshelfPl(std::string const &name_of_file) {
    std::ifstream ist(name_of_file.c_str());
    DaliExpects(ist.is_open(), "Cannot open file " + name_of_file);

    std::string line;
    std::vector<std::string> res;
    double lx = 0;
    double ly = 0;

    while (!ist.eof()) {
        getline(ist, line);
        StrTokenize(line, res);
        if (res.size() >= 4) {
            if (IsBlockExisting(res[0])) {
                try {
                    lx = std::stod(res[1]) / GridValueX()
                        / design_.distance_microns_;
                    ly = std::stod(res[2]) / GridValueY()
                        / design_.distance_microns_;
                    GetBlockPtr(res[0])->SetLoc(lx, ly);
                } catch (...) {
                    DaliExpects(false, "Invalid stod conversion:\n\t" + line);
                }
            }
        }
    }
}

}
