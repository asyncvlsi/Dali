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

bool circuit_t::add_to_block_list(block_t &block) {
  if (block_name_map.find(block.name()) == block_name_map.end()) {
    size_t block_list_size = block_list.size();
    block.set_num(block_list_size);
    block_name_map.insert(std::pair<std::string, int>(block.name(), block.num()));
    block_list.push_back(block);
    return true;
  } else {
    std::cout << "Error!\n";
    std::cout << "Existing block in block_list with name: " << block.name() << "\n";
    return false;
  }
}

bool circuit_t::add_to_net_list(net_t &net) {
  if (net_name_map.find(net.name()) == net_name_map.end()) {
    for (auto &&pin: net.pin_list) {
      if (block_name_map.find(pin.name()) == block_name_map.end()) {
        std::cout << "Error!\n";
        std::cout << "Invalid pin: block name does not match block_list\n";
        std::cout << pin << "\n";
        return false;
      }
      int block_num = block_name_map.find(pin.name())->second;
      pin.set_block_point(block_list[block_num]);
    }

    size_t net_list_size = net_list.size();
    net.set_num(net_list_size);
    net_name_map.insert(std::pair<std::string, int>(net.name(), net.num()));
    net_list.push_back(net);
    return true;
  } else {
    std::cout << "Error!\n";
    std::cout << "Existing net in net_list with name: " << net.name() << "\n";
    return false;
  }
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
  std::string tmp_name; // name
  std::string line, tmp_string;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading node file\n";

  while (!ist.eof()) {
    block_t tmp_block;
    std::vector<std::string> block_field;
    getline(ist, line);
    parse_line(line, block_field);
    if (block_field.size()<=3) {
      std::cout << "Ignoring line:\n";
      std::cout << "\t" << line << "\n";
      continue;
    }
    tmp_name = block_field[0];
    tmp_block.set_name(tmp_name);
    try {
      tmp_w = std::stoi(block_field[1]);
      tmp_block.set_width(tmp_w);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion:" << block_field[1] << "\n";
      std::cout << line << "\n";
      return false;
    }
    try {
      tmp_h = std::stoi(block_field[2]);
      tmp_block.set_height(tmp_h);
    } catch (...) {
      std::cout << "Error!\n";
      std::cout << "Invalid stoi conversion: " << block_field[2] << "\n";
      std::cout << line << "\n";
      return false;
    }

    if (!add_to_block_list(tmp_block)) {
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
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading net file\n";

  net_t tmp_net;
  std::string tmp_net_name;
  std::string tmp_block_name;
  std::string line;
  net_t net_temp;

  bool is_in_useful_context = false;
  while (!is_in_useful_context) {
    getline(ist, line);
    if (line.find("NetDegree") != std::string::npos) {
      is_in_useful_context = true;
    } else {
      std::cout << "Ignoring line:\n";
      std::cout << "\t" << line << "\n";
    }
  }
  while (!ist.eof()) {
    if (line.find("NetDegree") != std::string::npos) {
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
      tmp_net.set_name(net_head_field[2]);
      //std::cout << tmp_net.name() << "\n";
      tmp_net.pin_list.clear();
      getline(ist, line);
    } else {
      while (true) {
        pin_t tmp_pin;
        std::string tmp_name;
        int tmp_x_offset, tmp_y_offset;
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

        tmp_name = pin_field[0];
        tmp_pin.set_name(tmp_name);
        try {
          tmp_x_offset = std::stoi(pin_field[2]);
          tmp_pin.set_x_offset(tmp_x_offset);
        } catch (...) {
          std::cout << "Error!\n";
          std::cout << "Invalid stoi conversion:" << pin_field[2] << "\n";
          std::cout << "\t" << line << "\n";
          return false;
        }
        try {
          tmp_y_offset = std::stoi(pin_field[3]);
          tmp_pin.set_y_offset(tmp_y_offset);
        } catch (...) {
          std::cout << "Error!\n";
          std::cout << "Invalid stoi conversion: " << pin_field[3] << "\n";
          std::cout << "\t" << line << "\n";
          return false;
        }

        if (!tmp_net.add_pin(tmp_pin)) {
          return false;
        }

        if (ist.eof()) {
          if (!add_to_net_list(tmp_net)) {
            return false;
          }
          break;
        } else {
          getline(ist, line);
        }

        if (line.find("NetDegree") != std::string::npos) {
          if (!add_to_net_list(tmp_net)) {
            return false;
          }
          break;
        }
      }
    }
  }
  ist.close();
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
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  std::cout << "Start reading placement file\n";

  while (!ist.eof()) {
    getline(ist, line);
    bool is_comment = false;
    for (auto &&c: line) {
      if (c == '#') {
        is_comment = true;
        break;
      }
    }
    if (is_comment) {
      continue;
    }

    std::vector<std::string> block_field;
    parse_line(line, block_field);
    if (block_field.size() < 4) {
      std::cout << "Ignoring line:\n";
      std::cout << "\t" << line << "\n";
      continue;
    }
    tmp_name = block_field[0];
    if (block_name_map.find(tmp_name) == block_name_map.end()) {
      std::cout << "Warning: cannot find block with name: " << tmp_name << "\n";
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
      tmp_y = std::stoi(block_field[1]);
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