//
// Created by Yihang Yang on 2019-05-16.
//

#ifndef HPCC_CIRCUIT_NODE_NET_HPP
#define HPCC_CIRCUIT_NODE_NET_HPP

#include <vector>
#include <string>

class node_t {
public:
  node_t();
  int node_num, w, h;
  std::string orientation;
  bool is_terminal;
  float anchorx, anchory, x0, y0;
  // node_t_num is the promary key
  // is_terminal can only be 0 or 1, 0 means this node_t is not a terminal, 1 means it is
  // w is width
  // h is height
  // the location should be int, but cg should give a better precision if the location is float, will try int latter
  std::vector<size_t> edgelist;
  // used to record which nets this node_t is connected to
  bool isterminal() const { return is_terminal; }
  int nodenum() const { return node_num; }
  int width() const { return w; }
  int height() const { return h; }
  int area() const { return w*h; }
  float llx() const { return x0 - w/(float)2; }
  float lly() const { return y0 - h/(float)2; }
  float urx() const { return x0 + w/(float)2; }
  float ury() const { return y0 + h/(float)2; }
  void cp_x_2_anchor() { anchorx = x0; }
  void cp_y_2_anchor() { anchory = y0; }
  void swap_x_anchor() { std::swap(x0, anchorx); }
  void swap_y_anchor() { std::swap(y0, anchory); }
  bool is_overlap(const node_t &rhs) const;
};

class pininfo {
public:
  pininfo();
  size_t pinnum;
  float xoffset;
  float yoffset;
  float x;
  float y;
};

class net_t {
public:
  net_t();
  size_t net_num, Inum, Onum, p, maxpindex_x, minpindex_x, maxpindex_y, minpindex_y;
  float invpmin1, hpwlx, hpwly;
  // net_num is the primary key
  // Inum is the total number of input pin
  // Onum is the total number of output pin
  // p is the "p" in "p"-pin net
  // maxpindex_x is the index of the pin whose node number has the maximum x
  // minpindex_x is the index of the pin whose node number has the minimum x
  // maxpindex_y is the index of the pin whose node number has the maximum y
  // minpindex_y is the index of the pin whose node number has the minimum y
  std::vector<pininfo> pinlist;
  std::vector<char> nodetype;
};

#endif //HPCC_CIRCUIT_NODE_NET_HPP