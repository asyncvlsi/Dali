//
// Created by Yihang Yang on 2019-03-26.
//

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include "circuit.h"

Circuit::Circuit() {
  tot_movable_num_ = 0;
  tot_block_area_ = 0;
  tot_width_ = 0;
  tot_height_ = 0;
  tot_block_area_ = 0;
  tot_movable_num_ = 0;
  min_width_ = 100000000;
  max_width_ = 0;
  min_height_ = 100000000;
  max_height_ = 0;
}

Circuit::Circuit(int tot_block_type_num, int tot_block_num, int tot_net_num) {
  block_type_list.reserve(tot_block_type_num);
  block_list.reserve(tot_block_num);
  net_list.reserve(tot_net_num);
  tot_movable_num_ = 0;
  tot_block_area_ = 0;
  tot_width_ = 0;
  tot_height_ = 0;
  tot_block_area_ = 0;
  tot_movable_num_ = 0;
  min_width_ = 100000000;
  max_width_ = 0;
  min_height_ = 100000000;
  max_height_ = 0;
}

void Circuit::SetBoundary(int left, int right, int bottom, int top) {
  Assert(right > left, "right boundary is not larger than left boundary?");
  Assert(top > bottom, "top boundary is not larger than bottom boundary?");
  def_left = left;
  def_right = right;
  def_bottom = bottom;
  def_top = top;
}

bool Circuit::IsBlockTypeExist(std::string &block_type_name) {
  return !(block_type_name_map.find(block_type_name) == block_type_name_map.end());
}

BlockType *Circuit::GetBlockType(std::string &block_type_name) {
  Assert(IsBlockTypeExist(block_type_name), "BlockType not exist, cannot find its index: " + block_type_name);
  return &block_type_list[block_type_name_map.find(block_type_name)->second];
}

void Circuit::AddToBlockTypeMap(std::string &block_type_name) {
  int map_size = block_type_name_map.size();
  block_type_name_map.insert(std::pair<std::string, int>(block_type_name, map_size));
}

BlockType *Circuit::AddBlockType(std::string &block_type_name, int width, int height) {
  Assert(block_list.empty(), "Cannot add new BlockType, because block_list is not empty");
  Assert(net_list.empty(), "Cannot add new BlockType, because net_list is not empty");
  Assert(!IsBlockTypeExist(block_type_name), "BlockType exist, cannot create this block type again: " + block_type_name);
  AddToBlockTypeMap(block_type_name);
  std::pair<const std::string, int>* name_num_pair_ptr = &(*block_type_name_map.find(block_type_name));
  block_type_list.emplace_back(name_num_pair_ptr, width, height);
  return &block_type_list.back();
}

bool Circuit::IsBlockExist(std::string &block_name) {
  return !(block_name_map.find(block_name) == block_name_map.end());
}

Block *Circuit::GetBlock(std::string &block_name) {
  Assert(IsBlockExist(block_name), "Block Name does not exist, cannot find its index: " + block_name);
  return &block_list[block_name_map.find(block_name)->second];
}

void Circuit::AddToBlockMap(std::string &block_name) {
  int map_size = block_name_map.size();
  block_name_map.insert(std::pair<std::string, int>(block_name, map_size));
}

void Circuit::AddBlock(std::string &block_name, std::string &block_type_name, int llx, int lly, bool movable, BlockOrient orient) {
  Assert(net_list.empty(), "Cannot add new Block, because net_list is not empty");
  Assert(!IsBlockExist(block_name), "Block exists, cannot create this block again: " + block_name);
  BlockType *block_type = GetBlockType(block_type_name);
  block_list.emplace_back(block_name, block_type, llx, lly, movable, orient);
  AddToBlockMap(block_name);
}

bool Circuit::IsNetExist(std::string &net_name) {
  return !(net_name_map.find(net_name) == net_name_map.end());
}

Net *Circuit::GetIndex(std::string &net_name) {
  Assert(IsNetExist(net_name), "Net does not exist, cannot find its index: " + net_name);
  return &net_list[net_name_map.find(net_name)->second];
}

void Circuit::AddToNetMap(std::string &net_name) {
  int map_size = net_name_map.size();
  net_name_map.insert(std::pair<std::string, int>(net_name, map_size));
}

Net *Circuit::AddNet(std::string &net_name, double weight) {
  Assert(!IsNetExist(net_name), "Net exists, cannot create this net again: " + net_name);
  net_list.emplace_back(net_name, weight);
  AddToNetMap(net_name);
  return &net_list.back();
}

