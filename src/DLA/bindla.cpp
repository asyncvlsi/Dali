//
// Created by Yihang Yang on 2019-05-20.
//

#include "bindla.hpp"

bin_t::bin_t() {
  _left = 0;
  _bottom = 0;
  _width = 0;
  _height = 0;
}

bin_t::bin_t(double left, double bottom, int width, int height) : _left(left), _bottom(bottom), _width(width),
                                                                  _height(height) {}

void bin_t::set_left(double left) {
  _left = left;
}

double bin_t::left() {
  return _left;
}

void bin_t::set_bottom(double bottom) {
  _bottom = bottom;
}

double bin_t::bottom() {
  return _bottom;
}

void bin_t::set_width(int width) {
  _width = width;
}

int bin_t::width() {
  return _width;
}

void bin_t::set_height(int height) {
  _height = height;
}

int bin_t::height() {
  return  _height;
}

