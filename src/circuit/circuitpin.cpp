//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitpin.hpp"

pin_t::pin_t() {
  _x_offset = 0;
  _y_offset = 0;
  _block = nullptr;
  _pin_name = "";
}

pin_t::pin_t(int xOffset, int yOffset, block_t *block, std::string pinName)
              :_x_offset(xOffset), _y_offset(yOffset), _block(block), _pin_name(std::move(pinName)) {}

bool pin_t::operator ==(const pin_t &rhs) const {
  return ((_block == rhs._block) && (_x_offset == rhs._x_offset) && (_y_offset == rhs._y_offset));
}

std::string pin_t::name() const{
  return _block->name();
}

std::string pin_t::pin_name() const{
  if (_pin_name.length()>0) {
    return "( " + _block->name() + " " + _pin_name + " )";
  }
  return "( " + _block->name() + " " + "dummy_port_name" + " )";
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

int pin_t::abs_x() {
  if (get_block() == nullptr) {
    std::cout << "Error!\n";
    std::cout << "cannot find the block contain pin:\n" << this << "\n";
    assert(get_block() != nullptr);
  }
  return get_block()->llx() + x_offset();
}

int pin_t::abs_y() {
  if (get_block() == nullptr) {
    std::cout << "Error!\n";
    std::cout << "cannot find the block contain pin:\n" << this << "\n";
    assert(get_block() != nullptr);
  }
  return get_block()->lly() + y_offset();
}
