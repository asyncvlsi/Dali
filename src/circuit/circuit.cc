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
}

Circuit::Circuit(int tot_block_type_num, int tot_block_num, int tot_net_num) {
  block_type_list.reserve(tot_block_type_num);
  block_list.reserve(tot_block_num);
  net_list.reserve(tot_net_num);
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

int Circuit::BlockTypeIndex(std::string &block_type_name) {
  Assert(IsBlockTypeExist(block_type_name), "BlockType not exist, cannot find its index: " + block_type_name);
  return block_type_name_map.find(block_type_name)->second;
}

void Circuit::AddToBlockTypeMap(std::string &block_type_name) {
  int map_size = block_type_name_map.size();
  block_type_name_map.insert(std::pair<std::string, int>(block_type_name, map_size));
}

BlockType *Circuit::AddBlockType(std::string &block_type_name, int width, int height) {
  Assert(!IsBlockTypeExist(block_type_name), "BlockType exist, cannot create this block type again: " + block_type_name);
  int list_size = block_type_list.size();
  block_type_list.resize(list_size + 1);
  AddToBlockTypeMap(block_type_name);
  return &block_type_list.back();
}

bool Circuit::IsBlockInstExist(std::string &block_name) {
  return !(block_name_map.find(block_name) == block_name_map.end());
}

int Circuit::BlockInstIndex(std::string &block_name) {
  Assert(IsBlockInstExist(block_name), "Block Name does not exist, cannot find its index: " + block_name);
  return block_name_map.find(block_name)->second;
}

void Circuit::AddToBlockMap(std::string &block_name) {
  int map_size = block_name_map.size();
  block_name_map.insert(std::pair<std::string, int>(block_name, map_size));
}

void Circuit::AddBlockInst(std::string &block_name, std::string &block_type_name, int llx, int lly, bool movable, BlockOrient orient) {
  Assert(!IsBlockInstExist(block_name), "Block exists, cannot create this block again: " + block_name);
  int block_type_index = BlockTypeIndex(block_type_name);
  BlockType *block_type = &block_type_list[block_type_index];
  block_list.emplace_back(block_name, block_type, llx, lly, movable, orient);
  AddToBlockMap(block_name);
}

bool Circuit::IsNetExist(std::string &net_name) {
  return !(net_name_map.find(net_name) == net_name_map.end());
}

int Circuit::NetIndex(std::string &net_name) {
  Assert(IsNetExist(net_name), "Net does not exist, cannot find its index: " + net_name);
  return net_name_map.find(net_name)->second;
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

void Circuit::add_block_type(std::string &blockTypeName, int width, int height) {

}

void Circuit::add_pin_to_block(std::string &blockTypeName, std::string &pinName, int xOffset, int yOffset) {

}

void Circuit::add_new_block(std::string &blockName, std::string &blockTypeName, int llx, int lly, bool movable) {

}

void Circuit::add_pin_to_net(std::string &netName, std::string &blockName, std::string &pinName) {

}


void Circuit::parse_line(std::string &line, std::vector<std::string> &field_list) {
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

bool Circuit::read_nodes_file(std::string const &NameOfFile) {
  int tmp_w, tmp_h; // Width and Height
  bool tmp_movable = true;
  std::string tmp_name; // Name
  std::string line, tmp_string;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading node file\n";

  while (!ist.eof()) {
    std::vector<std::string> block_field;
    getline(ist, line);
    if (line.find("#")!=std::string::npos) {
      continue;
    }
    parse_line(line, block_field);
    if (block_field.size()<=2) {
      std::cout << "\t#" << line << "\n";
      continue;
    }
    tmp_name = block_field[0];
    try {
      tmp_w = std::stoi(block_field[1]);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion:" << block_field[1] << "\n";
      std::cout << line << "\n";
      return false;
    }
    try {
      tmp_h = std::stoi(block_field[2]);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion: " << block_field[2] << "\n";
      std::cout << line << "\n";
      return false;
    }

    tmp_movable = true;
    if (block_field.size() >= 4) {
      if (block_field[3].find("terminal")!=std::string::npos) {
        tmp_movable = false;
      }
    }

    if (!add_new_block(tmp_name, tmp_w, tmp_h, 0, 0, tmp_movable)) {
      return false;
    }
  }
  ist.close();

  return true;
}

void Circuit::report_block_list() {
  for (auto &&block: block_list) {
    std::cout << block << "\n";
  }
}

void Circuit::report_block_map() {
  for (auto &&name_num_pair: block_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

bool Circuit::read_nets_file(std::string const &NameOfFile) {
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading net file\n";
  std::string line;
  std::string tmp_net_name;

  /***here is a converter, converting ISPD file format from pin offset with respect to the center of a cell to lower left corner***/
  /*** start ***/
  //std::string new_file_name = NameOfFile + "_after_conversion";
  //std::ofstream ost(new_file_name.c_str());
  /*** To be continued ***/

  while (true) {
    getline(ist, line);
    if (line.find("NetDegree") != std::string::npos) {
      goto I_DONT_LIKE_THIS_BUT;
    } else {
      std::cout << "\t#" << line << "\n";
    }
  }
  while (!ist.eof()) {
    getline(ist, line);
    if (line.empty()) {
      continue;
    }
    I_DONT_LIKE_THIS_BUT:if (line.find("NetDegree") != std::string::npos) {
      std::vector<std::string> net_head_field;
      parse_line(line, net_head_field);
      //for (auto &&field: net_head_field) std::cout << field << "---";
      //std::cout << "\n";
      if (net_head_field.size() < 3) {
        std::cout << "Error!\n";
        std::cout << "Invalid input, filed number less than 3, expecting at least 3\n";
        std::cout << "\t" << line << "\n";
        return false;
      }
      tmp_net_name = net_head_field[2];
      create_blank_net(tmp_net_name);

      /*****cont'*****/
      //ost << line << "\n";
      /*****to be continues*****/

    } else {
      std::string tmp_block_name;
      int tmp_x_offset, tmp_y_offset;
      //double tmp_x_offset, tmp_y_offset;
      std::vector<std::string> pin_field;
      parse_line(line, pin_field);
      //for (auto &&field: pin_field) std::cout << field << "---";
      //std::cout << "\n";
      if (pin_field.size() < 4) {
        std::cout << "Error!\n";
        std::cout << "Invalid input, filed number less than 4, expecting at least 4\n";
        std::cout << "\t" << line << "\n";
        return false;
      }

      tmp_block_name = pin_field[0];
      try {
        tmp_x_offset = std::stoi(pin_field[2]);
        //tmp_x_offset = std::stod(pin_field[2]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion:" << pin_field[2] << "\n";
        std::cout << "\t" << line << "\n";
        return false;
      }
      try {
        tmp_y_offset = std::stoi(pin_field[3]);
        //tmp_y_offset = std::stod(pin_field[3]);
      } catch (...) {
        std::cout << "Error!\n";
        std::cout << "Invalid stoi conversion: " << pin_field[3] << "\n";
        std::cout << "\t" << line << "\n";
        return false;
      }
      if (!add_pin_to_net(tmp_net_name, tmp_block_name, tmp_x_offset, tmp_y_offset)) {
        return false;
      }

      /*****cont'*****/
      //size_t block_num = block_name_map.find(tmp_block_name)->second;
      //ost << "    " << pin_field[0] << "\t" << pin_field[1] << " : " << tmp_x_offset + block_list[block_num].Width()/2.0 << "\t" << tmp_y_offset + block_list[block_num].Height()/2.0 << "\n";
      /*****to be continues*****/

    }
  }
  ist.close();

  /*****cont'*****/
  //ost.close();

  return true;
}

void Circuit::report_net_list() {
  for (auto &&net: net_list) {
    std::cout << net << "\n";
  }
}

void Circuit::report_net_map() {
  for (auto &&name_num_pair: net_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

bool Circuit::read_pl_file(std::string const &NameOfFile) {
  Block *block;
  int tmp_block_index;
  int tmp_x, tmp_y;
  std::string line, tmp_name;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "Cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading placement file\n";

  while (!ist.eof()) {
    getline(ist, line);
    if (line.find("#")!=std::string::npos) {
      continue;
    }
    std::vector<std::string> block_field;
    parse_line(line, block_field);
    if (block_field.size() < 4) {
      std::cout << "\t#" << line << "\n";
      continue;
    }
    tmp_name = block_field[0];
    if (block_name_map.find(tmp_name) == block_name_map.end()) {
      std::cout << "Warning: Cannot find block with Name: " << tmp_name << "\n";
      std::cout << "Ignoring line:\n";
      std::cout << "\t" << line << "\n";
      continue;
    }
    tmp_block_index = block_name_map.find(tmp_name)->second;
    block = &block_list[tmp_block_index];
    try {
      tmp_x = std::stoi(block_field[1]);
      block->SetLLX(tmp_x);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion:" << block_field[1] << "\n";
      std::cout << line << "\n";
      return false;
    }
    try {
      tmp_y = std::stoi(block_field[2]);
      block->SetLLY(tmp_y);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion: " << block_field[2] << "\n";
      std::cout << line << "\n";
      return false;
    }
    block->SetOrient(block_field[3]);
    if (line.find("FIXED") != std::string::npos) {
      block->SetMovable(false);
    }
  }
  ist.close();
  return true;
}

double Circuit::ave_width_real_time() {
  ave_width_ = 0;
  for (auto &&block: block_list) {
    ave_width_ += block.Width();
  }
  ave_width_ /= block_list.size();
  return ave_width_;
}

double Circuit::ave_height_real_time() {
  ave_height_ = 0;
  for (auto &&block: block_list) {
    ave_height_ += block.Width();
  }
  ave_height_ /= block_list.size();
  return ave_height_;
}

int Circuit::tot_block_area_real_time() {
  tot_block_area_ = 0;
  for (auto &&block: block_list) {
    tot_block_area_ += block.Area();
  }
  return tot_block_area_;
}

double Circuit::ave_block_area_real_time() {
  return tot_block_area_real_time()/(double)block_list.size();
}

int Circuit::tot_movable_num_real_time() {
  tot_movable_num_ = 0;
  for (auto &&block: block_list) {
    if (block.IsMovable()) {
      tot_movable_num_++;
    }
  }
  return tot_movable_num_;
}

int Circuit::tot_unmovable_num_real_time() {
  return block_list.size() - tot_movable_num_real_time();
}

double Circuit::ave_width() {
  if (ave_width_ < 1e-5) {
    return ave_width_real_time();
  }
  return ave_width_;
}

double Circuit::ave_height() {
  if (ave_height_ < 1e-5) {
    return ave_height_real_time();
  }
  return ave_height_;
}

int Circuit::tot_block_area() {
  if (tot_block_area_ == 0) {
    return tot_block_area_real_time();
  }
  return tot_block_area_;
}

double Circuit::ave_block_area() {
  return tot_block_area()/(double)block_list.size();
}

int Circuit::tot_movable_num() {
  if (tot_movable_num_ == 0) {
    return tot_movable_num_real_time();
  }
  return tot_movable_num_;
}

int Circuit::tot_unmovable_num() {
  return block_list.size() - tot_movable_num_;
}

bool Circuit::write_nodes_file(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  for (auto &&block: block_list) {
    ost << "\t" << block.Name() << "\t" << block.Width() << "\t" << block.Height() << "\n";
  }

  return true;
}

bool Circuit::write_nets_file(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  for (auto &&net: net_list) {
    ost << "NetDegree : " << net.p() << "\t" << net.name() << "\n";
    for (auto &&pin: net.pin_list) {
      ost << "\t" << pin.name() << "\tI : " << pin.x_offset() << "\t" << pin.y_offset() << "\n";
    }
  }

  return true;
}
