//
// Created by Yihang Yang on 2019-03-26.
//

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <climits>
#include <algorithm>
#include "circuit.h"

Circuit::Circuit() {
  tot_movable_blk_num_ = 0;
  tot_block_area_ = 0;
  tot_width_ = 0;
  tot_height_ = 0;
  tot_block_area_ = 0;
  tot_mov_width_ = 0;
  tot_mov_height_ = 0;
  tot_mov_block_area_ = 0;
  tot_movable_blk_num_ = 0;
  min_width_ = INT_MAX;
  max_width_ = 0;
  min_height_ = INT_MAX;
  max_height_ = 0;
  grid_set_ = false;
  grid_value_x_ = 0;
  grid_value_y_ = 0;
}

Circuit::~Circuit() {
  /****
   * This destructor free the memory allocated for unordered_map<key, *T>
   * because T is initialized by
   *    auto *T = new T();
   * ****/
  for (auto &&pair: block_type_name_map) {
    delete pair.second;
  }
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
  std::pair<const std::string, int>* name_num_pair_ptr = &(*ret.first);
  metal_list.emplace_back(name_num_pair_ptr, width, spacing);
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

void Circuit::SetBoundaryFromDef(int left, int right, int bottom, int top) {
  Assert(right > left, "Right boundary is not larger than Left boundary?");
  Assert(top > bottom, "Top boundary is not larger than Bottom boundary?");
  def_left = left;
  def_right = right;
  def_bottom = bottom;
  def_top = top;
}

bool Circuit::IsBlockTypeExist(std::string &block_type_name) {
  return !(block_type_name_map.find(block_type_name) == block_type_name_map.end());
}

BlockType *Circuit::GetBlockType(std::string &block_type_name) {
  Assert(IsBlockTypeExist(block_type_name), "BlockType not exist, cannot find it: " + block_type_name);
  return block_type_name_map.find(block_type_name)->second;
}

BlockType *Circuit::AddBlockType(std::string &block_type_name, int width, int height) {
  Assert(!IsBlockTypeExist(block_type_name), "BlockType exist, cannot create this block type again: " + block_type_name);
  auto *block_type_ptr = new BlockType(width, height);
  auto ret = block_type_name_map.insert(std::pair<std::string, BlockType*>(block_type_name, block_type_ptr));
  block_type_ptr->SetName(&ret.first->first);
  return block_type_ptr;
}

bool Circuit::IsBlockExist(std::string &block_name) {
  return !(block_name_map.find(block_name) == block_name_map.end());
}

int Circuit::BlockIndex(std::string &block_name) {
  Assert(IsBlockExist(block_name), "Block Name does not exist, cannot find its index: " + block_name);
  return block_name_map.find(block_name)->second;
}

Block *Circuit::GetBlock(std::string &block_name) {
  return &block_list[BlockIndex(block_name)];
}

void Circuit::AddBlock(std::string &block_name, BlockType *block_type, int llx, int lly, bool movable, BlockOrient orient) {
  PlaceStatus place_status;
  if (movable) {
    place_status = UNPLACED;
  } else {
    place_status = FIXED;
  }
  AddBlock(block_name, block_type, llx, lly, place_status, orient);
}

void Circuit::AddBlock(std::string &block_name, std::string &block_type_name, int llx, int lly, bool movable, BlockOrient orient) {
  BlockType *block_type = GetBlockType(block_type_name);
  AddBlock(block_name, block_type, llx, lly, movable, orient);
}

void Circuit::AddBlock(std::string &block_name, BlockType *block_type, int llx, int lly, PlaceStatus place_status, BlockOrient orient) {
  Assert(net_list.empty(), "Cannot add new Block, because net_list now is not empty");
  Assert(!IsBlockExist(block_name), "Block exists, cannot create this block again: " + block_name);
  int map_size = block_name_map.size();
  auto ret = block_name_map.insert(std::pair<std::string, int>(block_name, map_size));
  std::pair<const std::string, int>* name_num_pair_ptr = &(*ret.first);
  block_list.emplace_back(block_type, name_num_pair_ptr, llx, lly, place_status, orient);

  auto old_tot_area = tot_block_area_;

  tot_block_area_ += block_list.back().Area();
  Assert(old_tot_area < tot_block_area_, "Total Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
  tot_width_ += block_list.back().Width();
  tot_height_ += block_list.back().Height();
  if (block_list.back().IsMovable()) {
    ++tot_movable_blk_num_;
    old_tot_area = tot_mov_block_area_;
    tot_mov_block_area_ += block_list.back().Area();
    Assert(old_tot_area < tot_mov_block_area_, "Total Movable Block Area Overflow, choose a different MANUFACTURINGGRID/unit");
    tot_mov_width_ += block_list.back().Width();
    tot_mov_height_ += block_list.back().Height();
  }
  if ( block_list.back().Height() < min_height_ ) {
    min_height_ = block_list.back().Height();
  }
  if ( block_list.back().Height() > max_height_ ) {
    max_height_ = block_list.back().Height();
  }
  if ( block_list.back().Width() < min_width_ ) {
    min_width_ = block_list.back().Width();
  }
  if ( block_list.back().Width() > min_width_ ) {
    max_width_ = block_list.back().Width();
  }
}

void Circuit::AddBlock(std::string &block_name, std::string &block_type_name, int llx, int lly, PlaceStatus place_status, BlockOrient orient) {
  BlockType *block_type = GetBlockType(block_type_name);
  AddBlock(block_name, block_type, llx, lly, place_status, orient);
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
  std::pair<const std::string, int>* name_num_pair_ptr = &(*net_name_map.find(net_name));
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

}

double Circuit::GridValueX() {
  return grid_value_x_;
}

double Circuit::GridValueY() {
  return grid_value_y_;
}

void Circuit::ReadLefFile(std::string const &name_of_file) {
  /****
  * This is a naive lef parser, it cannot cover all corner cases
  * Please use other APIs to build a circuit if necessary
  * ****/
  std::ifstream ist(name_of_file.c_str());
  Assert(ist.is_open(), "Cannot open input file " + name_of_file);
  std::cout << "loading lef file" << std::endl;
  std::string line;

  // 1. find DATABASE MICRONS
  lef_database_microns = 0;
  while ((lef_database_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DATABASE MICRONS")!=std::string::npos) {
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
      manufacturing_grid = 1.0/lef_database_microns;
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
  if (!grid_set_) {
    SetGridValue(manufacturing_grid, manufacturing_grid);
  }
  //std::cout << "MANUFACTURINGGRID: " << grid_value_ << "\n";

  // 3. read metal layer
  static std::vector<std::string> metal_identifier_list {"m", "M", "metal", "Metal"};
  while (!ist.eof()) {
    if (line.find("LAYER")!= std::string::npos) {
      std::vector<std::string> layer_field;
      StrSplit(line, layer_field);
      Assert(layer_field.size() == 2, "Invalid LAYER, expect only: LAYER layerName\n\tgot: " + line);
      int first_digit_pos = FindFirstDigit(layer_field[1]);
      std::string metal_id(layer_field[1], 0, first_digit_pos);
      if (std::find(metal_identifier_list.begin(), metal_identifier_list.end(), metal_id) != metal_identifier_list.end()) {
        std::string end_layer_flag = "END " + layer_field[1];
        MetalLayer *metal_layer = AddMetalLayer(layer_field[1]);
        do {
          getline(ist, line);
          if (line.find("DIRECTION")!=std::string::npos) {
            std::vector<std::string> direction_field;
            StrSplit(line, direction_field);
            Assert(direction_field.size()>=2, "Invalid DIRECTION\n" + line);
            MetalDirection direction = StrToMetalDirection(direction_field[1]);
            metal_layer->SetDirection(direction);
          }
          if (line.find("AREA")!=std::string::npos) {
            std::vector<std::string> area_field;
            StrSplit(line, area_field);
            Assert(area_field.size()>=2, "Invalid AREA\n" + line);
            try {
              double area = std::stod(area_field[1]);
              metal_layer->SetArea(area);
            } catch (...) {
              Assert(false, "Invalid stod conversion\n" + line);
            }
          }
          if (line.find("WIDTH")!=std::string::npos) {
            std::vector<std::string> width_field;
            StrSplit(line, width_field);
            if (width_field.size()!=2) continue;
            try {
              double width = std::stod(width_field[1]);
              metal_layer->SetWidth(width);
            } catch (...) {
              Assert(false, "Invalid stod conversion:\n" + line);
            }
          }
          if (line.find("SPACING")!=std::string::npos &&
              line.find("SPACINGTABLE")==std::string::npos &&
              line.find("ENDOFLINE")==std::string::npos) {
            std::vector<std::string> spacing_field;
            StrSplit(line, spacing_field);
            Assert(spacing_field.size()>=2, "Invalid SPACING\n" + line);
            try {
              double spacing = std::stod(spacing_field[1]);
              metal_layer->SetSpacing(spacing);
            } catch (...) {
              Assert(false, "Invalid stod conversion:\n" + line);
            }
          }
          if (line.find("PITCH")!=std::string::npos) {
            std::vector<std::string> pitch_field;
            StrSplit(line, pitch_field);
            int pch_sz = pitch_field.size();
            Assert(pch_sz>=2, "Invalid PITCH\n" + line);
            if (pch_sz==2) {
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
        } while (line.find(end_layer_flag)==std::string::npos && !ist.eof());
      }
    }
    getline(ist,line);
    if (line.find("VIA")!=std::string::npos || line.find("MACRO")!=std::string::npos) break;
  }
  //ReportMetalLayers();

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
        while ((width==0) && (height==0) && !ist.eof()) {
          if (line.find("SIZE") != std::string::npos) {
            std::vector<std::string> size_field;
            StrSplit(line, size_field);
            try {
              width = (int)(std::round(std::stod(size_field[1])/grid_value_x_));
              height = (int)(std::round(std::stod(size_field[3])/grid_value_y_));
            } catch (...) {
              Assert(false, "Invalid stod conversion:\n" + line);
            }
            new_block_type = AddBlockType(block_type_name, width, height);
            //std::cout << "  type width, height: " << block_type_list.back().Width() << " " << block_type_list.back().Height() << "\n";
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
          }  while (line.find("PORT")==std::string::npos && !ist.eof());

          double llx = 0, lly = 0, urx = 0, ury = 0;
          do {
            getline(ist, line);
            if (line.find("RECT") != std::string::npos) {
              //std::cout << line << "\n";
              std::vector<std::string> rect_field;
              StrSplit(line, rect_field);
              Assert(rect_field.size() >= 5, "Invalid rect definition: expecting 5 fields\n" + line);
              try {
                llx = std::stod(rect_field[1])/grid_value_x_;
                lly = std::stod(rect_field[2])/grid_value_y_;
                urx = std::stod(rect_field[3])/grid_value_x_;
                ury = std::stod(rect_field[4])/grid_value_y_;
              } catch (...) {
                Assert(false, "Invalid stod conversion:\n" + line);
              }
              //std::cout << LLX << " " << LLY << " " << URX << " " << URY << "\n";
              new_pin->AddRect(llx, lly, urx, ury);
            }
          } while (line.find(end_pin_flag)==std::string::npos && !ist.eof());
          Assert(!new_pin->Empty(), "Pin has no RECTs: " + *new_pin->Name());
        }
      } while (line.find(end_macro_flag)==std::string::npos && !ist.eof());
      Assert(!new_block_type->Empty(), "MACRO has no PINs: " + *new_block_type->Name());
    }
    getline(ist, line);
  }
  std::cout << "lef file loading complete" << std::endl;
}

void Circuit::ReadDefFile(std::string const &name_of_file) {
  /****
   * This is a naive def parser, it cannot cover all corner cases
   * Please use other APIs to build a circuit if this naive def parser cannot satisfy your needs
   * ****/
  std::ifstream ist(name_of_file.c_str());
  Assert(ist.is_open(), "Cannot open input file " + name_of_file);
  std::cout << "loading def file" << std::endl;
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
      if (line.find("COMPONENTS")!=std::string::npos) {
        std::vector<std::string> components_field;
        StrSplit(line, components_field);
        Assert(components_field.size()==2, "Improper use of COMPONENTS?\n" + line);
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
      if (line.find("PINS")!=std::string::npos) {
        std::vector<std::string> pins_field;
        StrSplit(line, pins_field);
        Assert(pins_field.size()==2, "Improper use of PINS?\n" + line);
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
      if (line.find("NETS")!=std::string::npos) {
        std::vector<std::string> nets_field;
        StrSplit(line, nets_field);
        Assert(nets_field.size()==2, "Improper use of NETS?\n" + line);
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
  block_list.reserve(components_count+pins_count);
  net_list.reserve(nets_count);

  // find UNITS DISTANCE MICRONS
  def_distance_microns = 0;
  while ((def_distance_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DISTANCE MICRONS")!=std::string::npos) {
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
  Assert(def_distance_microns>0, "Invalid/null UNITS DISTANCE MICRONS: " + std::to_string(def_distance_microns));
  //std::cout << "DISTANCE MICRONS " << def_distance_microns << "\n";

  // find DIEAREA
  def_left = 0;
  def_right = 0;
  def_bottom = 0;
  def_top = 0;
  while ((def_left == 0) && (def_right == 0) && (def_bottom == 0) && (def_top == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DIEAREA")!=std::string::npos) {
      std::vector<std::string> die_area_field;
      StrSplit(line, die_area_field);
      //std::cout << line << "\n";
      Assert(die_area_field.size() >= 9, "Invalid UNITS declaration: expecting 9 fields");
      try {
        def_left = (int)std::round(std::stoi(die_area_field[2])/grid_value_x_/def_distance_microns);
        def_bottom = (int)std::round(std::stoi(die_area_field[3])/grid_value_y_/def_distance_microns);
        def_right = (int)std::round(std::stoi(die_area_field[6])/grid_value_x_/def_distance_microns);
        def_top = (int)std::round(std::stoi(die_area_field[7])/grid_value_y_/def_distance_microns);
        def_boundary_set = true;
      } catch (...) {
        Assert(false, "Invalid stoi conversion (DIEAREA):\n" + line);
      }
    }
  }
  Warning(!def_boundary_set, "DIEAREA is absent, you need to specify the placement region explicitly before placement");
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
    std::cout << line << "\n";
    // a). find the number of pins
    std::vector<std::string> pins_field;
    StrSplit(line, pins_field);
    std::cout << pins_field[1] << "\n";
    try {
      int pins_cnt = std::stoi(pins_field[1]);
      pin_list.reserve(pins_cnt);
    } catch (...) {
      Assert(false, "Invalid stoi conversion:\n" + line);
    }

    getline(ist, line);

    while ((line.find("END PINS") == std::string::npos) && !ist.eof()) {
      if (line.find('-') != std::string::npos && line.find("NET") != std::string::npos) {
        std::cout << line << "\n";
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
    try {
      int net_list_size = (int) std::round(std::stoi(nets_size_field[1]));
    } catch (...) {
      Assert(false, "Invalid stoi conversion:\n" + line);
    }
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
            Block *block = GetBlock(pin_field[i + 1]);
            int pin_num = block->Type()->PinIndex(pin_field[i + 2]);
            new_net->AddBlockPinPair(block, pin_num);
          }
          //std::cout << "\n";
          if (line.find(";") != std::string::npos) break;
        }
        Assert(!new_net->blk_pin_list.empty(), "Net " + net_field[1] + " has no blk_pin_pair");
        Warning(new_net->blk_pin_list.size() == 1, "Net " + net_field[1] + " has only one blk_pin_pair");
      }
      getline(ist, line);
    }
  }
  std::cout << "def file loading complete\n";
}

void Circuit::ReportBlockTypeList() {
  for (auto &&it: block_type_name_map) {
    it.second->Report();
  }
}

void Circuit::ReportBlockTypeMap() {
  for (auto &&it: block_type_name_map) {
    std::cout << it.first << " " << it.second << "\n";
  }
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
    std::cout << "  movable blocks: " << TotMovableBlockNum() << "\n";
    std::cout << "  blocks: " << TotBlockNum() << "\n";
    std::cout << "  nets: " << net_list.size() << "\n";
    std::cout << "  grid size x: " << grid_value_x_ << " um, grid size y: " << grid_value_y_ << " um\n";
  }
}

int Circuit::MinWidth() const {
  return min_width_;
}

int Circuit::MaxWidth() const {
  return  max_width_;
}

int Circuit::MinHeight() const {
  return min_height_;
}

int Circuit::MaxHeight() const {
  return  max_height_;
}

long int Circuit::TotArea() const {
  return tot_block_area_;
}

int Circuit::TotBlockNum() const {
  return block_list.size();
}

int Circuit::TotMovableBlockNum() const {
  return tot_movable_blk_num_;
}

double Circuit::AveWidth() const {
  return tot_width_/(double)TotBlockNum();
}

double Circuit::AveHeight() const {
  return tot_height_/(double)TotBlockNum();
}

double Circuit::AveArea() const {
  return tot_block_area_/(double)TotBlockNum();
}

double Circuit::AveMovWidth() const {
  return tot_mov_width_/(double)TotMovableBlockNum();
}

double Circuit::AveMovHeight() const {
  return tot_mov_height_/(double)TotMovableBlockNum();
}

double Circuit::AveMovArea() const {
  return tot_mov_block_area_/(double)TotMovableBlockNum();
}

void Circuit::NetSortBlkPin() {
  for (auto &&net: net_list) {
    net.SortBlkPinList();
  }
}

double Circuit::HPWLX() {
  double HPWLX = 0;
  for (auto &&net: net_list) {
    HPWLX += net.HPWLX();
  }
  return HPWLX*GridValueX();
}

double Circuit::HPWLY() {
  double HPWLY = 0;
  for (auto &&net: net_list) {
    HPWLY += net.HPWLY();
  }
  return HPWLY*GridValueY();
}

double Circuit::HPWL() {
  return HPWLX() + HPWLY();
}

void Circuit::ReportHPWL() {
  std::cout << "  Current HPWL: " << std::scientific << HPWLX() + HPWLY() << " um\n";
}

double Circuit::HPWLCtoCX() {
  double HPWLCtoCX = 0;
  for (auto &&net: net_list) {
    HPWLCtoCX += net.HPWLCtoCX();
  }
  return HPWLCtoCX;
}

double Circuit::HPWLCtoCY() {
  double HPWLCtoCY = 0;
  for (auto &&net: net_list) {
    HPWLCtoCY += net.HPWLCtoCY();
  }
  return HPWLCtoCY;
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
        << "( " + std::to_string((int)(block.LLX()*def_distance_microns*grid_value_x_)) + " " + std::to_string((int)(block.LLY()*def_distance_microns*grid_value_y_)) + " )" << " "
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

void Circuit::SaveDefFile(std::string const &name_of_file, std::string const &def_file_name) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);

  std::ifstream ist(def_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + def_file_name);

  std::string line;
  // 1. print file header, copy from def file
  while (line.find("COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist,line);
    ost << line << "\n";
  }

  // 2. print component
  //std::cout << circuit_->blockList.size() << "\n";
  for (auto &&block: block_list) {
    ost << "- "
        << *block.Name() << " "
        << *(block.Type()->Name()) << " + "
        << "PLACED" << " "
        << "( " + std::to_string((int)(block.LLX()*def_distance_microns*grid_value_x_)) + " " + std::to_string((int)(block.LLY()*def_distance_microns*grid_value_y_)) + " )" << " "
        << OrientStr(block.Orient()) + " ;\n";
  }
  ost << "END COMPONENTS\n";
  // jump to the end of components
  while (line.find("END COMPONENTS") == std::string::npos && !ist.eof()) {
    getline(ist,line);
  }

  // 3. print net, copy from def file
  while (!ist.eof()) {
    getline(ist,line);
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

void Circuit::SaveISPD(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file " + name_of_file);
  for (auto &&node: block_list) {
    if (node.IsMovable()) {
      ost << *node.Name() << "\t" << int(node.LLX()*def_distance_microns*grid_value_x_) << "\t" << int(node.LLY()*def_distance_microns*grid_value_y_) << "\t:\tN\n";
    }
    else {
      ost << *node.Name() << "\t" << int(node.LLX()*def_distance_microns*grid_value_x_) << "\t" << int(node.LLY()*def_distance_microns*grid_value_y_) << "\t:\tN\t/FIXED\n";
    }
  }
  ost.close();
}
