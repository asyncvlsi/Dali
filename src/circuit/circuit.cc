//
// Created by Yihang Yang on 2019-03-26.
//

#include "circuit.h"

#include <climits>
#include <cmath>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "status.h"

Circuit::Circuit() : tot_width_(0),
                     tot_height_(0),
                     tot_blk_area_(0),
                     tot_mov_width_(0),
                     tot_mov_height_(0),
                     tot_mov_block_area_(0),
                     tot_mov_blk_num_(0),
                     blk_min_width_(INT_MAX),
                     blk_max_width_(0),
                     blk_min_height_(INT_MAX),
                     blk_max_height_(0),
                     grid_set_(false),
                     grid_value_x_(0),
                     grid_value_y_(0) {
  AddAbsIOPinType();
#ifdef USE_OPENDB
  db_ = nullptr;
#endif
  tech_param_ = nullptr;
  design_ = nullptr;
}

Circuit::~Circuit() {
  /****
   * This destructor free the memory allocated for unordered_map<key, *T>
   * because T is initialized by
   *    auto *T = new T();
   * ****/
  for (auto &&pair: block_type_map) {
    delete pair.second;
  }
  delete tech_param_;
}

#ifdef USE_OPENDB
Circuit::Circuit(odb::dbDatabase *db)
    : tot_width_(0), tot_height_(0), tot_blk_area_(0), tot_mov_width_(0), tot_mov_height_(0),
      tot_mov_block_area_(0), tot_mov_blk_num_(0), blk_min_width_(INT_MAX), blk_max_width_(0),
      blk_min_height_(INT_MAX), blk_max_height_(0), grid_set_(false), grid_value_x_(0), grid_value_y_(0) {
  AddAbsIOPinType();
  db_ = db;
  tech_param_ = nullptr;
  design_ = nullptr;
  InitializeFromDB(db);
}

