//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitpin.hpp"

pin_t::pin_t() {
  _block_name = "";
  _x_offset = 0;
  _y_offset = 0;
  _block = nullptr;
}

pin_t::pin_t(std::string &blockName, int xOffset, int yOffset)
              :_block_name(blockName), _x_offset(xOffset), _y_offset(yOffset) {}

bool pin_t::operator ==(const pin_t &rhs) const {
  return ((_block_name == rhs._block_name) && (_x_offset == rhs._x_offset) && (_y_offset == rhs._y_offset));
}

void pin_t::set_x_offset(int xOffset) {
  _x_offset = xOffset;
}

void pin_t::set_y_offset(int yOffset) {
  _y_offset = yOffset;
}

int pin_t::x_offset() {
  return _x_offset;
}

int pin_t::y_offset() {
  return _y_offset;
}

void pin_t::set_block_point(block_t *block) {
  _block = block;
}

block_t* pin_t::get_block() {
  return _block;
}
