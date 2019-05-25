//
// Created by Yihang Yang on 2019-03-26.
//

#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include "circuit.hpp"

circuit_t::circuit_t() {
  tot_movable_num = 0;
  tot_unmovable_num = 0;
  HPWL = 0;
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
    size_t net_list_size = net_list.size();
    net.set_num(net_list_size);
    net_name_map.insert(std::pair<std::string, int>(net.name(), net.num()));
    net_list.push_back(net);
    return true;
  } else {
    std::cout << "Error!\n";
    std::cout << "Existing block in block_list with name: " << net.name() << "\n";
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
  size_t pos0, pos1, pos2, NumOfTerminal;block_t tmp_block;
  std::string tmp_name; // name
  int tmp_w, tmp_h; // width and height

  std::string line, tmp_string;
  NumOfTerminal = 0;
  std::ifstream ist(NameOfFile.c_str());
  if (ist.is_open()==0) {
    std::cout << "cannot open input file " << NameOfFile << "\n";
    return false;
  }

  ave_width = 0;
  ave_height = 0;
  ave_cell_area = 0;
  while (!ist.eof()) {
    block_t tmp_block;
    std::vector<std::string> block_field;
    getline(ist, line);
    parse_line(line, block_field);
    if (block_field.size()<=3) {
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
    if (line.find("terminal") != std::string::npos) {
      tmp_block.set_movable(false);
    }

    if (!add_to_block_list(tmp_block)) {
      return false;
    }
  }
  ist.close();

  for (auto &&block: block_list) {
    ave_width += block.width();
    ave_height += block.height();
    ave_cell_area += block.width() * block.height();
  }

  ave_width = ave_width/tot_movable_num;
  ave_height = ave_height/tot_movable_num;
  ave_cell_area = ave_cell_area/tot_movable_num;

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
    }
  }
  while (!ist.eof()) {
    if (line.find("NetDegree") != std::string::npos) {
      std::vector<std::string> net_head_field;
      parse_line(line, net_head_field);
      //for (auto &&field: net_head_field) std::cout << field << "---";
      //std::cout << "\n";
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

        tmp_name = pin_field[0];
        tmp_pin.set_name(tmp_name);
        try {
          tmp_x_offset = std::stoi(pin_field[2]);
          tmp_pin.set_x_offset(tmp_x_offset);
        } catch (...) {
          std::cout << "Error!\n";
          std::cout << "Invalid stoi conversion:" << pin_field[2] << "\n";
          std::cout << line << "\n";
          return false;
        }
        try {
          tmp_y_offset = std::stoi(pin_field[3]);
          tmp_pin.set_y_offset(tmp_y_offset);
        } catch (...) {
          std::cout << "Error!\n";
          std::cout << "Invalid stoi conversion: " << pin_field[3] << "\n";
          std::cout << line << "\n";
          return false;
        }
        //std::cout << tmp_pin << "\n";

        if (!tmp_net.add_pin(tmp_pin)) {
          return false;
        }

        if (ist.eof()) {
          add_to_net_list(tmp_net);
          break;
        } else {
          getline(ist, line);
        }

        if (line.find("NetDegree") != std::string::npos) {
          add_to_net_list(tmp_net);
          break;
        }
      }
    }
  }
  ist.close();

  for (auto &&net: net_list) {
    std::cout << net << "\n";
  }

  return true;
}