void Circuit::InitializeFromDB(odb::dbDatabase *db) {
  db_ = db;
  auto tech = db->getTech();
  auto lib = db->getLibs().begin();
  auto chip = db->getChip();
  auto top_level = chip->getBlock();

  // 1. lef database microns
  lef_database_microns = tech->getDbUnitsPerMicron();
  //std::cout << tech->getDbUnitsPerMicron() << "\n";
  //std::cout << tech->getLefUnits() << "\n";
  //std::cout << top_level->getDefUnits() << "\n";

  // 2. manufacturing grid
  if (tech->hasManufacturingGrid()) {
    //std::cout << "Mangrid" << tech->getManufacturingGrid() << "\n";
    manufacturing_grid = tech->getManufacturingGrid() / double(lef_database_microns);
  } else {
    manufacturing_grid = 1.0 / lef_database_microns;
  }

  // 3. find the first and second metal layer pitch
  if (!grid_set_) {
    double grid_value_x = -1, grid_value_y = -1;
    Assert(tech->getRoutingLayerCount() >= 2, "Needs at least one metal layer to find metal pitch");
    for (auto &&layer: tech->getLayers()) {
      //std::cout << layer->getNumber() << "  " << layer->getName() << "  " << layer->getType() << "\n";
      std::string layer_name(layer->getName());
      if (layer_name == "m1" ||
          layer_name == "metal1" ||
          layer_name == "M1" ||
          layer_name == "Metal1" ||
          layer_name == "METAL1") {
        grid_value_y = (layer->getWidth() + layer->getSpacing()) / double(lef_database_microns);
        //std::cout << (layer->getWidth() + layer->getSpacing()) / double(lef_database_microns) << "\n";
      } else if (layer_name == "m2" ||
          layer_name == "metal2" ||
          layer_name == "M2" ||
          layer_name == "Metal2" ||
          layer_name == "METAL2") {
        grid_value_x = (layer->getWidth() + layer->getSpacing()) / double(lef_database_microns);
        //std::cout << (layer->getWidth() + layer->getSpacing()) / double(lef_database_microns) << "\n";
      }
    }
    SetGridValue(grid_value_x, grid_value_y);
  }

  // 4. load all macro, or we say gate type
  //std::cout << lib->getName() << " lib\n";
  double llx = 0, lly = 0, urx = 0, ury = 0;
  unsigned int width = 0, height = 0;
  for (auto &&mac: lib->getMasters()) {
    std::string blk_name(mac->getName());
    width = int(std::round((mac->getWidth() / grid_value_x_ / lef_database_microns)));
    height = int(std::round((mac->getHeight() / grid_value_y_ / lef_database_microns)));
    auto blk_type = AddBlockType(blk_name, width, height);
    //std::cout << mac->getName() << "\n";
    //std::cout << mac->getWidth()/grid_value_x_/lef_database_microns << "  " << mac->getHeight()/grid_value_y_/lef_database_microns << "\n";
    for (auto &&terminal: mac->getMTerms()) {
      std::string pin_name(terminal->getName());
      //std::cout << terminal->getName() << " " << terminal->getMPins().begin()->getGeometry().begin()->xMax()/grid_value_x/lef_database_microns << "\n";
      auto new_pin = blk_type->AddPin(pin_name);
      auto geo_shape = terminal->getMPins().begin()->getGeometry().begin();
      llx = geo_shape->xMin() / grid_value_x_ / lef_database_microns;
      urx = geo_shape->xMax() / grid_value_x_ / lef_database_microns;
      lly = geo_shape->yMin() / grid_value_y_ / lef_database_microns;
      ury = geo_shape->yMax() / grid_value_y_ / lef_database_microns;
      new_pin->AddRect(llx, lly, urx, ury);
    }
  }

  unsigned int components_count = 0, pins_count = 0, nets_count = 0;
  components_count = top_level->getInsts().size();
  pins_count = top_level->getBTerms().size();
  nets_count = top_level->getNets().size();
  block_list.reserve(components_count + pins_count);
  pin_list.reserve(pins_count);
  net_list.reserve(nets_count);

  std::cout << "components count: " << components_count << "\n"
            << "pin count:        " << pins_count << "\n"
            << "nets count:       " << nets_count << "\n";

  //std::cout << db->getChip()->getBlock()->getName() << "\n";
  //std::cout << db->getChip()->getBlock()->getBTerms().size() << "\n";
  // 5. load all gates
  int llx_int = 0, lly_int = 0;
  def_distance_microns = top_level->getDefUnits();
  odb::adsRect die_area;
  top_level->getDieArea(die_area);
  //std::cout << die_area.xMin() << "\n"
  //          << die_area.xMax() << "\n"
  //          << die_area.yMin() << "\n"
  //          << die_area.yMax() << "\n";
  SetDieArea(die_area.xMin(), die_area.xMax(), die_area.yMin(), die_area.yMax());
  for (auto &&blk: top_level->getInsts()) {
    //std::cout << blk->getName() << "  " << blk->getMaster()->getName() << "\n";
    std::string blk_name(blk->getName());
    std::string blk_type_name(blk->getMaster()->getName());
    blk->getLocation(llx_int, lly_int);
    llx_int = (int) std::round(llx_int / grid_value_x_ / def_distance_microns);
    lly_int = (int) std::round(lly_int / grid_value_y_ / def_distance_microns);
    std::string place_status(blk->getPlacementStatus().getString());
    std::string orient(blk->getOrient().getString());
    AddBlock(blk_name, blk_type_name, llx_int, lly_int, StrToPlaceStatus(place_status), StrToOrient(orient));
  }
  /*
  auto res = db->getChip()->getBlock()->findInst("LL_327_acx1");
  int x,y;
  res->getOrigin(x,y);
  std::cout << x << "  " << y << " origin\n";
  res->getLocation(x,y);
  std::cout << x << "  " << y << " location\n";
  std::cout << res->getOrient().getString() << " orient\n";
  std::string orient(res->getOrient().getString());
  std::cout << StrToOrient(orient) << "\n";
  std::cout << res->getPlacementStatus().getString() << " placement status\n";
  std::string place_status(res->getPlacementStatus().getString());
  std::cout << StrToPlaceStatus(place_status) << "\n";
   */

  for (auto &&term: top_level->getBTerms()) {
    //std::cout << term->getName() << "\n";
    std::string pin_name(term->getName());
    AddIOPin(pin_name);
  }

  //std::cout << "Nets:\n";
  for (auto &&net: top_level->getNets()) {
    //std::cout << net->getName() << "\n";
    std::string net_name(net->getName());
    auto new_net = AddNet(net_name, normal_signal_weight);
    for (auto &&bterm: net->getBTerms()) {
      //std::cout << "  ( PIN " << bterm->getName() << ")  \t";
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
  lef_database_microns = 0;
  while ((lef_database_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DATABASE MICRONS") != std::string::npos) {
      std::vector<std::string> line_field;
      StrSplit(line, line_field);
      Assert(line_field.size() >= 3, "Invalid UNITS declaration: expecting 3 fields");
      try {
        lef_database_microns = std::stoi(line_field[2]);
      } catch (...) {
        std::cout << line << "\n";
        Assert(false, "Invalid stoi conversion:" + line_field[2]);
      }
    }
  }
  //std::cout << "DATABASE MICRONS " << lef_database_microns << "\n";

  // 2. find MANUFACTURINGGRID
  manufacturing_grid = 0;
  while ((manufacturing_grid <= 1e-10) && !ist.eof()) {
    getline(ist, line);
    if (line.find("LAYER") != std::string::npos) {
      manufacturing_grid = 1.0 / lef_database_microns;
      std::cout << "  WARNING:\n  MANUFACTURINGGRID not specified explicitly, using 1.0/DATABASE MICRONS instead\n";
    }
    if (line.find("MANUFACTURINGGRID") != std::string::npos) {
      std::vector<std::string> grid_field;
      StrSplit(line, grid_field);
      Assert(grid_field.size() >= 2, "Invalid MANUFACTURINGGRID declaration: expecting 2 fields");
      try {
        manufacturing_grid = std::stod(grid_field[1]);
      } catch (...) {
        Assert(false, "Invalid stod conversion:\n" + line);
      }
      break;
    }
  }
  Assert(manufacturing_grid > 0, "Cannot find or invalid MANUFACTURINGGRID");
  //std::cout << "MANUFACTURINGGRID: " << grid_value_ << "\n";

  // 3. read metal layer
  static std::vector<std::string> metal_identifier_list{"m", "M", "metal", "Metal"};
  while (!ist.eof()) {
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
  if (!grid_set_) {
    if (metal_list.size() < 2) {
      SetGridValue(manufacturing_grid, manufacturing_grid);
      std::cout << "No enough metal layers to specify horizontal and vertical pitch\n"
                << "Using manufacturing grid as grid values\n";
    } else if (metal_list[0].PitchY() <= 0 || metal_list[1].PitchX() <= 0) {
      SetGridValue(manufacturing_grid, manufacturing_grid);
      std::cout << "Invalid metal pitch\n"
                << "Using manufacturing grid as grid values\n";
    } else {
      SetGridUsingMetalPitch();
    }
  }

  // 4. read block type information
  while (!ist.eof()) {
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
        while ((width == 0) && (height == 0) && !ist.eof()) {
          if (line.find("SIZE") != std::string::npos) {
            std::vector<std::string> size_field;
            StrSplit(line, size_field);
            try {
              width = (int) (std::round(std::stod(size_field[1]) / grid_value_x_));
              height = (int) (std::round(std::stod(size_field[3]) / grid_value_y_));
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
          } while (line.find("PORT") == std::string::npos && !ist.eof());

          double llx = 0, lly = 0, urx = 0, ury = 0;
          do {
            getline(ist, line);
            if (line.find("RECT") != std::string::npos) {
              //std::cout << line << "\n";
              std::vector<std::string> rect_field;
              StrSplit(line, rect_field);
              Assert(rect_field.size() >= 5, "Invalid rect definition: expecting 5 fields\n" + line);
              try {
                llx = std::stod(rect_field[1]) / grid_value_x_;
                lly = std::stod(rect_field[2]) / grid_value_y_;
                urx = std::stod(rect_field[3]) / grid_value_x_;
                ury = std::stod(rect_field[4]) / grid_value_y_;
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
      if (line.find("NETS") != std::string::npos) {
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
  block_list.reserve(components_count + pins_count);
  pin_list.reserve(pins_count);
  net_list.reserve(nets_count);

  // find UNITS DISTANCE MICRONS
  def_distance_microns = 0;
  while ((def_distance_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DISTANCE MICRONS") != std::string::npos) {
      std::vector<std::string> line_field;
      StrSplit(line, line_field);
      Assert(line_field.size() >= 4, "Invalid UNITS declaration: expecting 4 fields");
      try {
        def_distance_microns = std::stoi(line_field[3]);
      } catch (...) {
        Assert(false, "Invalid stoi conversion (UNITS DISTANCE MICRONS):\n" + line);
      }
    }
  }
  Assert(def_distance_microns > 0, "Invalid/null UNITS DISTANCE MICRONS: " + std::to_string(def_distance_microns));
  //std::cout << "DISTANCE MICRONS " << def_distance_microns << "\n";

  // find DIEAREA
  def_left = 0;
  def_right = 0;
  def_bottom = 0;
  def_top = 0;
  while ((def_left == 0) && (def_right == 0) && (def_bottom == 0) && (def_top == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DIEAREA") != std::string::npos) {
      std::vector<std::string> die_area_field;
      StrSplit(line, die_area_field);
      //std::cout << line << "\n";
      Assert(die_area_field.size() >= 9, "Invalid UNITS declaration: expecting 9 fields");
      try {
        def_left = (int) std::round(std::stoi(die_area_field[2]) / grid_value_x_ / def_distance_microns);
        def_bottom = (int) std::round(std::stoi(die_area_field[3]) / grid_value_y_ / def_distance_microns);
        def_right = (int) std::round(std::stoi(die_area_field[6]) / grid_value_x_ / def_distance_microns);
        def_top = (int) std::round(std::stoi(die_area_field[7]) / grid_value_y_ / def_distance_microns);
      } catch (...) {
        Assert(false, "Invalid stoi conversion (DIEAREA):\n" + line);
      }
    }
  }
  //std::cout << "DIEAREA ( " << def_left << " " << def_bottom << " ) ( " << def_right << " " << def_top << " )\n";

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
        AddBlock(block_declare_field[1], block_declare_field[2], 0, 0, UNPLACED, N);
      } else if (block_declare_field.size() == 10) {
        PlaceStatus place_status = StrToPlaceStatus(block_declare_field[4]);
        BlockOrient orient = StrToOrient(block_declare_field[9]);
        int llx = 0, lly = 0;
        try {
          llx = (int) std::round(std::stoi(block_declare_field[6]) / grid_value_x_ / def_distance_microns);
          lly = (int) std::round(std::stoi(block_declare_field[7]) / grid_value_y_ / def_distance_microns);
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
        //iopin = AddIOPin(io_pin_field[1]);
        AddIOPin(io_pin_field[1]);
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
      if (line.find('-') != std::string::npos) {
        //std::cout << line << "\n";
        std::vector<std::string> net_field;
        StrSplit(line, net_field);
        Assert(net_field.size() >= 2, "Invalid net declaration, expecting at least: - netName\n" + line);
        //std::cout << "\t" << net_field[0] << " " << net_field[1] << "\n";
        Net *new_net = nullptr;
        if (net_field[1].find("Reset") != std::string::npos) {
          //std::cout << net_field[1] << "\n";
          new_net = AddNet(net_field[1], reset_signal_weight);
        } else {
          new_net = AddNet(net_field[1], normal_signal_weight);
        }
        while (true) {
          getline(ist, line);
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
            Block *block = GetBlock(pin_field[i + 1]);
            auto pin = block->Type()->GetPin(pin_field[i + 2]);
            new_net->AddBlockPinPair(block, pin);
          }
          //std::cout << "\n";
          if (line.find(';') != std::string::npos) break;
        }
        Assert(!new_net->blk_pin_list.empty(), "Net " + net_field[1] + " has no blk_pin_pair");
        //Warning(new_net->blk_pin_list.size() == 1, "Net " + net_field[1] + " has only one blk_pin_pair");
      }
      getline(ist, line);
    }
  }
  std::cout << "DEF file loading complete: " << name_of_file << "\n";
}

bool Circuit::IsMetalLayerExist(std::string &metal_name) {
  return !(metal_name_map.find(metal_name) == metal_name_map.end());
}

int Circuit::MetalLayerIndex(std::string &metal_name) {
  Assert(IsMetalLayerExist(metal_name), "MetalLayer does not exist, cannot find it: " + metal_name);
  return metal_name_map.find(metal_name)->second;
}

MetalLayer *Circuit::GetMetalLayer(std::string &metal_name) {
  Assert(IsMetalLayerExist(metal_name), "MetalLayer does not exist, cannot find it: " + metal_name);
  return &metal_list[MetalLayerIndex(metal_name)];
}

MetalLayer *Circuit::AddMetalLayer(std::string &metal_name, double width, double spacing) {
  Assert(!IsMetalLayerExist(metal_name), "MetalLayer exist, cannot create this MetalLayer again: " + metal_name);
  int map_size = metal_name_map.size();
  auto ret = metal_name_map.insert(std::pair<std::string, int>(metal_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  metal_list.emplace_back(width, spacing, name_num_pair_ptr);
  return &(metal_list.back());
}

MetalLayer *Circuit::AddMetalLayer(std::string &metal_name) {
  return AddMetalLayer(metal_name, 0, 0);
}

void Circuit::ReportMetalLayers() {
  for (auto &&metal_layer: metal_list) {
    metal_layer.Report();
  }
}

void Circuit::SetBoundary(int left, int right, int bottom, int top) {
  Assert(right > left, "Right boundary is not larger than Left boundary?");
  Assert(top > bottom, "Top boundary is not larger than Bottom boundary?");
  def_left = left;
  def_right = right;
  def_bottom = bottom;
  def_top = top;
}

void Circuit::SetDieArea(int lower_x, int upper_x, int lower_y, int upper_y) {
  Assert(grid_value_x_ > 0 && grid_value_y_ > 0, "Need to set positive grid values before setting placement boundary");
  Assert(def_distance_microns > 0, "Need to set def_distance_microns before setting placement boundary");
  SetBoundary((int) std::round(lower_x / grid_value_x_ / def_distance_microns),
              (int) std::round(upper_x / grid_value_x_ / def_distance_microns),
              (int) std::round(lower_y / grid_value_y_ / def_distance_microns),
              (int) std::round(upper_y / grid_value_y_ / def_distance_microns));
}

BlockTypeCluster *Circuit::AddBlockTypeCluster() {
  well_info_.cluster_list_.emplace_back(nullptr, nullptr);
  return &(well_info_.cluster_list_.back());
}

BlockTypeWell *Circuit::AddBlockTypeWell(BlockTypeCluster *cluster, BlockType *blk_type, bool is_plug) {
  well_info_.well_list_.emplace_back(blk_type);
  blk_type->well_ = &(well_info_.well_list_.back());
  blk_type->well_->SetPlug(is_plug);
  blk_type->well_->SetCluster(cluster);
  return blk_type->well_;
}

BlockTypeWell *Circuit::AddBlockTypeWell(BlockTypeCluster *cluster, std::string &blk_type_name, bool is_plug) {
  BlockType *blk_type_ptr = GetBlockType(blk_type_name);
  AddBlockTypeWell(cluster, blk_type_ptr, is_plug);
  return blk_type_ptr->well_;
}

void Circuit::SetNWellParams(double width, double spacing, double op_spacing, double max_plug_dist) {
  if (tech_param_ == nullptr) tech_param_ = new Tech;
  tech_param_->SetNLayer(width, spacing, op_spacing, max_plug_dist);
}

void Circuit::SetPWellParams(double width, double spacing, double op_spacing, double max_plug_dist) {
  if (tech_param_ == nullptr) tech_param_ = new Tech;
  tech_param_->SetPLayer(width, spacing, op_spacing, max_plug_dist);
}

void Circuit::ReportWellShape() {
  for (auto &cluster: well_info_.cluster_list_) {
    cluster.GetPlug()->Report();
    cluster.GetUnplug()->Report();
  }
}

bool Circuit::IsBlockTypeExist(std::string &block_type_name) {
  return !(block_type_map.find(block_type_name) == block_type_map.end());
}

BlockType *Circuit::GetBlockType(std::string &block_type_name) {
  Assert(IsBlockTypeExist(block_type_name), "BlockType not exist, cannot find it: " + block_type_name);
  return block_type_map.find(block_type_name)->second;
}

BlockType *Circuit::AddBlockType(std::string &block_type_name, unsigned int width, unsigned int height) {
  Assert(!IsBlockTypeExist(block_type_name),
         "BlockType exist, cannot create this block type again: " + block_type_name);
  auto ret = block_type_map.insert(std::pair<std::string, BlockType *>(block_type_name, nullptr));
  auto tmp_ptr = new BlockType(&(ret.first->first), width, height);
  ret.first->second = tmp_ptr;
  if (tmp_ptr->Area() > INT_MAX) tmp_ptr->Report();
  return tmp_ptr;
}

void Circuit::ReportBlockType() {
  std::cout << "Total BlockType: " << block_type_map.size() << std::endl;
  for (auto &&pair: block_type_map) {
    pair.second->Report();
  }
}

void Circuit::CopyBlockType(Circuit &circuit) {
  BlockType *blk_type = nullptr;
  BlockType *blk_type_new = nullptr;
  std::string type_name, pin_name;
  for (auto &&item: circuit.block_type_map) {
    blk_type = item.second;
    type_name = *(blk_type->Name());
    if (type_name == "PIN") continue;
    blk_type_new = AddBlockType(type_name, blk_type->Width(), blk_type->Height());
    for (auto &&pin: blk_type->pin_list) {
      pin_name = *(pin.Name());
      blk_type_new->AddPin(pin_name, pin.OffsetX(), pin.OffsetY());
    }
  }
}

bool Circuit::IsBlockExist(std::string &block_name) {
  return !(block_name_map.find(block_name) == block_name_map.end());
}

int Circuit::BlockIndex(std::string &block_name) {
  auto ret = block_name_map.find(block_name);
  if (ret == block_name_map.end()) {
    Assert(false, "Block does not exist, cannot find its index: " + block_name);
  }
  return ret->second;
}

Block *Circuit::GetBlock(std::string &block_name) {
  return &block_list[BlockIndex(block_name)];
}

void Circuit::AddBlock(std::string &block_name,
                       BlockType *block_type,
                       int llx,
                       int lly,
                       bool movable,
                       BlockOrient orient) {
  PlaceStatus place_status;
  if (movable) {
    place_status = UNPLACED;
  } else {
    place_status = FIXED;
  }
  AddBlock(block_name, block_type, llx, lly, place_status, orient);
}

void Circuit::AddBlock(std::string &block_name,
                       std::string &block_type_name,
                       int llx,
                       int lly,
                       bool movable,
                       BlockOrient orient) {
  BlockType *block_type = GetBlockType(block_type_name);
  AddBlock(block_name, block_type, llx, lly, movable, orient);
}

void Circuit::AddBlock(std::string &block_name,
                       BlockType *block_type,
                       int llx,
                       int lly,
                       PlaceStatus place_status,
                       BlockOrient orient) {
  Assert(net_list.empty(), "Cannot add new Block, because net_list now is not empty");
  Assert(!IsBlockExist(block_name), "Block exists, cannot create this block again: " + block_name);
  int map_size = block_name_map.size();
  auto ret = block_name_map.insert(std::pair<std::string, int>(block_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  block_list.emplace_back(block_type, name_num_pair_ptr, llx, lly, place_status, orient);

  // update statistics of blocks
  unsigned long int old_tot_area = tot_blk_area_;
  tot_blk_area_ += block_list.back().Area();
  Assert(old_tot_area < tot_blk_area_, "Total Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
  tot_width_ += block_list.back().Width();
  tot_height_ += block_list.back().Height();
  if (block_list.back().IsMovable()) {
    ++tot_mov_blk_num_;
    old_tot_area = tot_mov_block_area_;
    tot_mov_block_area_ += block_list.back().Area();
    Assert(old_tot_area < tot_mov_block_area_,
           "Total Movable Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
    tot_mov_width_ += block_list.back().Width();
    tot_mov_height_ += block_list.back().Height();
  }
  if (block_list.back().Height() < blk_min_height_) {
    blk_min_height_ = block_list.back().Height();
  }
  if (block_list.back().Height() > blk_max_height_) {
    blk_max_height_ = block_list.back().Height();
  }
  if (block_list.back().Width() < blk_min_width_) {
    blk_min_width_ = block_list.back().Width();
  }
  if (block_list.back().Width() > blk_min_width_) {
    blk_max_width_ = block_list.back().Width();
  }
}

void Circuit::AddBlock(std::string &block_name,
                       std::string &block_type_name,
                       int llx,
                       int lly,
                       PlaceStatus place_status,
                       BlockOrient orient) {
  BlockType *block_type = GetBlockType(block_type_name);
  AddBlock(block_name, block_type, llx, lly, place_status, orient);
}

void Circuit::AddAbsIOPinType() {
  std::string iopin_type_name("PIN");
  auto io_pin_type = AddBlockType(iopin_type_name, 0, 0);
  std::string tmp_pin_name("pin");
  io_pin_type->AddPin(tmp_pin_name, 0, 0);
}

bool Circuit::IsIOPinExist(std::string &iopin_name) {
  return !(pin_name_map.find(iopin_name) == pin_name_map.end());
}

int Circuit::IOPinIndex(std::string &iopin_name) {
  auto ret = pin_name_map.find(iopin_name);
  if (ret == pin_name_map.end()) {
    Assert(false, "IOPIN does not exist, cannot find its index: " + iopin_name);
  }
  return ret->second;
}

IOPin *Circuit::GetIOPin(std::string &iopin_name) {
  return &pin_list[IOPinIndex(iopin_name)];
}

IOPin *Circuit::AddIOPin(std::string &iopin_name) {
  Assert(net_list.empty(), "Cannot add new IOPIN, because net_list now is not empty");
  Assert(!IsIOPinExist(iopin_name), "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int map_size = pin_name_map.size();
  auto ret = pin_name_map.insert(std::pair<std::string, int>(iopin_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  pin_list.emplace_back(name_num_pair_ptr);
  return &(pin_list.back());
}

IOPin *Circuit::AddIOPin(std::string &iopin_name, int lx, int ly) {
  Assert(net_list.empty(), "Cannot add new IOPIN, because net_list now is not empty");
  Assert(!IsIOPinExist(iopin_name), "IOPin exists, cannot create this IOPin again: " + iopin_name);
  int map_size = pin_name_map.size();
  auto ret = pin_name_map.insert(std::pair<std::string, int>(iopin_name, map_size));
  std::pair<const std::string, int> *name_num_pair_ptr = &(*ret.first);
  pin_list.emplace_back(name_num_pair_ptr, lx, ly);
  return &(pin_list.back());
}

void Circuit::ReportIOPin() {
  for (auto &&iopin: pin_list) {
    iopin.Report();
  }
}

bool Circuit::IsNetExist(std::string &net_name) {
  return !(net_name_map.find(net_name) == net_name_map.end());
}

int Circuit::NetIndex(std::string &net_name) {
  Assert(IsNetExist(net_name), "Net does not exist, cannot find its index: " + net_name);
  return net_name_map.find(net_name)->second;
}

Net *Circuit::GetNet(std::string &net_name) {
  return &net_list[NetIndex(net_name)];
}

void Circuit::AddToNetMap(std::string &net_name) {
  int map_size = net_name_map.size();
  net_name_map.insert(std::pair<std::string, int>(net_name, map_size));
}

Net *Circuit::AddNet(std::string &net_name, double weight) {
  Assert(!IsNetExist(net_name), "Net exists, cannot create this net again: " + net_name);
  AddToNetMap(net_name);
  std::pair<const std::string, int> *name_num_pair_ptr = &(*net_name_map.find(net_name));
  net_list.emplace_back(name_num_pair_ptr, weight);
  return &net_list.back();
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
  Assert(!grid_set_, "once set, grid_value cannot be changed!");
  grid_value_x_ = grid_value_x;
  grid_value_y_ = grid_value_y;
  grid_set_ = true;
}

void Circuit::SetGridUsingMetalPitch() {
  SetGridValue(metal_list[0].PitchY(), metal_list[1].PitchX());
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
      std::vector<std::string> well_fields;
      StrSplit(line, well_fields);
      bool is_n_well = (well_fields[1] == "nwell");
      if (!is_n_well) Assert(well_fields[1] == "pwell", "Unknow N/P well type: " + well_fields[1]);
      std::string end_layer_flag = "END " + well_fields[1];
      double width = 0, spacing = 0, op_spacing = 0, max_plug_dist = 0;
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
        } else {}
        getline(ist, line);
      } while (line.find(end_layer_flag) == std::string::npos && !ist.eof());
      if (is_n_well) {
        SetNWellParams(width, spacing, op_spacing, max_plug_dist);
      } else {
        SetPWellParams(width, spacing, op_spacing, max_plug_dist);
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
                lx = int(std::round(std::stod(shape_fields[1]) / grid_value_x_));
                ly = int(std::round(std::stod(shape_fields[2]) / grid_value_y_));
                ux = int(std::round(std::stod(shape_fields[3]) / grid_value_x_));
                uy = int(std::round(std::stod(shape_fields[4]) / grid_value_y_));
              } catch (...) {
                Assert(false, "Invalid stod conversion:\n" + line);
              }
              auto blk_type = GetBlockType(version_fields[1]);
              well->SetWellShape(is_n, lx, ly, ux, uy);
              if (is_n) {
                well->SetNWellShape(0, ly, int(blk_type->Width()), int(blk_type->Height()));
              } else {
                well->SetPWellShape(0, 0, int(blk_type->Width()), uy);
              }
            }
          } while (line.find("END VERSION") == std::string::npos && !ist.eof());
        }
      } while (line.find(end_macro_flag) == std::string::npos && !ist.eof());
      Assert(!cluster->Empty(), "No plug/unplug version provided");
    }
  }
  Assert(tech_param_ != nullptr, "N/P well technology information not found!");
  //tech_param_->Report();
  //ReportWellShape();

  std::cout << "CELL file loading complete: " << name_of_file << "\n";
}

void Circuit::ReportBlockList() {
  for (auto &&block: block_list) {
    block.Report();
  }
}

void Circuit::ReportBlockMap() {
  for (auto &&it: block_name_map) {
    std::cout << it.first << " " << it.second << "\n";
  }
}

void Circuit::ReportNetList() {
  for (auto &&net: net_list) {
    std::cout << *net.Name() << "  " << net.Weight() << "\n";
    for (auto &&block_pin_pair: net.blk_pin_list) {
      std::cout << "\t" << " (" << *(block_pin_pair.BlockName()) << " " << *(block_pin_pair.PinName()) << ") " << "\n";
    }
  }
}

void Circuit::ReportNetMap() {
  for (auto &&it: net_name_map) {
    std::cout << it.first << " " << it.second << "\n";
  }
}

void Circuit::ReportBriefSummary() {
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "  movable blocks: " << TotMovableBlockNum() << "\n"
              << "  blocks: " << TotBlockNum() << "\n"
              << "  nets: " << net_list.size() << "\n"
              << "  grid size x: " << grid_value_x_ << " um, grid size y: " << grid_value_y_ << " um\n"
              << "  total block area: " << tot_blk_area_ << "\n"
              << "  total white space: " << (unsigned long int) (def_right - def_left) * (def_top - def_bottom) << "\n"
              << "    left:   " << def_left << "\n"
              << "    right:  " << def_right << "\n"
              << "    bottom: " << def_bottom << "\n"
              << "    top:    " << def_top << "\n"
              << "  white space utility: " << WhiteSpaceUsage() << "\n";
  }
}

void Circuit::NetSortBlkPin() {
  for (auto &&net: net_list) {
    net.SortBlkPinList();
  }
}

double Circuit::HPWLX() {
  double hpwlx = 0;
  for (auto &&net: net_list) {
    hpwlx += net.HPWLX();
  }
  return hpwlx * GetGridValueX();
}

double Circuit::HPWLY() {
  double hpwly = 0;
  for (auto &&net: net_list) {
    hpwly += net.HPWLY();
  }
  return hpwly * GetGridValueY();
}

double Circuit::HPWL() {
  return HPWLX() + HPWLY();
}

void Circuit::ReportHPWL() {
  std::cout << "  Current HPWL: " << std::scientific << HPWLX() + HPWLY() << " um\n";
}

double Circuit::HPWLCtoCX() {
  double hpwl_c2c_x = 0;
  for (auto &&net: net_list) {
    hpwl_c2c_x += net.HPWLCtoCX();
  }
  return hpwl_c2c_x * GetGridValueX();
}

double Circuit::HPWLCtoCY() {
  double hpwl_c2c_y = 0;
  for (auto &&net: net_list) {
    hpwl_c2c_y += net.HPWLCtoCY();
  }
  return hpwl_c2c_y * GetGridValueY();
}

double Circuit::HPWLCtoC() {
  return HPWLCtoCX() + HPWLCtoCY();
}

void Circuit::ReportHPWLCtoC() {
  std::cout << "HPWLCtoC: " << HPWLCtoC() << "\n";
}

void Circuit::WriteDefFileDebug(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);

  // need some header here


  for (auto &&block: block_list) {
    ost << "- "
        << *(block.Name()) << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( " + std::to_string((int) (block.LLX() * def_distance_microns * grid_value_x_)) + " "
            + std::to_string((int) (block.LLY() * def_distance_microns * grid_value_y_)) + " )" << " "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";

  ost << "NETS " << net_list.size() << " ;\n";
  for (auto &&net: net_list) {
    ost << "- "
        << *(net.Name()) << "\n";
    ost << " ";
    for (auto &&pin_pair: net.blk_pin_list) {
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
  for (auto &&block: block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
  }
  /*
  for (auto &&net: net_list) {
    for (size_t i=0; i<net.pin_list.size(); i++) {
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        ost << "line([" << net.pin_list[i].abs_x() << "," << net.pin_list[j].abs_x() << "],[" << net.pin_list[i].abs_y() << "," << net.pin_list[j].abs_y() << "],'lineWidth', 0.5)\n";
      }
    }
  }
   */
  ost.close();
}

void Circuit::GenMATLABTable(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << Left() << "\t" << Right() << "\t" << Right() << "\t" << Left() << "\t" << Bottom() << "\t" << Bottom() << "\t"
      << Top() << "\t" << Top() << "\n";
  for (auto &block: block_list) {
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\n";
  }

}

void Circuit::GenMATLABWellTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  std::string unplug_file = name_of_file + "_unplug.txt";
  std::string plug_file = name_of_file + "_plug.txt";
  GenMATLABTable(frame_file);

  std::ofstream ost(unplug_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + unplug_file);
  std::ofstream ost1(plug_file.c_str());
  Assert(ost1.is_open(), "Cannot open output file: " + plug_file);

  BlockTypeWell *well;
  RectI *n_well_shape, *p_well_shape;
  for (auto &block: block_list) {
    well = block.Type()->GetWell();
    if (well != nullptr) {
      n_well_shape = well->GetNWellShape();
      p_well_shape = well->GetPWellShape();
      if (!well->IsPlug()) {
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
      } else {
        ost1
            << block.LLX() + n_well_shape->LLX() << "\t"
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
      }
    }
  }

  ost.close();
  ost1.close();
}

void Circuit::SaveDefFile(std::string const &name_of_file, std::string const &def_file_name) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);

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
  ost << "COMPONENTS " << block_list.size() << " ;\n";
  for (auto &&block: block_list) {
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( " + std::to_string((int) (block.LLX() * def_distance_microns * grid_value_x_)) + " "
            + std::to_string((int) (block.LLY() * def_distance_microns * grid_value_y_)) + " )" << " "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";
  // jump to the end of components
  while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist, line);
  }

  // 3. print net, copy from def file
  while (!ist.eof()) {
    getline(ist, line);
    ost << line << "\n";
  }
  /*
  ost << "NETS " << netList.size() << " ;\n";
  for (auto &&net: netList) {
    ost << "- "
        << net.name() << "\n";
    ost << " ";
    for (auto &&pin: net.pin_list) {
      ost << " " << pin.pin_name();
    }
    ost << "\n" << " ;\n";
  }
  ost << "END NETS\n\n";
  ost << "END DESIGN\n";
   */
  ost.close();
  ist.close();
}

void Circuit::SaveBookshelfNode(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  ost << "NumNodes : \t\t" << tot_mov_blk_num_ << "\n"
      << "NumTerminals : \t\t" << block_list.size() - tot_mov_blk_num_ << "\n";
  for (auto &block: block_list) {
    ost << "\t" << *(block.Name())
        << "\t" << block.Width() * def_distance_microns * grid_value_x_
        << "\t" << block.Height() * def_distance_microns * grid_value_y_
        << "\n";
  }
}

void Circuit::SaveBookshelfNet(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  unsigned int num_pins = 0;
  for (auto &net: net_list) {
    num_pins += net.blk_pin_list.size();
  }
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  ost << "NumNets : " << net_list.size() << "\n"
      << "NumPins : " << num_pins << "\n\n";
  for (auto &net: net_list) {
    ost << "NetDegree : " << net.blk_pin_list.size() << "   " << *net.Name() << "\n";
    for (auto &pair: net.blk_pin_list) {
      ost << "\t" << *(pair.GetBlock()->Name()) << "\t";
      if (pair.GetPin()->GetIOType()) {
        ost << "I : ";
      } else {
        ost << "O : ";
      }
      ost << (pair.GetPin()->OffsetX() - pair.GetBlock()->Type()->Width() / 2.0) * def_distance_microns * grid_value_x_
          << "\t"
          << (pair.GetPin()->OffsetY() - pair.GetBlock()->Type()->Height() / 2.0) * def_distance_microns * grid_value_y_
          << "\n";
    }
  }
}

void Circuit::SaveBookshelfPl(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  ost << "# this line is here just for ntuplace to recognize this file \n\n";
  for (auto &&node: block_list) {
    ost << *node.Name()
        << "\t"
        << int(node.LLX() * def_distance_microns * grid_value_x_)
        << "\t"
        << int(node.LLY() * def_distance_microns * grid_value_y_);
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
          lx = std::stod(res[1]) / grid_value_x_ / def_distance_microns;
          ly = std::stod(res[2]) / grid_value_y_ / def_distance_microns;
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
  for (auto &&c: line) {
    is_delimiter = false;
    for (auto &&delimiter: delimiter_list) {
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
