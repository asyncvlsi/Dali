//
// Created by Yihang Yang on 2019-06-27.
//

#include "circuitblocktype.hpp"

block_type_t::block_type_t(std::string &name, int width, int height) : _name(std::move(name)), _width(width), _height(height) {
  _num = 0;
}

std::string block_type_t::name() {
  return _name;
}

int block_type_t::num() {
  return  _num;
}

int block_type_t::width() {
  return _width;
}

int block_type_t::height() {
  return _height;
}
