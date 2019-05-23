//
// Created by Yihang Yang on 2019-05-20.
//

#include <algorithm>
#include <cstdlib>
#include "nodedla.hpp"

node_dla::node_dla() {
  node_num = 0;
  w = 0;
  h = 0;
  orientation = "N";
  is_terminal = false;
  x0 = 0;
  y0 = 0;
  vx = 0;
  vy = 0;
  totalwire = 0;
  placed = false;
  queued = false;
}

bool node_dla::is_overlap(const  node_dla &rhs) const{
  // If one rectangle is on left side of other
  if (llx() > rhs.urx() || rhs.llx() > urx())
    return false;

  // If one rectangle is above other
  if (lly() > rhs.ury() || rhs.lly() > ury())
    return false;

  return true;
}

double node_dla::overlap_area(const  node_dla &rhs) const{
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

void node_dla::random_move(double distance) {
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

void node_dla::retrieve_info_from_database(const block_t &node_info){
  node_num = node_info.node_num;
  w = node_info.w;
  h = node_info.h;
  orientation = node_info.orientation;
  is_terminal = node_info.movable;
  x0 = node_info.x0;
  y0 = node_info.y0;
}

void node_dla::write_info_to_database(block_t &node_info) {
  node_info.x0 = x0;
  node_info.y0 = y0;
}