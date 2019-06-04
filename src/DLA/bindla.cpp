//
// Created by Yihang Yang on 2019-05-20.
//

#include "bindla.hpp"

bin_t::bin_t() {
  m_left = 0;
  m_bottom = 0;
  m_width = 0;
  m_height = 0;
}

bin_t::bin_t(int left, int bottom, int width, int height) : m_left(left), m_bottom(bottom), m_width(width),
                                                                  m_height(height) {}

void bin_t::set_left(int left) {
  m_left = left;
}

int bin_t::left() {
  return m_left;
}

void bin_t::set_bottom(int bottom) {
  m_bottom = bottom;
}

int bin_t::bottom() {
  return m_bottom;
}

void bin_t::set_width(int width) {
  m_width = width;
}

int bin_t::width() {
  return m_width;
}

void bin_t::set_height(int height) {
  m_height = height;
}

int bin_t::height() {
  return  m_height;
}

int bin_t::right() {
  return m_left + m_width;
}

int bin_t::top() {
  return m_bottom + m_height;
}