void Circuit::add_block_type(std::string &block_type_name, int width, int height) {
  AddBlockType(block_type_name, width, height);
}

void Circuit::add_pin_to_block(std::string &block_type_name, std::string &pin_name, int x_offset, int y_offset) {
  BlockType *block_type_ptr = GetBlockType(block_type_name);
  block_type_ptr->AddPin(pin_name, x_offset, y_offset);
}

void Circuit::add_new_block(std::string &block_name, std::string &block_type_name, int llx, int lly, bool movable, BlockOrient orient) {
  AddBlock(block_name, block_type_name, llx, lly, movable, orient);
}

void Circuit::create_blank_net(std::string &net_name, double weight) {
  AddNet(net_name, weight);
}

void Circuit::add_pin_to_net(std::string &net_name, std::string &block_name, std::string &pin_name) {
  Net *net_ptr = GetIndex(net_name);
  Block *block_ptr = GetBlock(block_name);
  BlockType *block_type_ptr = block_ptr->Type();
  int pin_num = block_type_ptr->PinIndex(pin_name);
  net_ptr->AddBlockPinPair(block_ptr, pin_num);
}


void Circuit::ParseLine(std::string &line, std::vector<std::string> &field_list) {
  std::vector<char> delimiter_list;
  delimiter_list.push_back(' ');
  delimiter_list.push_back(':');
  delimiter_list.push_back('\t');
  delimiter_list.push_back('\r');
  delimiter_list.push_back('\n');

  field_list.clear();
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
        field_list.push_back(empty_str);
      }
      field_list[current_field] += c;
      old_is_delimiter = is_delimiter;
    }
  }
}

