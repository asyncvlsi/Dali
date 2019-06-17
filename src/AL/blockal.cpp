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
  vx = 0;
  vy = 0;
}

block_al_t::block_al_t(std::string &blockName, int w, int h, int lx, int ly, bool movable): block_t(blockName, w, h, lx, ly,movable) {
  _dllx = lx;
  _dlly = ly;
  _num = 0;
  vx = 0;
  vy = 0;
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

void block_al_t::x_increment(double delta_x) {
  _dllx += delta_x;
}

void block_al_t::y_increment(double delta_y) {
  _dlly += delta_y;
}

bool block_al_t::is_overlap(const block_al_t &rhs) const {
  bool not_overlap;
  not_overlap = dllx() >= rhs.durx() || rhs.dllx() >= durx() || dlly() >= rhs.dury() || rhs.dlly() >= dury();
  // If one rectangle is on left side of another or if one rectangle is above another
  return !not_overlap;
}

double block_al_t::overlap_area(const  block_al_t &rhs) const {
  if (is_overlap(rhs)) {
    double l1x, l1y, r1x, r1y, l2x, l2y, r2x, r2y;
    l1x = dllx();
    l1y = dlly();
    r1x = durx();
    r1y = dury();
    l2x = rhs.dllx();
    l2y = rhs.dlly();
    r2x = rhs.durx();
    r2y = rhs.dury();
    return (std::min(r1x, r2x) - std::max(l1x, l2x)) * (std::min(r1y, r2y) - std::max(l1y, l2y));
  } else {
    return 0;
  }
}

void block_al_t::modif_vx() {
  double epsilon = 1e-5;
  double modv = 0;
  if (fabs(vx)>=1) modv = round(vx);
  else if (fabs(vx)>epsilon) modv = vx/fabs(vx);
  else modv = 0;
  vx = modv;
}

void block_al_t::modif_vy() {
  double epsilon = 1e-5;
  double modv = 0;
  if (fabs(vy)>=1) modv = round(vy);
  else if (fabs(vy)>epsilon) modv = vy/fabs(vy);
  else modv = 0;
  vy = modv;
}

void block_al_t::update_loc(int time_step) {
  _dllx += vx * time_step;
  _dlly += vy * time_step;
}
