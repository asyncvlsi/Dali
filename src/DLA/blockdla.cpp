//
// Created by Yihang Yang on 2019-05-20.
//

#include <algorithm>
#include <cstdlib>
#include "blockdla.hpp"

block_dla_t::block_dla_t() {
  _num = 0;
  _w = 0;
  _h = 0;
  _orientation = "N";
  _movable = true;
  _total_wire = 0;
  _placed = false;
  _queued = false;
  x0 = 0;
  y0 = 0;
  vx = 0;
  vy = 0;
}

block_dla_t::block_dla_t(std::string &blockName, int w, int h, int llx, int lly, bool movable):block_t(blockName, w, h, llx, lly, movable) {

}

void block_dla_t::retrieve_info_from_database(const block_t &block){
  _name = block.name();
  _num = block.num();
  _w = block.width();
  _h = block.height();
  _orientation = block.orientation();
  _movable = block.is_movable();
}

void block_dla_t::write_info_to_database(block_t &block) {
  block.set_llx(_llx);
  block.set_lly(_lly);
}

void block_dla_t::set_placed(bool placed) {
  _placed = placed;
}

bool block_dla_t::is_placed() {
  return _placed;
}

void block_dla_t::set_queued(bool queued) {
  _queued = queued;
}

bool block_dla_t::is_queued() {
  return _queued;
}

bool block_dla_t::is_overlap(const  block_dla_t &rhs) const{
  bool not_overlap;
  not_overlap = llx() > rhs.urx() || rhs.llx() > urx() || lly() > rhs.ury() || rhs.lly() > ury();
  // If one rectangle is on left side of another or if one rectangle is above another
  return !not_overlap;
}

double block_dla_t::overlap_area(const  block_dla_t &rhs) const{
  if (is_overlap(rhs)) {
    double l1x, l1y, r1x, r1y, l2x, l2y, r2x, r2y;
    l1x = llx();
    l1y = lly();
    r1x = urx();
    r1y = ury();
    l2x = rhs.llx();
    l2y = rhs.lly();
    r2x = rhs.urx();
    r2y = rhs.ury();
    return (std::min(r1x, r2x) - std::max(l1x, l2x)) * (std::min(r1y, r2y) - std::max(l1y, l2y));
  } else {
    return 0;
  }
}

void block_dla_t::random_move(double distance) {
  int rand_num = std::rand();
  if (rand_num % 4 == 0) {
    // leftward move
    x0 -= distance;
  } else if (rand_num % 4 == 1) {
    // rightward move
    x0 += distance;
  } else if (rand_num % 4 == 2) {
    // downward move
    y0 -= distance;
  } else {
    // upward move
    y0 += distance;
  }
}