void Circuit::ReadLefFile(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    exit(1);
  }
  std::cout << "Start reading lef file\n";

  std::string line;
  lef_database_microns = 0;
  while ((lef_database_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DATABASE MICRONS")!=std::string::npos) {
      std::vector<std::string> line_field;
      ParseLine(line, line_field);
      if (line_field.size() < 3) {
        std::cout << "Error!\n";
        std::cout << "Invalid UNITS declaration: expecting 3 fields\n";
        exit(1);
      }
      try {
        lef_database_microns = std::stoi(line_field[2]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion:" << line_field[2] << "\n";
        std::cout << line << "\n";
        exit(1);
      }
    }
  }
  //std::cout << "DATABASE MICRONS " << lef_database_microns << "\n";

  m2_pitch = 0;
  while ((m2_pitch == 0) && !ist.eof()) {
    getline(ist, line);
    if((line.find("LAYER Metal2")!=std::string::npos) || (line.find("LAYER m2")!=std::string::npos)) {
      //std::cout << line << "\n";
      do {
        getline(ist, line);
        //std::cout << line << "\n";
        if (line.find("PITCH") != std::string::npos) {
          std::vector<std::string> line_field;
          ParseLine(line, line_field);
          if (line_field.size() < 3) {
            std::cout << "Error!\n";
            std::cout << "Invalid LAYER Metal2 PITCH declaration: expecting 3 fields\n";
            exit(1);
          }
          try {
            m2_pitch = std::stod(line_field[2]);
            //std::cout << line_field[2] << "\n";
          } catch (...) {
            std::cout << "Error!\n";
            std::cout << "Invalid stoi conversion:" << line_field[2] << "\n";
            std::cout << line << "\n";
            exit(1);
          }
        }
      } while (line.find("END Metal2")==std::string::npos && line.find("END m2")==std::string::npos && !ist.eof());
      break;
    }
  }
  if (m2_pitch == 0) {
    std::cout << "Error!\n";
    std::cout << "Cannot find Metal2/m2 PITCH\n";
    exit(1);
  }
  //std::cout << "Metal2 PITCH: " << m2_pitch << "\n";

  while (!ist.eof()) {
    getline(ist, line);
    if (line.find("MACRO") != std::string::npos) {
      std::vector<std::string> line_field;
      ParseLine(line, line_field);
      if (line_field.size() < 2) {
        std::cout << "Error!\n";
        std::cout << "Invalid type name: expecting 2 fields\n";
        std::cout << line << "\n";
        exit(1);
      }
      std::string blockTypeName = line_field[1];
      //std::cout << blockTypeName << "\n";
      int width = 0, height = 0;
      std::string end_macro_flag = "END " + line_field[1];
      do {
        getline(ist, line);
        while ((width==0) && (height==0) && !ist.eof()) {
          if (line.find("SIZE") != std::string::npos) {
            std::vector<std::string> size_field;
            ParseLine(line, size_field);
            try {
              width = std::stoi(size_field[1]);
            } catch (...) {
              std::cout << "Error!\n";
              std::cout << "Invalid stod conversion:" << size_field[1] << "\n";
              std::cout << line << "\n";
              exit(1);
            }
            try {
              height = std::stoi(size_field[3]);
            } catch (...) {
              std::cout << "Error!\n";
              std::cout << "Invalid stod conversion:" << size_field[3] << "\n";
              std::cout << line << "\n";
              exit(1);
            }
            width = (int)(std::round(width/m2_pitch));
            height = (int)(std::round(height/m2_pitch));
            add_block_type(blockTypeName, width, height);
            //std::cout << "  type width, height: "
            //          << blockTypeList.back().width() << " "
            //          << blockTypeList.back().height() << "\n";
          }
          getline(ist, line);
        }

        if (line.find("PIN") != std::string::npos) {
          std::vector<std::string> pin_field;
          ParseLine(line, pin_field);
          if (pin_field.size() < 2) {
            std::cout << "Error!\n";
            std::cout << "Invalid pin name: expecting 2 fields\n";
            std::cout << line << "\n";
            exit(1);
          }
          std::string pinName = pin_field[1];
          std::string end_pin_flag = "END " + pinName;

          // skip to "PORT" rectangle list
          do {
            getline(ist, line);
          }  while (line.find("PORT")==std::string::npos && !ist.eof());

          int tmp_xoffset = -1, tmp_yoffset = -1;
          do {
            getline(ist, line);
            if (line.find("RECT") != std::string::npos) {
              //std::cout << line << "\n";
              if ((tmp_xoffset == -1) && (tmp_yoffset == -1)) {
                std::vector<std::string> rect_field;
                ParseLine(line, rect_field);
                if (rect_field.size() < 5) {
                  std::cout << "Error!\n";
                  std::cout << "Invalid rect definition: expecting 5 fields\n";
                  std::cout << line << "\n";
                  exit(1);
                }
                tmp_xoffset = (int)(std::round(std::stod(rect_field[1])/m2_pitch));
                tmp_yoffset = (int)(std::round(std::stod(rect_field[4])/m2_pitch));
                //std::cout << "  " << tmp_xoffset << " " << tmp_yoffset << "\n";
                add_pin_to_block(blockTypeName, pinName, tmp_xoffset, tmp_yoffset);
              } else {
                continue;
              }
            }
          } while (line.find(end_pin_flag)==std::string::npos && !ist.eof());
        }
      } while (line.find(end_macro_flag)==std::string::npos && !ist.eof());
      if (block_type_list.back().pin_list.empty()) {
        std::cout << "Error!\n";
        std::cout << "MACRO: " << *block_type_list.back().Name() << " has no pin!\n";
        exit(1);
      }
    }
  }
  std::cout << "lef file reading complete\n";
}

