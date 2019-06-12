//
// Created by Yihang Yang on 2019-05-20.
//

#include "circuitbin.hpp"

bin_t::bin_t() {
  _left = 0;
  _bottom = 0;
  _width = 0;
  _height = 0;
}

bin_t::bin_t(int left_arg, int bottom_arg, int width_arg, int height_arg) : _left(left_arg), _bottom(bottom_arg), _width(width_arg),
                                                                  _height(height_arg) {}

void bin_t::set_left(int left_arg) {
  _left = left_arg;
}

int bin_t::left() {
  return _left;
}

void bin_t::set_bottom(int bottom_arg) {
  _bottom = bottom_arg;
}

int bin_t::bottom() {
  return _bottom;
}

void bin_t::set_width(int width_arg) {
  _width = width_arg;
}

int bin_t::width() {
  return _width;
}

void bin_t::set_height(int height_arg) {
  _height = height_arg;
}

int bin_t::height() {
  return  _height;
}

int bin_t::right() {
  return _left + _width;
}

int bin_t::top() {
  return _bottom + _height;
}
