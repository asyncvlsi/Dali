//
// Created by Yihang Yang on 2019-05-22.
//

#include "circuit_node_net.hpp"

node_t::node_t() {
  orientation = "N";
  node_num = 0;
  is_terminal = false;
  w = 0;
  h = 0;
  x0 = 0;
  y0 = 0;
  anchorx = 0;
  anchory = 0;
}

bool node_t::is_overlap(const  node_t &rhs) const{
  // If one rectangle is on left side of other
  if (llx() > rhs.urx() || rhs.llx() > urx())
    return false;

  // If one rectangle is above other
  if (lly() > rhs.ury() || rhs.lly() > ury())
    return false;

  return true;
}

pininfo::pininfo() {
  pinnum = 0; // pinnum is the nodenum of the node which this pin belongs to
  xoffset = 0;
  yoffset = 0;
  x = 0;
  y = 0;
}

net_t::net_t() {
  net_num = 0;
  Inum = 0;
  Onum = 0;
  p = 0;
  maxpindex_x = 0;
  minpindex_x = 0;
  maxpindex_y = 0;
  minpindex_y = 0;
  invpmin1 = 0;
  hpwlx = 0;
  hpwly = 0;
}
