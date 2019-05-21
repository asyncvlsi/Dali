//
// Created by Yihang Yang on 2019-05-20.
//

#include "nodedla.hpp"

node_dla::node_dla() {
  node_num = 0;
  w = 0;
  h = 0;
  orientation = "N";
  is_terminal = false;
  x0 = 0;
  y0 = 0;
}

void node_dla::retrive_info_from_database(const node_t &node_info){
  node_num = node_info.node_num;
  w = node_info.w;
  h = node_info.h;
  orientation = node_info.orientation;
  is_terminal = node_info.is_terminal;
  x0 = node_info.x0;
  y0 = node_info.y0;
}

void node_dla::write_info_to_database(node_t &node_info) {
  node_info.node_num = node_num;
  node_info.w = w;
  node_info.h = h;
  node_info.orientation = orientation;
  node_info.is_terminal = is_terminal;
  node_info.x0 = x0;
  node_info.y0 = y0;
}