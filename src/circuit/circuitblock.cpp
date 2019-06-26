//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitblock.hpp"

block_t::block_t() {
  _name = "";
  _w = 0;
  _h = 0;
  _llx = 0;
  _lly = 0;
  _movable = true;
  _orientation = "";
  _num = 0;
}

block_t::block_t(std::string &blockName, int w, int h, int lx, int ly, bool movable)
    : _name(blockName), _w(w), _h(h), _llx(lx), _lly(ly), _movable(movable) {}

void block_t::set_name(std::string &blockName) {
  _name = blockName;
}

std::string block_t::name() const{
  return _name;
}
void block_t::set_width(int w) {
  _w = w;
}

int block_t::width() const{
  return _w;
}

void block_t::set_height(int h) {
  _h = h;
}

int block_t::height() const{
  return _h;
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
  _llx = upper_right_x - _w;
}

int block_t::urx() const{
  return _llx + _w;
}

void block_t::set_ury(int upper_right_y) {
  _lly = upper_right_y - _h;
}

int block_t::ury() const{
  return _lly + _h;
}

void block_t::set_center_x(double center_x) {
  _llx = (int) (center_x - _w/2.0);
}

double block_t::x() const{
  return _llx + _w/2.0;
}

void block_t::set_center_y(double center_y) {
  _lly = (int) (center_y - _h/2.0);
}

double block_t::y() const{
  return _lly + _h/2.0;
}

void block_t::set_movable(bool movable) {
  _movable = movable;
}

bool block_t::is_movable() const{
  return _movable;
}

int block_t::area() const{
  return _h * _w;
}

void block_t::set_orientation(std::string &orient) {
  _orientation = orient;
}

std::string block_t::orientation() const{
  return _orientation;
}

void block_t::set_num(size_t &number) {
  _num = number;
}

size_t block_t::num() const{
  return  _num;
}

std::string block_t::type() {
  return _name;
}

std::string block_t::place_status() {
  return "PLACED";
}

std::string block_t::lower_left_corner() {
  return "( " + std::to_string(llx()) + " " + std::to_string(lly()) + " )";
}

