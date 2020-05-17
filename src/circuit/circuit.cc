//
// Created by Yihang Yang on 2019-03-26.
//

#include "circuit.h"

#include <chrono>
#include <climits>
#include <cmath>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "status.h"

Circuit::Circuit() {
  AddDummyIOPinType();
#ifdef USE_OPENDB
  db_ = nullptr;
#endif
}

#ifdef USE_OPENDB
Circuit::Circuit(odb::dbDatabase *db) {
  AddDummyIOPinType();
  db_ = db;
  InitializeFromDB(db);
}

void Circuit::InitializeFromDB(odb::dbDatabase *db) {
  db_ = db;
  auto tech = db->getTech();
  Assert(tech != nullptr, "No tech info specified!\n");
  auto lib = db->getLibs().begin();
  Assert(lib != db->getLibs().end(), "No lib info specified!");

  // 1. lef database microns
  tech_.lef_database_microns = tech->getDbUnitsPerMicron();
  //std::cout << tech->getDbUnitsPerMicron() << "\n";
  //std::cout << tech->getLefUnits() << "\n";
  //std::cout << top_level->getDefUnits() << "\n";

  // 2. manufacturing grid and metals
  if (tech->hasManufacturingGrid()) {
    //std::cout << "Mangrid" << tech->getManufacturingGrid() << "\n";
    tech_.manufacturing_grid = tech->getManufacturingGrid() / double(tech_.lef_database_microns);
  } else {
    tech_.manufacturing_grid = 1.0 / tech_.lef_database_microns;
  }
  for (auto &&layer: tech->getLayers()) {
    if (layer->getType() == 0) {
      //std::cout << layer->getNumber() << "  " << layer->getName() << "  " << layer->getType() << " ";
      std::string metal_layer_name(layer->getName());
      double min_width = layer->getWidth() / (double) tech_.lef_database_microns;
      double min_spacing = layer->getSpacing() / (double) tech_.lef_database_microns;
      //std::cout << min_width << "  " << min_spacing << "  ";
      auto *metal_layer = AddMetalLayer(metal_layer_name, min_width, min_spacing);
      if (layer->hasArea()) {
        //std::cout << layer->getArea();
        double min_area = layer->getArea();
        metal_layer->SetArea(min_area);
      }
      //std::cout << "\n";
    }
  }

  // 3. find the first and second metal layer pitch
  if (!tech_.grid_set_) {
    double grid_value_x = -1, grid_value_y = -1;
    Assert(tech->getRoutingLayerCount() >= 2, "Needs at least one metal layer to find metal pitch");
    odb::dbTechLayer *m1_layer = nullptr;
    odb::dbTechLayer *m2_layer = nullptr;
    for (auto &&layer: tech->getLayers()) {
      //std::cout << layer->getNumber() << "  " << layer->getName() << "  " << layer->getType() << "\n";
      std::string layer_name(layer->getName());
      if (layer_name == "m1" ||
          layer_name == "metal1" ||
          layer_name == "M1" ||
          layer_name == "Metal1" ||
          layer_name == "METAL1") {
        m1_layer = layer;
        //std::cout << (layer->getWidth() + layer->getSpacing()) / double(tech_.lef_database_microns) << "\n";
      } else if (layer_name == "m2" ||
          layer_name == "metal2" ||
          layer_name == "M2" ||
          layer_name == "Metal2" ||
          layer_name == "METAL2") {
        m2_layer = layer;
        //std::cout << (layer->getWidth() + layer->getSpacing()) / double(tech_.lef_database_microns) << "\n";
      }
    }
    if (m1_layer != nullptr && m2_layer != nullptr) {
      if (m1_layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL &&
          m2_layer->getDirection() == odb::dbTechLayerDir::VERTICAL) {
        grid_value_x = (m2_layer->getWidth() + m2_layer->getSpacing()) / double(tech_.lef_database_microns);
        grid_value_y = (m1_layer->getWidth() + m1_layer->getSpacing()) / double(tech_.lef_database_microns);
      } else if (m1_layer->getDirection() == odb::dbTechLayerDir::VERTICAL &&
          m2_layer->getDirection() == odb::dbTechLayerDir::HORIZONTAL) {
        grid_value_x = (m1_layer->getWidth() + m1_layer->getSpacing()) / double(tech_.lef_database_microns);
        grid_value_y = (m2_layer->getWidth() + m2_layer->getSpacing()) / double(tech_.lef_database_microns);
      } else {
        Assert(false, "layer metal1 and layer m2 must have different orientations\n");
      }
      //std::cout << grid_value_x << "  " << grid_value_y << "\n";
    } else {
      Assert(false, "Cannot find layer metal1 and layer metal2");
    }
    SetGridValue(grid_value_x, grid_value_y);
  }
  auto site = lib->getSites().begin();
  SetRowHeight(site->getHeight() / double(tech_.lef_database_microns));
  //std::cout << site->getName() << "  " << site->getWidth() / double(lef_database_microns) << "  " << row_height_ << "\n";
  double residual = std::fmod(tech_.row_height_, tech_.grid_value_y_);
  Assert(residual < 1e-6, "Site height is not integer multiple of grid value in Y");

  // 4. load all macros, aka gate types, block types, cell types
  //std::cout << lib->getName() << " lib\n";
  double llx = 0, lly = 0, urx = 0, ury = 0;
  int width = 0, height = 0;
  for (auto &&macro: lib->getMasters()) {
    std::string macro_name(macro->getName());
    width = int(std::round((macro->getWidth() / tech_.grid_value_x_ / tech_.lef_database_microns)));
    height = int(std::round((macro->getHeight() / tech_.grid_value_y_ / tech_.lef_database_microns)));
    auto blk_type = AddBlockType(macro_name, width, height);
    if (macro_name.find("welltap") != std::string::npos) {
      tech_.well_tap_cell_ = blk_type;
    }
    //std::cout << macro->getName() << "\n";
    //std::cout << macro->getWidth()/grid_value_x_/lef_database_microns << "  " << macro->getHeight()/grid_value_y_/lef_database_microns << "\n";
    for (auto &&terminal: macro->getMTerms()) {
      std::string pin_name(terminal->getName());
      //if (pin_name == "Vdd" || pin_name == "GND") continue;
      //std::cout << terminal->getName() << " " << terminal->getMPins().begin()->getGeometry().begin()->xMax()/grid_value_x/lef_database_microns << "\n";
      auto new_pin = blk_type->AddPin(pin_name);
      Assert(!terminal->getMPins().empty(), "No physical pins, Macro: " + *blk_type->Name() + ", pin: " + pin_name);
      Assert(!terminal->getMPins().begin()->getGeometry().empty(), "No geometries provided for pin");
      auto geo_shape = terminal->getMPins().begin()->getGeometry().begin();
      llx = geo_shape->xMin() / tech_.grid_value_x_ / tech_.lef_database_microns;
      urx = geo_shape->xMax() / tech_.grid_value_x_ / tech_.lef_database_microns;
      lly = geo_shape->yMin() / tech_.grid_value_y_ / tech_.lef_database_microns;
      ury = geo_shape->yMax() / tech_.grid_value_y_ / tech_.lef_database_microns;
      new_pin->AddRect(llx, lly, urx, ury);
    }
  }
  //tech_.well_tap_cell_->Report();

  auto chip = db->getChip();
  if (chip == nullptr) return;
  auto top_level = chip->getBlock();
  int components_count = 0, pins_count = 0, nets_count = 0;
  components_count = top_level->getInsts().size();
  pins_count = top_level->getBTerms().size();
  nets_count = top_level->getNets().size();
  design_.block_list.reserve(components_count + pins_count);
  design_.iopin_list.reserve(pins_count);
  design_.net_list.reserve(nets_count);

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "components count: " << components_count << "\n"
              << "pin count:        " << pins_count << "\n"
              << "nets count:       " << nets_count << "\n";
  }

  //std::cout << db->getChip()->getBlock()->getName() << "\n";
  //std::cout << db->getChip()->getBlock()->getBTerms().size() << "\n";

  // 5. load all gates
  int llx_int = 0, lly_int = 0;
  design_.def_distance_microns = top_level->getDefUnits();
  odb::adsRect die_area;
  top_level->getDieArea(die_area);
  //std::cout << die_area.xMin() << "\n"
  //          << die_area.xMax() << "\n"
  //          << die_area.yMin() << "\n"
  //          << die_area.yMax() << "\n";
  design_.die_area_offset_x_ = die_area.xMin();
  design_.die_area_offset_y_ = die_area.yMin();
  SetDieArea(die_area.xMin() - die_area.xMin(),
             die_area.xMax() - die_area.xMin(),
             die_area.yMin() - die_area.yMin(),
             die_area.yMax() - die_area.yMin());
  for (auto &&blk: top_level->getInsts()) {
    //std::cout << blk->getName() << "  " << blk->getMaster()->getName() << "\n";
    std::string blk_name(blk->getName());
    std::string blk_type_name(blk->getMaster()->getName());
    blk->getLocation(llx_int, lly_int);
    llx_int = (int) std::round(llx_int / tech_.grid_value_x_ / design_.def_distance_microns);
    lly_int = (int) std::round(lly_int / tech_.grid_value_y_ / design_.def_distance_microns);
    std::string place_status(blk->getPlacementStatus().getString());
    std::string orient(blk->getOrient().getString());
    AddBlock(blk_name, blk_type_name, llx_int, lly_int, StrToPlaceStatus(place_status), StrToOrient(orient));
  }

  for (auto &&iopin: top_level->getBTerms()) {
    //std::cout << iopin->getName() << "\n";
    std::string iopin_name(iopin->getName());
    int iopin_x = 0;
    int iopin_y = 0;
    bool is_loc_set = iopin->getFirstPinLocation(iopin_x, iopin_y);
    IOPin *pin = nullptr;
    if (is_loc_set) {
      pin = AddPlacedIOPin(iopin_name, iopin_x, iopin_x);
    } else {
      pin = AddUnplacedIOPin(iopin_name);
    }
    std::string str_sig_use(iopin->getSigType().getString());
    SignalUse sig_use = StrToSignalUse(str_sig_use);
    pin->SetUse(sig_use);

    std::string str_sig_dir(iopin->getIoType().getString());
    SignalDirection sig_dir = StrToSignalDirection(str_sig_dir);
    pin->SetDirection(sig_dir);
  }

  //std::cout << "Nets:\n";
  for (auto &&net: top_level->getNets()) {
    //std::cout << net->getName() << "\n";
    std::string net_name(net->getName());
    int net_cap = int(net->getITermCount() + net->getBTermCount());
    auto new_net = AddNet(net_name, net_cap, design_.normal_signal_weight);
    for (auto &&bterm: net->getBTerms()) {
      //std::cout << "  ( PIN " << bterm->getName() << ")  \t";
      std::string iopin_name(bterm->getName());
      IOPin *iopin = GetIOPin(iopin_name);
      Net *io_net = GetNet(net_name);
      iopin->SetNet(io_net);
    }
    for (auto &&iterm: net->getITerms()) {
      //std::cout << "  (" << iterm->getInst()->getName() << "  " << iterm->getMTerm()->getName() << ")  \t";
      std::string blk_name(iterm->getInst()->getName());
      std::string pin_name(iterm->getMTerm()->getName());
      Block *blk_ptr = GetBlock(blk_name);
      auto pin = blk_ptr->Type()->GetPin(pin_name);
      new_net->AddBlockPinPair(blk_ptr, pin);
    }
    //std::cout << "\n";
  }
}
#endif

