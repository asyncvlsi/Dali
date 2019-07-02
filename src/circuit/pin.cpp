//
// Created by Yihang Yang on 2019-05-23.
//

#include "pin.hpp"

pin_t::pin_t(block_type_t *blockType, std::string &pinName, int xOffset, int yOffset): _block_type(blockType), _pin_name(pinName), _x_offset(xOffset), _y_offset(yOffset) {}

bool pin_t::operator ==(const pin_t &rhs) const {
  return ((_block_type == rhs._block_type) && (_pin_name == rhs._pin_name));
}

std::string pin_t::block_name() const{
  return _block_type->name();
}

std::string pin_t::name() const{
  return _pin_name;
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

void pin_t::set_block_point(block_type_t *blockType) {
  _block_type = blockType;
}

block_type_t* pin_t::get_block() {
  return _block_type;
}
