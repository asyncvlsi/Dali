//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitblock.hpp"

block_t::block_t() {
  m_name = "";
  m_w = 0;
  m_h = 0;
  m_llx = 0;
  m_lly = 0;
  m_movable = true;
  m_orientation = "";
  m_num = 0;
}

block_t::block_t(std::string &blockName, int w, int h, int llx, int lly, bool movable)
    : m_name(blockName), m_w(w), m_h(h), m_llx(llx), m_lly(lly), m_movable(movable) {}

void block_t::set_name(std::string &blockName) {
  m_name = blockName;
}

std::string block_t::name() const{
  return m_name;
}
void block_t::set_width(int width) {
  m_w = width;
}

int block_t::width() const{
  return m_w;
}

void block_t::set_height(int height) {
  m_h = height;
}

int block_t::height() const{
  return m_h;
}

void block_t::set_llx(int lower_left_x) {
  m_llx = lower_left_x;
}

int block_t::llx() const{
  return m_llx;
}

void block_t::set_lly(int lower_left_y) {
  m_lly = lower_left_y;
}

int block_t::lly() const{
  return m_lly;
}

void block_t::set_urx(int upper_right_x) {
  m_llx = upper_right_x - m_w;
}

int block_t::urx() const{
  return m_llx + m_w;
}

void block_t::set_ury(int upper_right_y) {
  m_lly = upper_right_y - m_h;
}

int block_t::ury() const{
  return m_lly + m_h;
}

void block_t::set_center_x(double center_x) {
  m_llx = (int) (center_x - m_w/2.0);
}

double block_t::x() const{
  return m_llx + m_w/2.0;
}

void block_t::set_center_y(double center_y) {
  m_lly = (int) (center_y - m_h/2.0);
}

double block_t::y() const{
  return m_lly + m_h/2.0;
}

void block_t::set_movable(bool movable) {
  m_movable = movable;
}

bool block_t::is_movable() const{
  return m_movable;
}

int block_t::area() const{
  return m_h * m_w;
}

void block_t::set_orientation(std::string &orientation) {
  m_orientation = orientation;
}

std::string block_t::orientation() const{
  return m_orientation;
}

void block_t::set_num(size_t &num) {
  m_num = num;
}

size_t block_t::num() const{
  return  m_num;
}