void Circuit::ReadDefFile(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    exit(1);
  }

  std::cout << "Start reading def file\n";

  std::string line;
  def_distance_microns = 0;
  while ((def_distance_microns == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DISTANCE MICRONS")!=std::string::npos) {
      std::vector<std::string> line_field;
      ParseLine(line, line_field);
      if (line_field.size() < 4) {
        std::cout << "Error!\n";
        std::cout << "Invalid UNITS declaration: expecting 4 fields\n";
        exit(1);
      }
      try {
        def_distance_microns = std::stoi(line_field[3]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion:" << line_field[3] << "\n";
        std::cout << line << "\n";
        exit(1);
      }
    }
  }
  //std::cout << "DISTANCE MICRONS " << def_distance_microns << "\n";

  def_left = 0;
  def_right = 0;
  def_bottom = 0;
  def_top = 0;
  while ((def_left == 0) && (def_right == 0) && (def_bottom == 0) && (def_top == 0) && !ist.eof()) {
    getline(ist, line);
    if (line.find("DIEAREA")!=std::string::npos) {
      std::vector<std::string> die_area_field;
      ParseLine(line, die_area_field);
      //std::cout << line << "\n";
      if (die_area_field.size() < 9) {
        std::cout << "Error!\n";
        std::cout << "Invalid UNITS declaration: expecting 9 fields\n";
        exit(1);
      }
      try {
        def_left = (int)std::round(std::stoi(die_area_field[2])/m2_pitch/def_distance_microns);
        def_bottom = (int)std::round(std::stoi(die_area_field[3])/m2_pitch/def_distance_microns);
        def_right = (int)std::round(std::stoi(die_area_field[6])/m2_pitch/def_distance_microns);
        def_top = (int)std::round(std::stoi(die_area_field[7])/m2_pitch/def_distance_microns);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion: expecting DIEAREA ( XXX XXX ) ( XXX XXX ) ;\n";
        std::cout << line << "\n";
        exit(1);
      }
    }
  }
  //std::cout << "DIEAREA ( " << def_left << " " << def_bottom << " ) ( " << def_right << " " << def_top << " )\n";

  while ((line.find("COMPONENTS") == std::string::npos) && !ist.eof()) {
    getline(ist, line);
  }
  //std::cout << line << "\n";
  getline(ist, line);
  while ((line.find("END COMPONENTS") == std::string::npos) && !ist.eof()) {
    //std::cout << line << "\t";
    std::vector<std::string> block_declare_field;
    ParseLine(line, block_declare_field);
    if (block_declare_field.empty()) {
      continue;
    } else if (block_declare_field.size() < 3) {
      std::cout << "Error!\n";
      std::cout << "Invalid block declaration, expecting at least: - compName modelName\n";
      exit(1);
    }
    //std::cout << block_declare_field[0] << " " << block_declare_field[1] << "\n";
    if (block_type_name_map.find(block_declare_field[2]) == block_type_name_map.end()) {
      std::cout << "Error!\n";
      std::cout << "Invalid block declaration, no such type exists\n";
      std::cout << line << "\n";
      exit(1);
    } else {
      BlockType *block_type = &block_type_list[block_type_name_map.find(block_declare_field[2])->second];
      std::string blockName = *block_type->Name();
      add_new_block(block_declare_field[1], blockName, 0, 0, true);
    }
    getline(ist, line);
  }

  while (line.find("NETS") == std::string::npos && !ist.eof()) {
    getline(ist, line);
  }
  //std::cout << line << "\n";
  getline(ist, line);
  // the following is a hack now, cannot handle all cases, probably need to use BISON in the future if necessary
  while ((line.find("END NETS") == std::string::npos) && !ist.eof()) {
    if (line.find("-") != std::string::npos) {
      //std::cout << line << "\n";
      std::vector<std::string> net_field;
      ParseLine(line, net_field);
      if (net_field.size() < 2) {
        std::cout << "Error!\n";
        std::cout << "Invalid net declaration, expecting at least: - netName\n";
        exit(1);
      }
      //std::cout << "\t" << net_field[0] << " " << net_field[1] << "\n";
      if (net_field[1].find("Reset") != std::string::npos) {
        //std::cout << net_field[1] << "\n";
        create_blank_net(net_field[1], reset_signal_weight);
      } else {
        create_blank_net(net_field[1], normal_signal_weight);
      }
      getline(ist, line);
      //std::cout << line << "\n";
      std::vector<std::string> pin_field;
      ParseLine(line, pin_field);
      if ((pin_field.size()%4 != 0) || (pin_field.size() < 8)) {
        std::cout << "Error!\n";
        std::cout << "Invalid net declaration, expecting 4n fields, where n >= 2\n";
        exit(1);
      }
      for (size_t i=0; i<pin_field.size(); i += 4) {
        //std::cout << "     " << pin_field[i+1] << " " << pin_field[i+2];
        add_pin_to_net(net_field[1], pin_field[i+1], pin_field[i+2]);
      }
      //std::cout << "\n";
    }
    getline(ist, line);
  }
  std::cout << "def file reading complete\n";
}

void Circuit::ReportBlockTypeList() {

}

void Circuit::ReportBlockTypeMap() {

}

void Circuit::ReportBlockList() {
  for (auto &&block: block_list) {
    std::cout << block << "\n";
  }
}

void Circuit::ReportBlockMap() {
  for (auto &&name_num_pair: block_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}



void Circuit::ReportNetList() {
  for (auto &&net: net_list) {
    std::cout << net << "\n";
  }
}

void Circuit::ReportNetMap() {
  for (auto &&name_num_pair: net_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

void Circuit::WriteDefFileDebug(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    exit(1);
  }

  for (auto &&block: block_list) {
    ost << "\t" << block.Name() << "\t" << block.Width() << "\t" << block.Height() << "\n";
  }
}

void Circuit::GenMATLABScript(std::string const &filename) {

}

void Circuit::SaveDefFile(std::string const &NameOfFile, std::string const &defFileName) {

}
