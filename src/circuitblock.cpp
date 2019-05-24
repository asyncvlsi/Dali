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
  _orientation = 0;
  _num = 0;
}

block_t::block_t(std::string &blockName, int w, int h, int llx, int lly, bool movable)
    : _name(blockName), _w(w), _h(h), _llx(llx), _lly(lly), _movable(movable) {}

void block_t::set_name(std::string &blockName) {
  _name = blockName;
}

std::string block_t::name(){
  return _name;
}
void block_t::set_width(int width) {
  _w = width;
}

int block_t::width() {
  return _w;
}

void block_t::set_height(int height) {
  _h = height;
}

int block_t::height(){
  return _h;
}

void block_t::set_llx(int lower_left_x) {
  _llx = lower_left_x;
}

int block_t::llx() {
  return _llx;
}

void block_t::set_lly(int lower_left_y) {
  _lly = lower_left_y;
}

int block_t::lly() {
  return _lly;
}

void block_t::set_movable(bool movable) {
  _movable = movable;
}

bool block_t::is_movable() {
  return _movable;
}

void block_t::set_num(size_t num) {
  _num = num;
}

size_t block_t::num() {
  return  _num;
}
