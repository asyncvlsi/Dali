//
// Created by Yihang Yang on 2019-05-23.
//

#include "block.h"

block_t::block_t(BlockType *type, std::string name, int llx, int lly, bool movable, orient_t orient) : _type(
    type), _name(std::move(name)), _llx(llx), _lly(lly), _movable(movable), _orient(orient) {
  _num = 0;
}

void block_t::set_name(std::string blockName) {
  _name = std::move(blockName);
}

std::string block_t::name() const{
  return _name;
}

int block_t::width() const{
  return _type->Width();
}


int block_t::height() const{
  return _type->Height();
}

void block_t::set_llx(int lower_left_x) {
  _llx = lower_left_x;
}

int block_t::llx() const{
  return _llx;
}

void block_t::set_lly(int lower_left_y) {
  _lly = lower_left_y;
}

int block_t::lly() const{
  return _lly;
}

void block_t::set_urx(int upper_right_x) {
  _llx = upper_right_x - width();
}

int block_t::urx() const{
  return _llx + width();
}

void block_t::set_ury(int upper_right_y) {
  _lly = upper_right_y - height();
}

int block_t::ury() const{
  return _lly + height();
}

void block_t::set_center_x(double center_x) {
  _llx = (int) (center_x - width()/2.0);
}

double block_t::x() const{
  return _llx + width()/2.0;
}

void block_t::set_center_y(double center_y) {
  _lly = (int) (center_y - height()/2.0);
}

double block_t::y() const{
  return _lly + height()/2.0;
}

void block_t::set_movable(bool movable) {
  _movable = movable;
}

bool block_t::is_movable() const {
  return _movable;
}

int block_t::area() const {
  return height() * width();
}

void block_t::set_orientation(orient_t &orient) {
  _orient = orient;
}

orient_t block_t::orientation() const {
  return _orient;
}

std::string block_t::orient_str() const {
  std::string s;
  switch (_orient) {
    case 0: { s = "N"; } break;
    case 1: { s = "S"; } break;
    case 2: { s = "W"; } break;
    case 3: { s = "E"; } break;
    case 4: { s = "FN"; } break;
    case 5: { s = "FS"; } break;
    case 6: { s = "FW"; } break;
    case 7: { s = "FE"; } break;
    default: { s = "unknown";
      std::cout << "unknown block orientation, this should not happen\n";
      exit(1); }
  }
  return s;
}

void block_t::set_num(size_t &number) {
  _num = number;
}

size_t block_t::num() const{
  return  _num;
}

std::string block_t::type_name() {
  return _type->Name();
}

std::string block_t::place_status() {
  return "PLACED";
}

std::string block_t::lower_left_corner() {
  return "( " + std::to_string(llx()) + " " + std::to_string(lly()) + " )";
}



