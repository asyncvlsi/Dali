//
// Created by Yihang Yang on 2019-03-26.
//

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include "circuit.hpp"

circuit_t::circuit_t() {
  HPWL = 0;
  _tot_movable_num = 0;
  _ave_width = 0;
  _ave_height = 0;
  _tot_block_area = 0;
}

/*
 * ... array somewhere with some size ...
 * block_t **allblocks;
 * int num_blocks = 0;
 * int max_blocks = 0;
 *
 * if (max_blocks == num_blocks) {
 *      if (max_blocks == 0) {
 *          allblocks = (block_t **) malloc (sizeof (block_t *)*1024);
 *          // check for NULL result, error
 *          max_blocks = 1024;
 *      }
 *      else {
 *            max_blocks = max_blocks + 1024;
 *           allblocks = (block_t **) realloc (sizeof (block_t *)*max_blocks);
 *           // check for null result
 *      }
 *  }
 *  allblocks[num_blocks] = new block_t (w, h, llx, lly, movable);
 *  num_blocks++;
 *  allblocks[num_blocks-1]->set_num (num_blocks-1);
 *  return allblocks[num_blocks-1];
 *
 */


bool circuit_t::add_new_block(std::string &blockName, int w, int h, int llx, int lly, bool movable, std::string typeName) {
  if (block_name_map.find(blockName) == block_name_map.end()) {
    size_t block_list_size = block_list.size();
    block_t tmp_block(blockName, w, h, llx, lly, movable, std::move(typeName));
    tmp_block.set_num(block_list_size);
    block_name_map.insert(std::pair<std::string, int>(tmp_block.name(), tmp_block.num()));
    block_list.push_back(tmp_block);
    return true;
  } else {
    std::cout << "Error!\n";
    std::cout << "Existing block in block_list with name: " << blockName << "\n";
    return false;
  }
}

bool circuit_t::create_blank_net(std::string &netName, double weight) {
  if (net_name_map.find(netName) == net_name_map.end()) {
    net_t tmp_net(netName, weight);
    size_t net_list_size = net_list.size();
    tmp_net.set_num(net_list_size);
    net_name_map.insert(std::pair<std::string, int>(tmp_net.name(), tmp_net.num()));
    net_list.push_back(tmp_net);
    return true;
  } else {
    std::cout << "Error!\n";
    std::cout << "Existing net in net_list with name: " << netName << "\n";
    return false;
  }
}

bool circuit_t::add_pin_to_net(std::string &netName, std::string &blockName, int xOffset, int yOffset, std::string pinName) {
  if (net_name_map.find(netName) == net_name_map.end()) {
    std::cout << "Error!\n";
    std::cout << "No net in net_list has name: " << netName << "\n";
    return false;
  }
  if (block_name_map.find(blockName) == block_name_map.end()){
    std::cout << "Error!\n";
    std::cout << "No block in block_list has name: " << blockName << "\n";
    return false;
  }

  int block_num = block_name_map.find(blockName)->second;
  pin_t tmp_pin(xOffset, yOffset, &block_list[block_num], std::move(pinName));

  int net_num = net_name_map.find(netName)->second;
  return net_list[net_num].add_pin(tmp_pin);

}


void circuit_t::parse_line(std::string &line, std::vector<std::string> &field_list) {
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

bool circuit_t::read_nodes_file(std::string const &NameOfFile) {
  int tmp_w, tmp_h; // width and height
  bool tmp_movable = true;
  std::string tmp_name; // name
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

void circuit_t::report_block_list() {
  for (auto &&block: block_list) {
    std::cout << block << "\n";
  }
}

void circuit_t::report_block_map() {
  for (auto &&name_num_pair: block_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

bool circuit_t::read_nets_file(std::string const &NameOfFile) {
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
      //ost << "    " << pin_field[0] << "\t" << pin_field[1] << " : " << tmp_x_offset + block_list[block_num].width()/2.0 << "\t" << tmp_y_offset + block_list[block_num].height()/2.0 << "\n";
      /*****to be continues*****/

    }
  }
  ist.close();

  /*****cont'*****/
  //ost.close();

  return true;
}

void circuit_t::report_net_list() {
  for (auto &&net: net_list) {
    std::cout << net << "\n";
  }
}

void circuit_t::report_net_map() {
  for (auto &&name_num_pair: net_name_map) {
    std::cout << name_num_pair.first << " " << name_num_pair.second << "\n";
  }
}

bool circuit_t::read_pl_file(std::string const &NameOfFile) {
  block_t *block;
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
      std::cout << "Warning: Cannot find block with name: " << tmp_name << "\n";
      std::cout << "Ignoring line:\n";
      std::cout << "\t" << line << "\n";
      continue;
    }
    tmp_block_index = block_name_map.find(tmp_name)->second;
    block = &block_list[tmp_block_index];
    try {
      tmp_x = std::stoi(block_field[1]);
      block->set_llx(tmp_x);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion:" << block_field[1] << "\n";
      std::cout << line << "\n";
      return false;
    }
    try {
      tmp_y = std::stoi(block_field[2]);
      block->set_lly(tmp_y);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion: " << block_field[2] << "\n";
      std::cout << line << "\n";
      return false;
    }
    block->set_orientation(block_field[3]);
    if (line.find("FIXED") != std::string::npos) {
      block->set_movable(false);
    }
  }
  ist.close();
  return true;
}

double circuit_t::ave_width_real_time() {
  _ave_width = 0;
  for (auto &&block: block_list) {
    _ave_width += block.width();
  }
  _ave_width /= block_list.size();
  return _ave_width;
}

double circuit_t::ave_height_real_time() {
  _ave_height = 0;
  for (auto &&block: block_list) {
    _ave_height += block.width();
  }
  _ave_height /= block_list.size();
  return _ave_height;
}

int circuit_t::tot_block_area_real_time() {
  _tot_block_area = 0;
  for (auto &&block: block_list) {
    _tot_block_area += block.area();
  }
  return _tot_block_area;
}

double circuit_t::ave_block_area_real_time() {
  return tot_block_area_real_time()/(double)block_list.size();
}

int circuit_t::tot_movable_num_real_time() {
  _tot_movable_num = 0;
  for (auto &&block: block_list) {
    if (block.is_movable()) {
      _tot_movable_num++;
    }
  }
  return _tot_movable_num;
}

int circuit_t::tot_unmovable_num_real_time() {
  return block_list.size() - tot_movable_num_real_time();
}

double circuit_t::ave_width() {
  if (_ave_width < 1e-5) {
    return ave_width_real_time();
  }
  return _ave_width;
}

double circuit_t::ave_height() {
  if (_ave_height < 1e-5) {
    return ave_height_real_time();
  }
  return _ave_height;
}

int circuit_t::tot_block_area() {
  if (_tot_block_area == 0) {
    return tot_block_area_real_time();
  }
  return _tot_block_area;
}

double circuit_t::ave_block_area() {
  return tot_block_area()/(double)block_list.size();
}

int circuit_t::tot_movable_num() {
  if (_tot_movable_num == 0) {
    return tot_movable_num_real_time();
  }
  return _tot_movable_num;
}

int circuit_t::tot_unmovable_num() {
  return block_list.size() - _tot_movable_num;
}

bool circuit_t::write_nodes_file(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (!ost.is_open()) {
    std::cout << "Cannot open file " << NameOfFile << "\n";
    return false;
  }

  for (auto &&block: block_list) {
    ost << "\t" << block.name() << "\t" << block.width() << "\t" << block.height() << "\n";
  }

  return true;
}

bool circuit_t::write_nets_file(std::string const &NameOfFile) {
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
