//
// Created by Yihang Yang on 2019-06-15.
//

#include "blockal.hpp"

block_al_t::block_al_t() {
  _w = 0;
  _h = 0;
  _dllx = 0;
  _dlly = 0;
  _movable = true;
  _num = 0;
}

block_al_t::block_al_t(std::string &blockName, int w, int h, int lx, int ly, bool movable): block_t(blockName, w, h, lx, ly,movable) {
  _dllx = lx;
  _dlly = ly;
  _num = 0;
}

void block_al_t::retrieve_info_from_database(const block_t &node_info) {
  _llx = node_info.llx();
  _lly = node_info.lly();
  _name = node_info.name();
  _num = node_info.num();
  _w = node_info.width();
  _h = node_info.height();
  _orientation = node_info.orientation();
  _movable = node_info.is_movable();
}

void block_al_t::write_info_to_database(block_t &node_info) {
  node_info.set_llx((int)_dllx);
  node_info.set_lly((int)_dlly);
}

void block_al_t::set_dllx(double lower_left_x) {
  _dllx = lower_left_x;
}

double block_al_t::dllx() const{
  return _dllx;
}

void block_al_t::set_dlly(double lower_left_y) {
  _dlly = lower_left_y;
}

double block_al_t::dlly() const{
  return _dlly;
}

void block_al_t::set_durx(double upper_right_x) {
  _dllx = upper_right_x - _w;
}

double block_al_t::durx() const{
  return _dllx + _w;
}

void block_al_t::set_dury(double upper_right_y) {
  _dlly = upper_right_y - _h;
}

double block_al_t::dury() const{
  return _dlly + _h;
}

void block_al_t::set_center_dx(double center_x) {
  _dllx = center_x - _w/2.0;
}

double block_al_t::dx() const{
  return _dllx + _w/2.0;
}

void block_al_t::set_center_dy(double center_y) {
  _dlly = center_y - _h/2.0;
}

double block_al_t::dy() const{
  return _dlly + _h/2.0;
}