void Circuit::ReadLefFile(std::string const &name_of_file) {
  /****
  * This is a naive lef parser, it cannot cover all corner cases
  * Please use other APIs to build a circuit if necessary
  * ****/
  std::ifstream ist(name_of_file.c_str());
  Assert(ist.is_open(), "Cannot open input file: " + name_of_file);
  std::cout << "Loading LEF file" << "\n";
  std::string line;

  // 1. find DATABASE MICRONS
  tech_.lef_database_microns = 0;
  while ((tech_.lef_database_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (!line.empty() && line[0] == '#') continue;
    if (line.find("DATABASE MICRONS") != std::string::npos) {
      std::vector<std::string> line_field;
      StrSplit(line, line_field);
      Assert(line_field.size() >= 3, "Invalid UNITS declaration: expecting 3 fields");
      try {
        tech_.lef_database_microns = std::stoi(line_field[2]);
      } catch (...) {
        std::cout << line << "\n";
        Assert(false, "Invalid stoi conversion:" + line_field[2]);
      }
    }
  }
  std::cout << "DATABASE MICRONS " << tech_.lef_database_microns << "\n";

  // 2. find MANUFACTURINGGRID
  tech_.manufacturing_grid = 0;
  while ((tech_.manufacturing_grid <= 1e-10) && !ist.eof()) {
    getline(ist, line);
    if (!line.empty() && line[0] == '#') continue;
    if (line.find("LAYER") != std::string::npos) {
      tech_.manufacturing_grid = 1.0 / tech_.lef_database_microns;
      std::cout << "  WARNING:\n  MANUFACTURINGGRID not specified explicitly, using 1.0/DATABASE MICRONS instead\n";
    }
    if (line.find("MANUFACTURINGGRID") != std::string::npos) {
      std::vector<std::string> grid_field;
      StrSplit(line, grid_field);
      Assert(grid_field.size() >= 2, "Invalid MANUFACTURINGGRID declaration: expecting 2 fields");
      try {
        tech_.manufacturing_grid = std::stod(grid_field[1]);
      } catch (...) {
        Assert(false, "Invalid stod conversion:\n" + line);
      }
      break;
    }
  }
  Assert(tech_.manufacturing_grid > 0, "Cannot find or invalid MANUFACTURINGGRID");
  std::cout << "MANUFACTURINGGRID: " << tech_.manufacturing_grid << "\n";

  // 3. read metal layer
  static std::vector<std::string> metal_identifier_list{"m", "M", "metal", "Metal"};
  while (!ist.eof()) {
    if (!line.empty() && line[0] == '#') {
      getline(ist, line);
      continue;
    }
    if (line.find("LAYER") != std::string::npos) {
      std::vector<std::string> layer_field;
      StrSplit(line, layer_field);
      Assert(layer_field.size() == 2, "Invalid LAYER, expect only: LAYER layerName\n\tgot: " + line);
      int first_digit_pos = FindFirstDigit(layer_field[1]);
      std::string metal_id(layer_field[1], 0, first_digit_pos);
      if (std::find(metal_identifier_list.begin(), metal_identifier_list.end(), metal_id)
          != metal_identifier_list.end()) {
        std::string end_layer_flag = "END " + layer_field[1];
        MetalLayer *metal_layer = AddMetalLayer(layer_field[1]);
        do {
          getline(ist, line);
          if (!line.empty() && line[0] == '#') continue;
          if (line.find("DIRECTION") != std::string::npos) {
            std::vector<std::string> direction_field;
            StrSplit(line, direction_field);
            Assert(direction_field.size() >= 2, "Invalid DIRECTION\n" + line);
            MetalDirection direction = StrToMetalDirection(direction_field[1]);
            metal_layer->SetDirection(direction);
          }
          if (line.find("AREA") != std::string::npos) {
            std::vector<std::string> area_field;
            StrSplit(line, area_field);
            Assert(area_field.size() >= 2, "Invalid AREA\n" + line);
            try {
              double area = std::stod(area_field[1]);
              metal_layer->SetArea(area);
            } catch (...) {
              Assert(false, "Invalid stod conversion\n" + line);
            }
          }
          if (line.find("WIDTH") != std::string::npos) {
            std::vector<std::string> width_field;
            StrSplit(line, width_field);
            if (width_field.size() != 2) continue;
            try {
              double width = std::stod(width_field[1]);
              metal_layer->SetWidth(width);
            } catch (...) {
              Assert(false, "Invalid stod conversion:\n" + line);
            }
          }
          if (line.find("SPACING") != std::string::npos &&
              line.find("SPACINGTABLE") == std::string::npos &&
              line.find("ENDOFLINE") == std::string::npos) {
            std::vector<std::string> spacing_field;
            StrSplit(line, spacing_field);
            Assert(spacing_field.size() >= 2, "Invalid SPACING\n" + line);
            try {
              double spacing = std::stod(spacing_field[1]);
              metal_layer->SetSpacing(spacing);
            } catch (...) {
              Assert(false, "Invalid stod conversion:\n" + line);
            }
          }
          if (line.find("PITCH") != std::string::npos) {
            std::vector<std::string> pitch_field;
            StrSplit(line, pitch_field);
            int pch_sz = pitch_field.size();
            Assert(pch_sz >= 2, "Invalid PITCH\n" + line);
            if (pch_sz == 2) {
              try {
                double pitch = std::stod(pitch_field[1]);
                metal_layer->SetPitch(pitch, pitch);
              } catch (...) {
                Assert(false, "Invalid stod conversion:\n" + line);
              }
            } else {
              try {
                double x_pitch = std::stod(pitch_field[1]);
                double y_pitch = std::stod(pitch_field[2]);
                metal_layer->SetPitch(x_pitch, y_pitch);
              } catch (...) {
                Assert(false, "Invalid stod conversion:\n" + line);
              }
            }
          }
        } while (line.find(end_layer_flag) == std::string::npos && !ist.eof());
      }
    }
    getline(ist, line);
    if (line.find("VIA") != std::string::npos || line.find("MACRO") != std::string::npos) break;
  }
  //ReportMetalLayers();
  if (!tech_.grid_set_) {
    if (tech_.metal_list.size() < 2) {
      SetGridValue(tech_.manufacturing_grid, tech_.manufacturing_grid);
      std::cout << "No enough metal layers to specify horizontal and vertical pitch\n"
                << "Using manufacturing grid as grid values\n";
    } else if (tech_.metal_list[0].PitchY() <= 0 || tech_.metal_list[1].PitchX() <= 0) {
      SetGridValue(tech_.manufacturing_grid, tech_.manufacturing_grid);
      std::cout << "Invalid metal pitch\n"
                << "Using manufacturing grid as grid values\n";
    } else {
      SetGridUsingMetalPitch();
    }
    std::cout << "Grid Value: " << tech_.grid_value_x_ << "  " << tech_.grid_value_y_ << "\n";
  }

  // 4. read block type information
  while (!ist.eof()) {
    if (!line.empty() && line[0] == '#') {
      getline(ist, line);
      continue;
    }
    if (line.find("MACRO") != std::string::npos) {
      std::vector<std::string> line_field;
      StrSplit(line, line_field);
      Assert(line_field.size() >= 2, "Invalid type name: expecting 2 fields\n" + line);
      std::string block_type_name = line_field[1];
      //std::cout << block_type_name << "\n";
      BlockType *new_block_type = nullptr;
      int width = 0, height = 0;
      std::string end_macro_flag = "END " + line_field[1];
      do {
        getline(ist, line);
        if (!line.empty() && line[0] == '#') continue;
        while ((width == 0) && (height == 0) && !ist.eof()) {
          if (line.find("SIZE") != std::string::npos) {
            std::vector<std::string> size_field;
            StrSplit(line, size_field);
            try {
              width = (int) (std::round(std::stod(size_field[1]) / tech_.grid_value_x_));
              height = (int) (std::round(std::stod(size_field[3]) / tech_.grid_value_y_));
            } catch (...) {
              Assert(false, "Invalid stod conversion:\n" + line);
            }
            new_block_type = AddBlockType(block_type_name, width, height);
            //std::cout << "  type width, height: " << new_block_type->Width() << " " << new_block_type->Height() << "\n";
          }
          getline(ist, line);
        }

        if (line.find("PIN") != std::string::npos) {
          std::vector<std::string> pin_field;

          StrSplit(line, pin_field);
          Assert(pin_field.size() >= 2, "Invalid pin name: expecting 2 fields\n" + line);

          std::string pin_name = pin_field[1];
          std::string end_pin_flag = "END " + pin_name;
          Pin *new_pin = nullptr;
          new_pin = new_block_type->AddPin(pin_name);
          // skip to "PORT" rectangle list
          do {
            getline(ist, line);
            if (!line.empty() && line[0] == '#') continue;
          } while (line.find("PORT") == std::string::npos && !ist.eof());

          double llx = 0, lly = 0, urx = 0, ury = 0;
          do {
            getline(ist, line);
            if (!line.empty() && line[0] == '#') continue;
            if (line.find("RECT") != std::string::npos) {
              //std::cout << line << "\n";
              std::vector<std::string> rect_field;
              StrSplit(line, rect_field);
              Assert(rect_field.size() >= 5, "Invalid rect definition: expecting 5 fields\n" + line);
              try {
                llx = std::stod(rect_field[1]) / tech_.grid_value_x_;
                lly = std::stod(rect_field[2]) / tech_.grid_value_y_;
                urx = std::stod(rect_field[3]) / tech_.grid_value_x_;
                ury = std::stod(rect_field[4]) / tech_.grid_value_y_;
              } catch (...) {
                Assert(false, "Invalid stod conversion:\n" + line);
              }
              new_pin->AddRect(llx, lly, urx, ury);
            }
          } while (line.find(end_pin_flag) == std::string::npos && !ist.eof());
          Assert(!new_pin->Empty(), "Pin has no RECTs: " + *new_pin->Name());
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());
      Assert(!new_block_type->Empty(), "MACRO has no PINs: " + *new_block_type->Name());
    }
    getline(ist, line);
  }
  std::cout << "LEF file loading complete: " << name_of_file << "\n";
  //ReportBlockType();
}

void Circuit::ReadDefFile(std::string const &name_of_file) {
  /****
   * This is a naive def parser, it cannot cover all corner cases
   * Please use other APIs to build a circuit if this naive def parser cannot satisfy your needs
   * ****/
  std::ifstream ist(name_of_file.c_str());
  Assert(ist.is_open(), "Cannot open input file: " + name_of_file);
  std::cout << "Loading DEF file" << std::endl;
  std::string line;

  bool component_section_exist = false;
  int components_count = 0;
  bool pins_section_exist = false;
  int pins_count = 0;
  bool nets_section_exist = false;
  int nets_count = 0;

  while (!ist.eof()) {
    getline(ist, line);
    if (!component_section_exist) {
      if (line.find("COMPONENTS") != std::string::npos) {
        std::vector<std::string> components_field;
        StrSplit(line, components_field);
        Assert(components_field.size() == 2, "Improper use of COMPONENTS?\n" + line);
        try {
          components_count = std::stoi(components_field[1]);
          std::cout << "COMPONENTS:  " << components_count << "\n";
          component_section_exist = true;
        } catch (...) {
          Assert(false, "Invalid stoi conversion:\n" + line);
        }
      }
    }
    if (!pins_section_exist) {
      if (line.find("PINS") != std::string::npos) {
        std::vector<std::string> pins_field;
        StrSplit(line, pins_field);
        Assert(pins_field.size() == 2, "Improper use of PINS?\n" + line);
        try {
          pins_count = std::stoi(pins_field[1]);
          std::cout << "PINS:  " << pins_count << "\n";
          pins_section_exist = true;
        } catch (...) {
          Assert(false, "Invalid stoi conversion:\n" + line);
        }
      }
    }
    if (!nets_section_exist) {
      if ((line.find("NETS") != std::string::npos) && (line.find("SPECIALNETS") == std::string::npos)) {
        std::vector<std::string> nets_field;
        StrSplit(line, nets_field);
        Assert(nets_field.size() == 2, "Improper use of NETS?\n" + line);
        try {
          nets_count = std::stoi(nets_field[1]);
          std::cout << "NETS:  " << nets_count << "\n";
          nets_section_exist = true;
        } catch (...) {
          Assert(false, "Invalid stoi conversion:\n" + line);
        }
      }
    }

    if (component_section_exist && pins_section_exist && nets_section_exist) break;
  }
  ist.clear();
  ist.seekg(0, std::ios::beg);
  design_.block_list.reserve(components_count + pins_count);
  design_.iopin_list.reserve(pins_count);
  design_.net_list.reserve(nets_count);

  // find UNITS DISTANCE MICRONS
  design_.def_distance_microns = 0;
  while ((design_.def_distance_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DISTANCE MICRONS") != std::string::npos) {
      std::vector<std::string> line_field;
      StrSplit(line, line_field);
      Assert(line_field.size() >= 4, "Invalid UNITS declaration: expecting 4 fields");
      try {
        design_.def_distance_microns = std::stoi(line_field[3]);
      } catch (...) {
        Assert(false, "Invalid stoi conversion (UNITS DISTANCE MICRONS):\n" + line);
      }
    }
  }
  Assert(design_.def_distance_microns > 0,
         "Invalid/null UNITS DISTANCE MICRONS: " + std::to_string(design_.def_distance_microns));
  //std::cout << "DISTANCE MICRONS " << def_distance_microns << "\n";

  // find DIEAREA
  int def_left = 0;
  int def_right = 0;
  int def_bottom = 0;
  int def_top = 0;
  double factor_x = tech_.grid_value_x_ * design_.def_distance_microns;
  double factor_y = tech_.grid_value_y_ * design_.def_distance_microns;
  while ((def_left == 0) && (def_right == 0) && (def_bottom == 0) && (def_top == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DIEAREA") != std::string::npos) {
      std::vector<std::string> die_area_field;
      StrSplit(line, die_area_field);
      //std::cout << line << "\n";
      Assert(die_area_field.size() >= 9, "Invalid UNITS declaration: expecting 9 fields");
      try {
        def_left = (int) std::round(std::stoi(die_area_field[2]) / factor_x);
        def_bottom = (int) std::round(std::stoi(die_area_field[3]) / factor_y);
        def_right = (int) std::round(std::stoi(die_area_field[6]) / factor_x);
        def_top = (int) std::round(std::stoi(die_area_field[7]) / factor_y);
        SetBoundary(def_left, def_right, def_bottom, def_top);
      } catch (...) {
        Assert(false, "Invalid stoi conversion (DIEAREA):\n" + line);
      }
    }
  }
  //std::cout << "DIEAREA ( " << region_left_ << " " << region_bottom_ << " ) ( " << region_right_ << " " << region_top_ << " )\n";

  // find COMPONENTS
  if (component_section_exist) {
    while ((line.find("COMPONENTS") == std::string::npos) && !ist.eof()) {
      getline(ist, line);
    }
    //std::cout << line << "\n";
    getline(ist, line);

    // a). parse the body of components
    while ((line.find("END COMPONENTS") == std::string::npos) && !ist.eof()) {
      //std::cout << line << "\t";
      std::vector<std::string> block_declare_field;
      StrSplit(line, block_declare_field);
      if (block_declare_field.size() <= 1) {
        getline(ist, line);
        continue;
      }
      Assert(block_declare_field.size() >= 3,
             "Invalid block declaration, expecting at least: - compName modelName ;\n" + line);
      //std::cout << block_declare_field[0] << " " << block_declare_field[1] << "\n";
      if (block_declare_field.size() == 3) {
        AddBlock(block_declare_field[1], block_declare_field[2], 0, 0, UNPLACED_, N_);
      } else if (block_declare_field.size() == 10) {
        PlaceStatus place_status = StrToPlaceStatus(block_declare_field[4]);
        BlockOrient orient = StrToOrient(block_declare_field[9]);
        int llx = 0, lly = 0;
        try {
          llx = (int) std::round(std::stoi(block_declare_field[6]) / factor_x);
          lly = (int) std::round(std::stoi(block_declare_field[7]) / factor_y);
        } catch (...) {
          Assert(false, "Invalid stoi conversion:\n" + line);
        }
        AddBlock(block_declare_field[1], block_declare_field[2], llx, lly, place_status, orient);
      } else {
        Assert(false, "Unknown block declaration!");
      }
      getline(ist, line);
    }
  }

  // find PINS
  if (pins_section_exist) {
    while ((line.find("PINS") == std::string::npos) && !ist.eof()) {
      getline(ist, line);
    }
    //std::cout << line << "\n";
    getline(ist, line);

    while ((line.find("END PINS") == std::string::npos) && !ist.eof()) {
      if (line.find('-') != std::string::npos && line.find("NET") != std::string::npos) {
        //std::cout << line << "\n";
        std::vector<std::string> io_pin_field;
        StrSplit(line, io_pin_field);
        //IOPin *iopin = nullptr;
        //iopin = AddUnplacedIOPin(io_pin_field[1]);
        AddUnplacedIOPin(io_pin_field[1]);
      }
      getline(ist, line);
    }
  }

  if (nets_section_exist) {
    while (line.find("NETS") == std::string::npos && !ist.eof()) {
      getline(ist, line);
    }
    // a). find the number of nets
    std::vector<std::string> nets_size_field;
    StrSplit(line, nets_size_field);
    //std::cout << line << "\n";
    getline(ist, line);
    // the following is a hack now, cannot handle all cases, probably need to use BISON in the future if necessary
    while ((line.find("END NETS") == std::string::npos) && !ist.eof()) {
      if (!line.empty() && line[0] == '#') {
        getline(ist, line);
        continue;
      }
      if (line.find('-') != std::string::npos) {
        //std::cout << line << "\n";
        std::vector<std::string> net_field;
        StrSplit(line, net_field);
        Assert(net_field.size() >= 2, "Invalid net declaration, expecting at least: - netName\n" + line);
        //std::cout << "\t" << net_field[0] << " " << net_field[1] << "\n";
        Net *new_net = nullptr;
        std::cout << "Circuit::ReadDefFile(), this naive parser is broken, please do not use it\n";
        if (net_field[1].find("Reset") != std::string::npos) {
          //std::cout << net_field[1] << "\n";
          new_net = AddNet(net_field[1], 100, design_.reset_signal_weight);
        } else {
          new_net = AddNet(net_field[1], 100, design_.normal_signal_weight);
        }
        while (true) {
          getline(ist, line);
          if (!line.empty() && line[0] == '#') {
            continue;
          }
          //std::cout << line << "\n";
          std::vector<std::string> pin_field;
          StrSplit(line, pin_field);
          if ((pin_field.size() % 4 != 0)) {
            Assert(false, "Invalid net declaration, expecting 4n fields, where n >= 2:\n" + line);
          }
          for (size_t i = 0; i < pin_field.size(); i += 4) {
            //std::cout << "     " << pin_field[i+1] << " " << pin_field[i+2];
            if (pin_field[i + 1] == "PIN") {
              GetIOPin(pin_field[i + 2])->SetNet(new_net);
              continue;
            }
            //std::cout << net_field[1] << "  " << pin_field[i + 1] << "\n";
            Block *block = GetBlock(pin_field[i + 1]);
            auto pin = block->Type()->GetPin(pin_field[i + 2]);
            new_net->AddBlockPinPair(block, pin);
          }
          //std::cout << "\n";
          if (line.find(';') != std::string::npos) break;
        }
        //Assert(!new_net->blk_pin_list.empty(), "Net " + net_field[1] + " has no blk_pin_pair");
        if (new_net->blk_pin_list.empty()) {
          NetListPopBack();
        }
        //Warning(new_net->blk_pin_list.size() == 1, "Net " + net_field[1] + " has only one blk_pin_pair");
      }
      getline(ist, line);
    }
  }
  std::cout << "DEF file loading complete: " << name_of_file << "\n";
}

MetalLayer *Circuit::AddMetalLayer(std::string &metal_name, double width, double spacing) {
  Assert(!IsMetalLayerExist(metal_name), "MetalLayer exist, cannot create this MetalLayer again: " + metal_name);
  int map_size = tech_.metal_name_map.size();
  auto ret = tech_.metal_name_map.insert(std::pair<std::string, int>(metal_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  tech_.metal_list.emplace_back(width, spacing, name_num_pair_ptr);
  return &(tech_.metal_list.back());
}

void Circuit::ReportMetalLayers() {
  for (auto &metal_layer: tech_.metal_list) {
    metal_layer.Report();
  }
}

BlockTypeWell *Circuit::AddBlockTypeWell(BlockTypeCluster *cluster, BlockType *blk_type, bool is_plug) {
  well_info_.well_list_.emplace_back(blk_type);
  blk_type->ptr_well_ = &(well_info_.well_list_.back());
  blk_type->ptr_well_->SetPlug(is_plug);
  blk_type->ptr_well_->SetCluster(cluster);
  return blk_type->ptr_well_;
}

void Circuit::ReportWellShape() {
  for (auto &cluster: well_info_.cluster_list_) {
    //cluster.GetPlug()->Report();
    cluster.GetUnplug()->Report();
  }
}

BlockType *Circuit::AddBlockType(std::string &block_type_name, int width, int height) {
  Assert(!IsBlockTypeExist(block_type_name),
         "BlockType exist, cannot create this block type again: " + block_type_name);
  auto ret = tech_.block_type_map.insert(std::pair<std::string, BlockType *>(block_type_name, nullptr));
  auto tmp_ptr = new BlockType(&(ret.first->first), width, height);
  ret.first->second = tmp_ptr;
  if (tmp_ptr->Area() > INT_MAX) tmp_ptr->Report();
  return tmp_ptr;
}

void Circuit::ReportBlockType() {
  std::cout << "Total BlockType: " << tech_.block_type_map.size() << std::endl;
  for (auto &pair: tech_.block_type_map) {
    pair.second->Report();
  }
}

void Circuit::CopyBlockType(Circuit &circuit) {
  BlockType *blk_type = nullptr;
  BlockType *blk_type_new = nullptr;
  std::string type_name, pin_name;
  for (auto &item: circuit.tech_.block_type_map) {
    blk_type = item.second;
    type_name = *(blk_type->Name());
    if (type_name == "PIN") continue;
    blk_type_new = AddBlockType(type_name, blk_type->Width(), blk_type->Height());
    for (auto &pin: blk_type->pin_list) {
      pin_name = *(pin.Name());
      blk_type_new->AddPin(pin_name, pin.OffsetX(), pin.OffsetY());
    }
  }
}

bool Circuit::IsBlockExist(std::string &block_name) {
  return !(design_.block_name_map.find(block_name) == design_.block_name_map.end());
}

int Circuit::BlockIndex(std::string &block_name) {
  auto ret = design_.block_name_map.find(block_name);
  if (ret == design_.block_name_map.end()) {
    Assert(false, "Block does not exist, cannot find its index: " + block_name);
  }
  return ret->second;
}

Block *Circuit::GetBlock(std::string &block_name) {
  return &design_.block_list[BlockIndex(block_name)];
}

void Circuit::AddBlock(std::string &block_name,
                       BlockType *block_type,
                       int llx,
                       int lly,
                       bool movable,
                       BlockOrient orient,
                       bool is_real_cel) {
  PlaceStatus place_status;
  if (movable) {
    place_status = UNPLACED_;
  } else {
    place_status = FIXED_;
  }
  AddBlock(block_name, block_type, llx, lly, place_status, orient, is_real_cel);
}

void Circuit::AddBlock(std::string &block_name,
                       std::string &block_type_name,
                       int llx,
                       int lly,
                       bool movable,
                       BlockOrient orient,
                       bool is_real_cel) {
  BlockType *block_type = GetBlockType(block_type_name);
  AddBlock(block_name, block_type, llx, lly, movable, orient, is_real_cel);
}

void Circuit::AddBlock(std::string &block_name,
                       BlockType *block_type,
                       int llx,
                       int lly,
                       PlaceStatus place_status,
                       BlockOrient orient,
                       bool is_real_cel) {
  Assert(design_.net_list.empty(), "Cannot add new Block, because net_list now is not empty");
  Assert(!IsBlockExist(block_name), "Block exists, cannot create this block again: " + block_name);
  int map_size = design_.block_name_map.size();
  auto ret = design_.block_name_map.insert(std::pair<std::string, int>(block_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  design_.block_list.emplace_back(block_type, name_num_pair_ptr, llx, lly, place_status, orient);

  // update statistics of blocks
  long int old_tot_area = design_.tot_blk_area_;
  design_.tot_blk_area_ += design_.block_list.back().Area();
  Assert(old_tot_area < design_.tot_blk_area_, "Total Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
  design_.tot_width_ += design_.block_list.back().Width();
  design_.tot_height_ += design_.block_list.back().Height();
  if (is_real_cel) {
    ++design_.blk_count_;
  }
  if (design_.block_list.back().IsMovable()) {
    ++design_.tot_mov_blk_num_;
    old_tot_area = design_.tot_mov_block_area_;
    design_.tot_mov_block_area_ += design_.block_list.back().Area();
    Assert(old_tot_area < design_.tot_mov_block_area_,
           "Total Movable Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
    design_.tot_mov_width_ += design_.block_list.back().Width();
    design_.tot_mov_height_ += design_.block_list.back().Height();
  }
  if (design_.block_list.back().Height() < design_.blk_min_height_) {
    design_.blk_min_height_ = design_.block_list.back().Height();
  }
  if (design_.block_list.back().Height() > design_.blk_max_height_) {
    design_.blk_max_height_ = design_.block_list.back().Height();
  }
  if (design_.block_list.back().Width() < design_.blk_min_width_) {
    design_.blk_min_width_ = design_.block_list.back().Width();
  }
  if (design_.block_list.back().Width() > design_.blk_min_width_) {
    design_.blk_max_width_ = design_.block_list.back().Width();
  }
}

void Circuit::AddBlock(std::string &block_name,
                       std::string &block_type_name,
                       int llx,
                       int lly,
                       PlaceStatus place_status,
                       BlockOrient orient,
                       bool is_real_cel) {
  BlockType *block_type = GetBlockType(block_type_name);
  AddBlock(block_name, block_type, llx, lly, place_status, orient, is_real_cel);
}

void Circuit::AddDummyIOPinType() {
  std::string iopin_type_name("PIN");
  auto io_pin_type = AddBlockType(iopin_type_name, 0, 0);
  std::string tmp_pin_name("pin");
  io_pin_type->AddPin(tmp_pin_name, 0, 0);
  tech_.io_dummy_blk_type_ = io_pin_type;
}

bool Circuit::IsIOPinExist(std::string &iopin_name) {
  return !(design_.iopin_name_map.find(iopin_name) == design_.iopin_name_map.end());
}

int Circuit::IOPinIndex(std::string &iopin_name) {
  auto ret = design_.iopin_name_map.find(iopin_name);
  if (ret == design_.iopin_name_map.end()) {
    Assert(false, "IOPIN does not exist, cannot find its index: " + iopin_name);
  }
  return ret->second;
}

IOPin *Circuit::GetIOPin(std::string &iopin_name) {
  return &design_.iopin_list[IOPinIndex(iopin_name)];
}

IOPin *Circuit::AddUnplacedIOPin(std::string &iopin_name) {
  Assert(design_.net_list.empty(), "Cannot add new IOPIN, because net_list now is not empty");
  Assert(!IsIOPinExist(iopin_name), "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int map_size = design_.iopin_name_map.size();
  auto ret = design_.iopin_name_map.insert(std::pair<std::string, int>(iopin_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  design_.iopin_list.emplace_back(name_num_pair_ptr);
  return &(design_.iopin_list.back());
}

IOPin *Circuit::AddPlacedIOPin(std::string &iopin_name, int lx, int ly) {
  Assert(design_.net_list.empty(), "Cannot add new IOPIN, because net_list now is not empty");
  Assert(!IsIOPinExist(iopin_name), "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int map_size = design_.iopin_name_map.size();
  auto ret = design_.iopin_name_map.insert(std::pair<std::string, int>(iopin_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  design_.iopin_list.emplace_back(name_num_pair_ptr, lx, ly);
  design_.pre_placed_io_count_ += 1;

  // add a dummy cell corresponding to this IOPIN to block_list.
  AddBlock(iopin_name, tech_.io_dummy_blk_type_, lx, ly, false, N_, false);

  return &(design_.iopin_list.back());
}

IOPin *Circuit::AddIOPin(std::string &iopin_name, PlaceStatus place_status, int lx, int ly) {
  if (place_status == UNPLACED_) {
    AddUnplacedIOPin(iopin_name);
  } else {
    AddPlacedIOPin(iopin_name, lx, ly);
  }
}

void Circuit::ReportIOPin() {
  for (auto &iopin: design_.iopin_list) {
    iopin.Report();
  }
}

bool Circuit::IsNetExist(std::string &net_name) {
  return !(design_.net_name_map.find(net_name) == design_.net_name_map.end());
}

int Circuit::NetIndex(std::string &net_name) {
  Assert(IsNetExist(net_name), "Net does not exist, cannot find its index: " + net_name);
  return design_.net_name_map.find(net_name)->second;
}

Net *Circuit::GetNet(std::string &net_name) {
  return &design_.net_list[NetIndex(net_name)];
}

void Circuit::AddToNetMap(std::string &net_name) {
  int map_size = design_.net_name_map.size();
  design_.net_name_map.insert(std::pair<std::string, int>(net_name, map_size));
}

Net *Circuit::AddNet(std::string &net_name, int capacity, double weight) {
  /****
   * Returns a pointer to the newly created Net.
   * @param net_name: name of the net
   * @param capacity: maximum number of possible pins in this net
   * @param weight:   weight of this net
   * ****/
  Assert(!IsNetExist(net_name), "Net exists, cannot create this net again: " + net_name);
  AddToNetMap(net_name);
  std::pair<const std::string, int> *name_num_pair_ptr = &(*design_.net_name_map.find(net_name));
  design_.net_list.emplace_back(name_num_pair_ptr, capacity, weight);
  return &design_.net_list.back();
}

void Circuit::NetListPopBack() {
  design_.net_name_map.erase(design_.net_list.back().NameStr());
  design_.net_list.pop_back();
}

/*
bool Circuit::CreatePseudoNet(std::string &drive_blk, std::string &drive_pin,
                              std::string &load_blk, std::string &load_pin, double weight) {

}

bool Circuit::CreatePseudoNet(Block *drive_blk, int drive_pin, Block *load_blk, int load_pin, double weight) {

}

bool Circuit::RemovePseudoNet(std::string &drive_blk, std::string &drive_pin, std::string &load_blk, std::string &load_pin) {

}

bool Circuit::RemovePseudoNet(Block *drive_blk, int drive_pin, Block *load_blk, int load_pin) {

}

void Circuit::RemoveAllPseudoNets() {

}
 */

void Circuit::SetGridValue(double grid_value_x, double grid_value_y) {
  Assert(grid_value_x > 0, "grid_value_x must be a positive real number!");
  Assert(grid_value_y > 0, "grid_value_y must be a positive real number!");
  Assert(!tech_.grid_set_, "once set, grid_value cannot be changed!");
  tech_.grid_value_x_ = grid_value_x;
  tech_.grid_value_y_ = grid_value_y;
  tech_.grid_set_ = true;
}

void Circuit::SetGridUsingMetalPitch() {
  SetGridValue(tech_.metal_list[0].PitchY(), tech_.metal_list[1].PitchX());
}

void Circuit::ReadCellFile(std::string const &name_of_file) {
  std::ifstream ist(name_of_file.c_str());
  Assert(ist.is_open(), "Cannot open input file: " + name_of_file);
  std::cout << "Loading CELL file: " << name_of_file << "\n";
  std::string line;

  while (!ist.eof()) {
    getline(ist, line);
    if (line.empty()) continue;
    if (line.find("LAYER") != std::string::npos) {
      if (line.find("LEGALIZER") != std::string::npos) {
        std::vector<std::string> legalizer_fields;
        double same_diff_spacing = 0;
        double any_diff_spacing = 0;
        do {
          getline(ist, line);
          StrSplit(line, legalizer_fields);
          Assert(legalizer_fields.size() == 2, "Expect: SPACING + Value, get: " + line);
          if (legalizer_fields[0] == "SAME_DIFF_SPACING") {
            try {
              same_diff_spacing = std::stod(legalizer_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + legalizer_fields[1]);
            }
          } else if (legalizer_fields[0] == "ANY_DIFF_SPACING") {
            try {
              any_diff_spacing = std::stod(legalizer_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + legalizer_fields[1]);
            }
          }
        } while (line.find("END LEGALIZER") == std::string::npos && !ist.eof());
        //std::cout << "same diff spacing: " << same_diff_spacing << "\n any diff spacing: " << any_diff_spacing << "\n";
        SetLegalizerSpacing(same_diff_spacing, any_diff_spacing);
      } else {
        std::vector<std::string> well_fields;
        StrSplit(line, well_fields);
        bool is_n_well = (well_fields[1] == "nwell");
        if (!is_n_well) Assert(well_fields[1] == "pwell", "Unknow N/P well type: " + well_fields[1]);
        std::string end_layer_flag = "END " + well_fields[1];
        double width = 0;
        double spacing = 0;
        double op_spacing = 0;
        double max_plug_dist = 0;
        double overhang = 0;
        do {
          if (line.find("MINWIDTH") != std::string::npos) {
            StrSplit(line, well_fields);
            try {
              width = std::stod(well_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + well_fields[1]);
            }
          } else if (line.find("OPPOSPACING") != std::string::npos) {
            StrSplit(line, well_fields);
            try {
              op_spacing = std::stod(well_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + well_fields[1]);
            }
          } else if (line.find("SPACING") != std::string::npos) {
            StrSplit(line, well_fields);
            try {
              spacing = std::stod(well_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + well_fields[1]);
            }
          } else if (line.find("MAXPLUGDIST") != std::string::npos) {
            StrSplit(line, well_fields);
            try {
              max_plug_dist = std::stod(well_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + well_fields[1]);
            }
          } else if (line.find("MAXPLUGDIST") != std::string::npos) {
            StrSplit(line, well_fields);
            try {
              overhang = std::stod(well_fields[1]);
            } catch (...) {
              std::cout << line << std::endl;
              Assert(false, "Invalid stod conversion: " + well_fields[1]);
            }
          } else {}
          getline(ist, line);
        } while (line.find(end_layer_flag) == std::string::npos && !ist.eof());
        if (is_n_well) {
          SetNWellParams(width, spacing, op_spacing, max_plug_dist, overhang);
        } else {
          SetPWellParams(width, spacing, op_spacing, max_plug_dist, overhang);
        }
      }
    }

    if (line.find("MACRO") != std::string::npos) {
      //std::cout << line << "\n";
      std::vector<std::string> macro_fields;
      StrSplit(line, macro_fields);
      std::string end_macro_flag = "END " + macro_fields[1];
      auto cluster = AddBlockTypeCluster();
      do {
        getline(ist, line);
        if (line.find("VERSION") != std::string::npos) {
          std::vector<std::string> version_fields;
          StrSplit(line, version_fields);
          getline(ist, line);
          bool is_plug = false;
          if (line.find("UNPLUG") == std::string::npos) is_plug = true;
          BlockTypeWell *well = AddBlockTypeWell(cluster, version_fields[1], is_plug);
          int lx = 0, ly = 0, ux = 0, uy = 0;
          bool is_n = false;
          do {
            getline(ist, line);
            if (line.find("nwell") != std::string::npos) {
              is_n = true;
            } else if (line.find("RECT") != std::string::npos) {
              std::vector<std::string> shape_fields;
              StrSplit(line, shape_fields);
              try {
                lx = int(std::round(std::stod(shape_fields[1]) / tech_.grid_value_x_));
                ly = int(std::round(std::stod(shape_fields[2]) / tech_.grid_value_y_));
                ux = int(std::round(std::stod(shape_fields[3]) / tech_.grid_value_x_));
                uy = int(std::round(std::stod(shape_fields[4]) / tech_.grid_value_y_));
              } catch (...) {
                Assert(false, "Invalid stod conversion:\n" + line);
              }
              auto blk_type = GetBlockType(version_fields[1]);
              well->SetWellShape(is_n, lx, ly, ux, uy);
              if (is_n) {
                well->SetNWellShape(0, ly, blk_type->Width(), blk_type->Height());
              } else {
                well->SetPWellShape(0, 0, blk_type->Width(), uy);
              }
            }
          } while (line.find("END VERSION") == std::string::npos && !ist.eof());
          Assert(well->IsNPWellAbutted(), "N/P well not abutted: " + version_fields[1]);
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());
      Assert(!cluster->Empty(), "No plug/unplug version provided");
    }
  }
  Assert(!tech_.IsWellInfoSet(), "N/P well technology information not found!");
  //tech_->Report();
  //ReportWellShape();

  std::cout << "CELL file loading complete: " << name_of_file << "\n";
}

void Circuit::LoadImaginaryCellFile() {
  /****
   * Creates fake NP-well information for testing purposes
   * ****/

  // 1. create fake well tap cell
  std::string tap_cell_name("welltap_svt");
  tech_.well_tap_cell_ = AddBlockType(tap_cell_name, MinBlkWidth(), MinBlkHeight());

  // 2. create fake well parameters
  double fake_same_diff_spacing = 0;
  double fake_any_diff_spacing = 0;
  SetLegalizerSpacing(fake_same_diff_spacing, fake_any_diff_spacing);

  double width = 0;
  double spacing = 0;
  double op_spacing = 0;
  double max_plug_dist = 0;
  double overhang = 0;

  width = MinBlkHeight() / 2.0 * tech_.grid_value_y_;
  spacing = MinBlkWidth() * tech_.grid_value_x_;
  op_spacing = MinBlkWidth() * tech_.grid_value_x_;
  max_plug_dist = AveMovBlkWidth() * 10 * tech_.grid_value_x_;

  SetNWellParams(width, spacing, op_spacing, max_plug_dist, overhang);
  SetNWellParams(width, spacing, op_spacing, max_plug_dist, overhang);

  // 3. create fake NP-well geometries for each BlockType
  for (auto &pair : tech_.block_type_map) {
    auto *blk_type = pair.second;
    auto cluster = AddBlockTypeCluster();
    BlockTypeWell *well = AddBlockTypeWell(cluster, blk_type, false);
    int np_edge = blk_type->Height() / 2;
    well->SetNWellShape(0, np_edge, blk_type->Width(), blk_type->Height());
    well->SetPWellShape(0, 0, blk_type->Width(), np_edge);
  }
}

void Circuit::ReportBlockList() {
  for (auto &block: design_.block_list) {
    block.Report();
  }
}

void Circuit::ReportBlockMap() {
  for (auto &it: design_.block_name_map) {
    std::cout << it.first << " " << it.second << "\n";
  }
}

void Circuit::ReportNetList() {
  for (auto &net: design_.net_list) {
    std::cout << *net.Name() << "  " << net.Weight() << "\n";
    for (auto &block_pin_pair: net.blk_pin_list) {
      std::cout << "\t" << " (" << *(block_pin_pair.BlockName()) << " " << *(block_pin_pair.PinName()) << ") " << "\n";
    }
  }
}

void Circuit::ReportNetMap() {
  for (auto &it: design_.net_name_map) {
    std::cout << it.first << " " << it.second << "\n";
  }
}

void Circuit::UpdateNetHPWLHisto() {
  int bin_count = (int) design_.net_histogram_.bin_list_.size();
  design_.net_histogram_.sum_hpwl_.assign(bin_count, 0);
  design_.net_histogram_.ave_hpwl_.assign(bin_count, 0);
  design_.net_histogram_.min_hpwl_.assign(bin_count, DBL_MAX);
  design_.net_histogram_.max_hpwl_.assign(bin_count, DBL_MIN);

  for (auto &net: design_.net_list) {
    int net_size = net.P();
    double hpwl_x = net.HPWLX();
    double hpwl_y = net.HPWLY() * tech_.grid_value_y_ / tech_.grid_value_x_;
    design_.UpdateNetHPWLHisto(net_size, hpwl_x + hpwl_y);
  }

  design_.net_histogram_.tot_hpwl_ = 0;
  for (int i = 0; i < bin_count; ++i) {
    design_.net_histogram_.tot_hpwl_ += design_.net_histogram_.sum_hpwl_[i];
  }
}

void Circuit::ReportBriefSummary() const {
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "  movable blocks: " << TotMovableBlockNum() << "\n"
              << "  blocks: " << TotBlkNum() << "\n"
              << "  nets: " << design_.net_list.size() << "\n"
              << "  grid size x: " << tech_.grid_value_x_ << " um, grid size y: " << tech_.grid_value_y_ << " um\n"
              << "  total block area: " << design_.tot_blk_area_ << "\n"
              << "  total white space: " << (long int) RegionWidth() * (long int) RegionHeight() << "\n"
              << "    left:   " << RegionLLX() << "\n"
              << "    right:  " << RegionURX() << "\n"
              << "    bottom: " << RegionLLY() << "\n"
              << "    top:    " << RegionURY() << "\n"
              << "  white space utility: " << WhiteSpaceUsage() << "\n";
  }
}

void Circuit::NetSortBlkPin() {
  for (auto &net: design_.net_list) {
    net.SortBlkPinList();
  }
}

double Circuit::HPWLX() {
  double hpwlx = 0;
  for (auto &net: design_.net_list) {
    hpwlx += net.HPWLX();
  }
  return hpwlx * GetGridValueX();
}

double Circuit::HPWLY() {
  double hpwly = 0;
  for (auto &net: design_.net_list) {
    hpwly += net.HPWLY();
  }
  return hpwly * GetGridValueY();
}

void Circuit::ReportHPWLHistogramLinear(int bin_num) {
  std::vector<double> hpwl_list;
  double min_hpwl = DBL_MAX;
  double max_hpwl = DBL_MIN;
  hpwl_list.reserve(design_.net_list.size());
  double factor = tech_.grid_value_y_ / tech_.grid_value_x_;
  for (auto &net:design_.net_list) {
    double tmp_hpwl = net.HPWLX() + net.HPWLY() * factor;
    if (net.P() >= 1) {
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

  int tot_count = design_.net_list.size();
  printf("\n");
  printf("                  HPWL histogram (linear scale bins)\n");
  printf("===================================================================\n");
  printf("   HPWL interval         Count\n");
  for (int i = 0; i < bin_num; ++i) {
    double lo = min_hpwl + step * i;
    double hi = lo + step;
    printf("  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
    int percent = std::ceil(50 * count[i] / (double) tot_count);
    for (int j = 0; j < percent; ++j) {
      printf("*");
    }
    printf("\n");
  }
  printf("===================================================================\n");
  printf(" * HPWL unit, grid value in X: %.2e um\n", tech_.grid_value_x_);
  printf("\n");
}

void Circuit::ReportHPWLHistogramLogarithm(int bin_num) {
  std::vector<double> hpwl_list;
  double min_hpwl = DBL_MAX;
  double max_hpwl = DBL_MIN;
  hpwl_list.reserve(design_.net_list.size());
  double factor = tech_.grid_value_y_ / tech_.grid_value_x_;
  for (auto &net:design_.net_list) {
    double tmp_hpwl = net.HPWLX() + net.HPWLY() * factor;
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

  int tot_count = design_.net_list.size();
  printf("\n");
  printf("                  HPWL histogram (log scale bins)\n");
  printf("===================================================================\n");
  printf("   HPWL interval         Count\n");
  for (int i = 0; i < bin_num; ++i) {
    double lo = std::pow(10, min_hpwl + step * i);
    double hi = std::pow(10, min_hpwl + step * (i + 1));
    printf("  [%.1e, %.1e) %8d  ", lo, hi, count[i]);
    int percent = std::ceil(50 * count[i] / (double) tot_count);
    for (int j = 0; j < percent; ++j) {
      printf("*");
    }
    printf("\n");
  }
  printf("===================================================================\n");
  printf(" * HPWL unit, grid value in X: %.2e um\n", tech_.grid_value_x_);
  printf("\n");
}

double Circuit::HPWLCtoCX() {
  double hpwl_c2c_x = 0;
  for (auto &net: design_.net_list) {
    hpwl_c2c_x += net.HPWLCtoCX();
  }
  return hpwl_c2c_x * GetGridValueX();
}

double Circuit::HPWLCtoCY() {
  double hpwl_c2c_y = 0;
  for (auto &net: design_.net_list) {
    hpwl_c2c_y += net.HPWLCtoCY();
  }
  return hpwl_c2c_y * GetGridValueY();
}

void Circuit::WriteDefFileDebug(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);

  // need some header here


  for (auto &block: design_.block_list) {
    ost << "- "
        << *(block.Name()) << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( " + std::to_string((int) (block.LLX() * design_.def_distance_microns * tech_.grid_value_x_)) + " "
            + std::to_string((int) (block.LLY() * design_.def_distance_microns * tech_.grid_value_y_)) + " )" << " "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";

  ost << "NETS " << design_.net_list.size() << " ;\n";
  for (auto &net: design_.net_list) {
    ost << "- "
        << *(net.Name()) << "\n";
    ost << " ";
    for (auto &pin_pair: net.blk_pin_list) {
      ost << " ( " << *(pin_pair.BlockName()) << " " << *(pin_pair.PinName()) << " ) ";
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";

  ost.close();
}

void Circuit::GenMATLABScript(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  for (auto &block: design_.block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
  }
  /*
  for (auto &net: net_list) {
    for (size_t i=0; i<net.iopin_list.size(); i++) {
      for (size_t j=i+1; j<net.iopin_list.size(); j++) {
        ost << "line([" << net.iopin_list[i].abs_x() << "," << net.iopin_list[j].abs_x() << "],[" << net.iopin_list[i].abs_y() << "," << net.iopin_list[j].abs_y() << "],'lineWidth', 0.5)\n";
      }
    }
  }
   */
  ost.close();
}

void Circuit::GenMATLABTable(std::string const &name_of_file, bool only_well_tap) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
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
    for (auto &block: design_.block_list) {
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
  for (auto &block: design_.well_tap_list) {
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

void Circuit::GenMATLABWellTable(std::string const &name_of_file, bool only_well_tap) {
  std::string frame_file = name_of_file + "_outline.txt";
  std::string unplug_file = name_of_file + "_unplug.txt";
  GenMATLABTable(frame_file, only_well_tap);

  std::ofstream ost(unplug_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + unplug_file);

  BlockTypeWell *well;
  RectI *n_well_shape, *p_well_shape;
  if (!only_well_tap) {
    for (auto &block: design_.block_list) {
      well = block.Type()->GetWell();
      if (well != nullptr) {
        n_well_shape = well->GetNWellShape();
        p_well_shape = well->GetPWellShape();
        if (block.Orient() == N_) {
          ost << block.LLX() + n_well_shape->LLX() << "\t"
              << block.LLX() + n_well_shape->URX() << "\t"
              << block.LLX() + n_well_shape->URX() << "\t"
              << block.LLX() + n_well_shape->LLX() << "\t"
              << block.LLY() + n_well_shape->LLY() << "\t"
              << block.LLY() + n_well_shape->LLY() << "\t"
              << block.LLY() + n_well_shape->URY() << "\t"
              << block.LLY() + n_well_shape->URY() << "\t"

              << block.LLX() + p_well_shape->LLX() << "\t"
              << block.LLX() + p_well_shape->URX() << "\t"
              << block.LLX() + p_well_shape->URX() << "\t"
              << block.LLX() + p_well_shape->LLX() << "\t"
              << block.LLY() + p_well_shape->LLY() << "\t"
              << block.LLY() + p_well_shape->LLY() << "\t"
              << block.LLY() + p_well_shape->URY() << "\t"
              << block.LLY() + p_well_shape->URY() << "\n";
        } else if (block.Orient() == FS_) {
          ost << block.LLX() + n_well_shape->LLX() << "\t"
              << block.LLX() + n_well_shape->URX() << "\t"
              << block.LLX() + n_well_shape->URX() << "\t"
              << block.LLX() + n_well_shape->LLX() << "\t"
              << block.URY() - n_well_shape->LLY() << "\t"
              << block.URY() - n_well_shape->LLY() << "\t"
              << block.URY() - n_well_shape->URY() << "\t"
              << block.URY() - n_well_shape->URY() << "\t"

              << block.LLX() + p_well_shape->LLX() << "\t"
              << block.LLX() + p_well_shape->URX() << "\t"
              << block.LLX() + p_well_shape->URX() << "\t"
              << block.LLX() + p_well_shape->LLX() << "\t"
              << block.URY() - p_well_shape->LLY() << "\t"
              << block.URY() - p_well_shape->LLY() << "\t"
              << block.URY() - p_well_shape->URY() << "\t"
              << block.URY() - p_well_shape->URY() << "\n";
        }
      }
    }
  }

  for (auto &block: design_.well_tap_list) {
    well = block.Type()->GetWell();
    if (well != nullptr) {
      n_well_shape = well->GetNWellShape();
      p_well_shape = well->GetPWellShape();
      if (block.Orient() == N_) {
        ost << block.LLX() + n_well_shape->LLX() << "\t"
            << block.LLX() + n_well_shape->URX() << "\t"
            << block.LLX() + n_well_shape->URX() << "\t"
            << block.LLX() + n_well_shape->LLX() << "\t"
            << block.LLY() + n_well_shape->LLY() << "\t"
            << block.LLY() + n_well_shape->LLY() << "\t"
            << block.LLY() + n_well_shape->URY() << "\t"
            << block.LLY() + n_well_shape->URY() << "\t"

            << block.LLX() + p_well_shape->LLX() << "\t"
            << block.LLX() + p_well_shape->URX() << "\t"
            << block.LLX() + p_well_shape->URX() << "\t"
            << block.LLX() + p_well_shape->LLX() << "\t"
            << block.LLY() + p_well_shape->LLY() << "\t"
            << block.LLY() + p_well_shape->LLY() << "\t"
            << block.LLY() + p_well_shape->URY() << "\t"
            << block.LLY() + p_well_shape->URY() << "\n";
      } else if (block.Orient() == FS_) {
        ost << block.LLX() + n_well_shape->LLX() << "\t"
            << block.LLX() + n_well_shape->URX() << "\t"
            << block.LLX() + n_well_shape->URX() << "\t"
            << block.LLX() + n_well_shape->LLX() << "\t"
            << block.URY() - n_well_shape->LLY() << "\t"
            << block.URY() - n_well_shape->LLY() << "\t"
            << block.URY() - n_well_shape->URY() << "\t"
            << block.URY() - n_well_shape->URY() << "\t"

            << block.LLX() + p_well_shape->LLX() << "\t"
            << block.LLX() + p_well_shape->URX() << "\t"
            << block.LLX() + p_well_shape->URX() << "\t"
            << block.LLX() + p_well_shape->LLX() << "\t"
            << block.URY() - p_well_shape->LLY() << "\t"
            << block.URY() - p_well_shape->LLY() << "\t"
            << block.URY() - p_well_shape->URY() << "\t"
            << block.URY() - p_well_shape->URY() << "\n";
      }
    }
  }

  ost.close();
}

void Circuit::SaveDefFile(std::string const &name_of_file, std::string const &def_file_name, bool is_complete_version) {
  std::string file_name;
  if (is_complete_version) {
    file_name = name_of_file + ".def";
  } else {
    file_name = name_of_file + "_trim.def";
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    if (is_complete_version) {
      printf("Writing DEF file '%s', ", file_name.c_str());
    } else {
      printf("Writing trimmed DEF file (for debugging) '%s', ", file_name.c_str());
    }
  }
  std::ofstream ost(file_name.c_str());
  Assert(ost.is_open(), "Cannot open file " + file_name);

  std::ifstream ist(def_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + def_file_name);

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
  double factor_x = design_.def_distance_microns * tech_.grid_value_x_;
  double factor_y = design_.def_distance_microns * tech_.grid_value_y_;
  if (is_complete_version) {
    ost << "COMPONENTS " << design_.block_list.size() - design_.pre_placed_io_count_ + design_.well_tap_list.size()
        << " ;\n";
  } else {
    ost << "COMPONENTS " << design_.block_list.size() - design_.pre_placed_io_count_ + design_.well_tap_list.size() + 1
        << " ;\n";
    ost << "- "
        << "npwells "
        << name_of_file + "well + "
        << PlaceStatusStr(COVER_) << " "
        << "( "
        << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
        << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
        << " ) "
        << "N"
        << " ;\n";
  }
  for (auto &block: design_.block_list) {
    if (block.Type() == tech_.io_dummy_blk_type_) continue;
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << block.GetPlaceStatusStr() << " "
        << "( "
        << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
        << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
        << " ) "
        << OrientStr(block.Orient())
        << " ;\n";
  }
  for (auto &block: design_.well_tap_list) {
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( "
        << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
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
  ost << "PINS " << design_.iopin_list.size() << " ;\n";
  Assert(!tech_.metal_list.empty(), "Need metal layer info to generate PIN location\n");
  std::string metal_name = *(tech_.metal_list[0].Name());
  int half_width = std::ceil(tech_.metal_list[0].MinHeight() / 2.0 * design_.def_distance_microns);
  int height = std::ceil(tech_.metal_list[0].Width() * design_.def_distance_microns);
  for (auto &iopin: design_.iopin_list) {
    ost << "- "
        << *iopin.Name()
        << " + NET "
        << iopin.GetNet()->NameStr()
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

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void Circuit::SaveDefFile(std::string const &base_name,
                          std::string const &name_padding,
                          std::string const &def_file_name,
                          int save_floorplan,
                          int save_cell,
                          int save_iopin,
                          int save_net) {
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
  std::string file_name = base_name + name_padding + ".def";
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Writing DEF file '%s', ", file_name.c_str());
  }
  std::ofstream ost(file_name.c_str());
  Assert(ost.is_open(), "Cannot open file " + file_name);
  std::ifstream ist(def_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + def_file_name);

  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  ost << "##################################################\n";
  ost << "#  created by: Dali, build time: " << __DATE__ << " " << __TIME__ << "\n";
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
  double factor_x = design_.def_distance_microns * tech_.grid_value_x_;
  double factor_y = design_.def_distance_microns * tech_.grid_value_y_;
  switch (save_cell) {
    case 0: { // no cells are saved
      ost << "COMPONENTS 0 ;\n";
      break;
    }
    case 1: { // save all normal cells, regardless of the placement status
      int cell_count = 0;
      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        ++cell_count;
      }
      cell_count += design_.well_tap_list.size();
      ost << "COMPONENTS " << cell_count << " ;\n";
      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      for (auto &block: design_.well_tap_list) {
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      break;
    }
    case 2: { // save only well tap cells
      int cell_count = design_.well_tap_list.size();
      ost << "COMPONENTS " << cell_count << " ;\n";
      for (auto &block: design_.well_tap_list) {
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      break;
    }
    case 3: { // save all normal cells + dummy cell for well filling
      int cell_count = 0;
      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        ++cell_count;
      }
      cell_count += design_.well_tap_list.size();
      cell_count += 1;
      ost << "COMPONENTS " << cell_count << " ;\n";
      ost << "- "
          << "npwells "
          << base_name + "well + "
          << PlaceStatusStr(COVER_) << " "
          << "( "
          << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
          << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
          << " ) "
          << "N"
          << " ;\n";

      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      for (auto &block: design_.well_tap_list) {
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      break;
    }
    case 4: { // save all normal cells + dummy cell for well filling + dummy cell for n/p-plus filling
      int cell_count = 0;
      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        ++cell_count;
      }
      cell_count += design_.well_tap_list.size();
      cell_count += 2;
      ost << "COMPONENTS " << cell_count << " ;\n";
      ost << "- "
          << "npwells "
          << base_name + "well + "
          << PlaceStatusStr(COVER_) << " "
          << "( "
          << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
          << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
          << " ) "
          << "N"
          << " ;\n";
      ost << "- "
          << "ppnps "
          << base_name + "ppnp + "
          << PlaceStatusStr(COVER_) << " "
          << "( "
          << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
          << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
          << " ) "
          << "N"
          << " ;\n";

      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      for (auto &block: design_.well_tap_list) {
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      break;
    }
    case 5: { // save all placed cells
      int cell_count = 0;
      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        if (block.GetPlaceStatus() == UNPLACED_) continue;
        ++cell_count;
      }
      cell_count += design_.well_tap_list.size();
      ost << "COMPONENTS " << cell_count << " ;\n";
      for (auto &block: design_.block_list) {
        if (block.Type() == tech_.io_dummy_blk_type_) continue;
        if (block.GetPlaceStatus() == UNPLACED_) continue;
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      for (auto &block: design_.well_tap_list) {
        ost << "- "
            << *(block.Name()) << " "
            << *(block.Type()->Name()) << " + "
            << block.GetPlaceStatusStr() << " "
            << "( "
            << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
            << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
            << " ) "
            << OrientStr(block.Orient())
            << " ;\n";
      }
      break;
    }
    default: {
      Assert(false, "Invalid value setting for @param save_cell in Circuit::SaveDefFile()\n");
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
      ost << "PINS " << design_.iopin_list.size() << " ;\n";
      Assert(!tech_.metal_list.empty(), "Need metal layer info to generate PIN location\n");
      std::string metal_name = *(tech_.metal_list[3].Name());
      int half_width = std::ceil(tech_.metal_list[3].MinHeight() / 2.0 * design_.def_distance_microns);
      int height = std::ceil(tech_.metal_list[3].Width() * design_.def_distance_microns);
      for (auto &iopin: design_.iopin_list) {
        ost << "- "
            << *iopin.Name()
            << " + NET "
            << iopin.GetNet()->NameStr()
            << " + DIRECTION " << SignalDirectionStr(iopin.Direction())
            << " + USE " << SignalUseStr(iopin.Use());
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
      break;
    }
    case 2: { // save all IOPINs with status before IO placement
      ost << "PINS " << design_.iopin_list.size() << " ;\n";
      Assert(!tech_.metal_list.empty(), "Need metal layer info to generate PIN location\n");
      std::string metal_name = *(tech_.metal_list[0].Name());
      int half_width = std::ceil(tech_.metal_list[0].MinHeight() / 2.0 * design_.def_distance_microns);
      int height = std::ceil(tech_.metal_list[0].Width() * design_.def_distance_microns);
      for (auto &iopin: design_.iopin_list) {
        ost << "- "
            << *iopin.Name()
            << " + NET "
            << iopin.GetNet()->NameStr()
            << " + DIRECTION " << SignalDirectionStr(iopin.Direction())
            << " + USE " << SignalUseStr(iopin.Use());
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
      break;
    }
    default: {
      Assert(false, "Invalid value setting for @param save_iopin in Circuit::SaveDefFile()\n");
    }
  }
  ost << "END PINS\n\n";

  switch (save_net) {
    case 0: { // no nets are saved
      ost << "NETS 0 ;\n";
      break;
    }
    case 1: { // save all nets
      ost << "NETS " << design_.net_list.size() << " ;\n";
      for (auto &net: design_.net_list) {
        ost << "- " << *(net.Name()) << "\n";
        ost << " ";
        for (auto &pin_pair: net.blk_pin_list) {
          ost << " ( " << *(pin_pair.BlockName()) << " " << *(pin_pair.PinName()) << " ) ";
        }
        ost << "\n" << " ;\n";
      }
      break;
    }
    case 2: { // save nets containing saved cells and IOPINs
      break;
    }
    case 3: {// save power nets for well tap cell
      ost << "\nNETS 2 ;\n";
      // GND
      ost << "- ggnndd\n";
      ost << " ";
      for (auto &block: design_.well_tap_list) {
        ost << " ( " << block.NameStr() << " g0 )";
      }
      ost << "\n" << " ;\n";
      //Vdd
      ost << "- vvdddd\n";
      ost << " ";
      for (auto &block: design_.well_tap_list) {
        ost << " ( " << block.NameStr() << " v0 )";
      }
      ost << "\n" << " ;\n";
      break;
    }
    default: {
      Assert(false, "Invalid value setting for @param save_net in Circuit::SaveDefFile()\n");
    }
  }
  ost << "END NETS\n\n";

  ost << "END DESIGN\n";

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void Circuit::SaveIODefFile(std::string const &name_of_file, std::string const &def_file_name) {
  std::string file_name;
  file_name = name_of_file + "_io.def";
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("Writing IO DEF file '%s', ", file_name.c_str());
  }
  std::ofstream ost(file_name.c_str());
  Assert(ost.is_open(), "Cannot open file " + file_name);

  std::ifstream ist(def_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + def_file_name);

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
  double factor_x = design_.def_distance_microns * tech_.grid_value_x_;
  double factor_y = design_.def_distance_microns * tech_.grid_value_y_;
  ost << "COMPONENTS " << design_.block_list.size() - design_.pre_placed_io_count_ + design_.well_tap_list.size()
      << " ;\n";
  for (auto &block: design_.block_list) {
    if (block.Type() == tech_.io_dummy_blk_type_) continue;
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << block.GetPlaceStatusStr() << " "
        << "( "
        << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
        << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
        << " ) "
        << OrientStr(block.Orient())
        << " ;\n";
  }
  for (auto &block: design_.well_tap_list) {
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( "
        << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
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
  ost << "PINS " << design_.iopin_list.size() << " ;\n";
  Assert(!tech_.metal_list.empty(), "Need metal layer info to generate PIN location\n");
  std::string metal_name = *(tech_.metal_list[0].Name());
  int half_width = std::ceil(tech_.metal_list[0].MinHeight() / 2.0 * design_.def_distance_microns);
  int height = std::ceil(tech_.metal_list[0].Width() * design_.def_distance_microns);
  for (auto &iopin: design_.iopin_list) {
    ost << "- "
        << *iopin.Name()
        << " + NET "
        << iopin.GetNet()->NameStr()
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

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void Circuit::SaveDefWell(std::string const &name_of_file, std::string const &def_file_name, bool is_no_normal_cell) {
  printf("Writing WellTap Network DEF file (for debugging) '%s', ", name_of_file.c_str());
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);

  std::ifstream ist(def_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + def_file_name);

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
  double factor_x = design_.def_distance_microns * tech_.grid_value_x_;
  double factor_y = design_.def_distance_microns * tech_.grid_value_y_;
  if (is_no_normal_cell) {
    ost << "COMPONENTS " << design_.well_tap_list.size() << " ;\n";
  } else {
    ost << "COMPONENTS " << design_.block_list.size() + design_.well_tap_list.size() << " ;\n";
  }
  for (auto &block: design_.well_tap_list) {
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( "
        << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
        << (int) (block.LLY() * factor_y) + design_.die_area_offset_y_
        << " ) "
        << OrientStr(block.Orient())
        << " ;\n";
  }
  if (!is_no_normal_cell) {
    for (auto &block: design_.block_list) {
      if (block.Type() == tech_.io_dummy_blk_type_) continue;
      ost << "- "
          << *block.Name() << " "
          << *(block.Type()->Name()) << " + "
          << block.GetPlaceStatusStr() << " "
          << "( "
          << (int) (block.LLX() * factor_x) + design_.die_area_offset_x_ << " "
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
  for (auto &block: design_.well_tap_list) {
    ost << " ( " << block.NameStr() << " g0 )";
  }
  ost << "\n" << " ;\n";
  //Vdd
  ost << "- vvdddd\n";
  ost << " ";
  for (auto &block: design_.well_tap_list) {
    ost << " ( " << block.NameStr() << " v0 )";
  }
  ost << "\n" << " ;\n";

  ost << "END NETS\n\n";
  ost << "END DESIGN\n";

  ost.close();
  ist.close();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void Circuit::SaveDefPPNPWell(std::string const &name_of_file, std::string const &def_file_name) {
  std::string file_name = name_of_file + "_ppnpwell.def";
  printf("Writing PPNPWell DEF file (for debugging) '%s', ", file_name.c_str());
  std::ofstream ost(file_name.c_str());
  Assert(ost.is_open(), "Cannot open file " + file_name);

  std::ifstream ist(def_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + def_file_name);

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
  double factor_x = design_.def_distance_microns * tech_.grid_value_x_;
  double factor_y = design_.def_distance_microns * tech_.grid_value_y_;
  ost << "COMPONENTS " << 2 << " ;\n";
  ost << "- "
      << "npwells "
      << name_of_file + "well + "
      << PlaceStatusStr(COVER_) << " "
      << "( "
      << (int) (RegionLLX() * factor_x) + design_.die_area_offset_x_ << " "
      << (int) (RegionLLY() * factor_y) + design_.die_area_offset_y_
      << " ) "
      << "N"
      << " ;\n";
  ost << "- "
      << "ppnps "
      << name_of_file + "ppnp + "
      << PlaceStatusStr(COVER_) << " "
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

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("done\n");
  }
}

void Circuit::SaveInstanceDefFile(std::string const &name_of_file, std::string const &def_file_name) {
  SaveDefFile(name_of_file, def_file_name, false);
}

void Circuit::SaveBookshelfNode(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  ost << "NumNodes : \t\t" << design_.tot_mov_blk_num_ << "\n"
      << "NumTerminals : \t\t" << design_.block_list.size() - design_.tot_mov_blk_num_ << "\n";
  for (auto &block: design_.block_list) {
    ost << "\t" << *(block.Name())
        << "\t" << block.Width() * design_.def_distance_microns * tech_.grid_value_x_
        << "\t" << block.Height() * design_.def_distance_microns * tech_.grid_value_y_
        << "\n";
  }
}

void Circuit::SaveBookshelfNet(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  int num_pins = 0;
  for (auto &net: design_.net_list) {
    num_pins += net.blk_pin_list.size();
  }
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  ost << "NumNets : " << design_.net_list.size() << "\n"
      << "NumPins : " << num_pins << "\n\n";
  for (auto &net: design_.net_list) {
    ost << "NetDegree : " << net.blk_pin_list.size() << "   " << *net.Name() << "\n";
    for (auto &pair: net.blk_pin_list) {
      ost << "\t" << *(pair.GetBlock()->Name()) << "\t";
      if (pair.GetPin()->GetIOType()) {
        ost << "I : ";
      } else {
        ost << "O : ";
      }
      ost << (pair.GetPin()->OffsetX() - pair.GetBlock()->Type()->Width() / 2.0) * design_.def_distance_microns
          * tech_.grid_value_x_
          << "\t"
          << (pair.GetPin()->OffsetY() - pair.GetBlock()->Type()->Height() / 2.0) * design_.def_distance_microns
              * tech_.grid_value_y_
          << "\n";
    }
  }
}

void Circuit::SaveBookshelfPl(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  for (auto &node: design_.block_list) {
    ost << *node.Name()
        << "\t"
        << int(node.LLX() * design_.def_distance_microns * tech_.grid_value_x_)
        << "\t"
        << int(node.LLY() * design_.def_distance_microns * tech_.grid_value_y_);
    if (node.IsMovable()) {
      ost << "\t:\tN\n";
    } else {
      ost << "\t:\tN\t/FIXED\n";
    }
  }
  ost.close();
}

void Circuit::SaveBookshelfScl(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
#ifdef USE_OPENDB
  if (db_ == nullptr) {
    std::cout << "During saving bookshelf .scl file. No ROW info has been found!";
    return;
  }
  auto rows = db_->getChip()->getBlock()->getRows();
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
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
}

void Circuit::SaveBookshelfAux(std::string const &name_of_file) {
  std::string aux_name = name_of_file + ".aux";
  std::ofstream ost(aux_name.c_str());
  Assert(ost.is_open(), "Cannot open file " + aux_name);
  ost << "RowBasedPlacement :  "
      << name_of_file << ".nodes  "
      << name_of_file << ".nets  "
      << name_of_file << ".wts  "
      << name_of_file << ".pl  "
      << name_of_file << ".scl";
}

void Circuit::LoadBookshelfPl(std::string const &name_of_file) {
  std::ifstream ist(name_of_file.c_str());
  Assert(ist.is_open(), "Cannot open file " + name_of_file);

  std::string line;
  std::vector<std::string> res;
  double lx = 0;
  double ly = 0;

  while (!ist.eof()) {
    getline(ist, line);
    StrSplit(line, res);
    if (res.size() >= 4) {
      if (IsBlockExist(res[0])) {
        try {
          lx = std::stod(res[1]) / tech_.grid_value_x_ / design_.def_distance_microns;
          ly = std::stod(res[2]) / tech_.grid_value_y_ / design_.def_distance_microns;
          GetBlock(res[0])->SetLoc(lx, ly);
        } catch (...) {
          Assert(false, "Invalid stod conversion:\n\t" + line);
        }
      }
    }
  }
}

void Circuit::StrSplit(std::string &line, std::vector<std::string> &res) {
  static std::vector<char> delimiter_list{' ', ':', ';', '\t', '\r', '\n'};

  res.clear();
  std::string empty_str;
  bool is_delimiter, old_is_delimiter = true;
  int current_field = -1;
  for (auto &c: line) {
    is_delimiter = false;
    for (auto &delimiter: delimiter_list) {
      if (c == delimiter) {
        is_delimiter = true;
        break;
      }
    }
    if (is_delimiter) {
      old_is_delimiter = is_delimiter;
      continue;
    } else {
      if (old_is_delimiter) {
        current_field++;
        res.push_back(empty_str);
      }
      res[current_field] += c;
      old_is_delimiter = is_delimiter;
    }
  }
}

int Circuit::FindFirstDigit(std::string &str) {
  /****
   * this function assumes that the input string is a concatenation of
   * a pure English char string, and a pure digit string
   * like "metal7", "metal12", etc.
   * it will return the location of the first digit
   * ****/

  int res = -1;
  int sz = str.size();
  for (int i = 0; i < sz; ++i) {
    if (str[i] >= '0' && str[i] <= '9') {
      res = i;
      break;
    }
  }
  if (res > 0) {
    for (int i = res + 1; i < sz; ++i) {
      Assert(str[i] >= '0' && str[i] <= '9', "Invalid naming convention: " + str);
    }
  }
  return res;
}
