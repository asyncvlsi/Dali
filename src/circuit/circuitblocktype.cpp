//
// Created by Yihang Yang on 2019-06-27.
//

#include "circuitblocktype.hpp"

block_type_t::block_type_t() {
  _name = "";
  _width = 0;
  _height = 0;
  _num = 0;
}

block_type_t::block_type_t(std::string &name, int width, int height) : _name(std::move(name)), _width(width), _height(height) {
  _num = 0;
}

void block_type_t::set_name(std::string &typeName) {
  _name = typeName;
}

std::string block_type_t::name() {
  return _name;
}

void block_type_t::set_width(int w) {
  _width = w;
}

int block_type_t::width() {
  return _width;
}

void block_type_t::set_height(int h) {
  _height = h;
}

int block_type_t::height() {
  return _height;
}

int block_type_t::num() {
  return  _num;
}

bool block_type_t::add_pin(std::string &pinName, double xOffset, double yOffset) {
  if (pinname_num_map.find(pinName) == pinname_num_map.end()) {
    pinname_num_map.insert(std::pair<std::string, size_t>(pinName, pin_list.size()));
    pin_list.emplace_back(xOffset, yOffset);
    return true;
  } else {
    std::cout << "Error!\n";
    std::cout << "Existing pin in pin_list with name: " << pinName << "\n";
    return false;
  }
}
