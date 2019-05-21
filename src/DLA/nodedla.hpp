//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_NODEDLA_HPP
#define HPCC_NODEDLA_HPP

#include <vector>
#include <string>
#include "bindla.hpp"
#include "circuit_node_net.hpp"

struct bin_index {
  int iloc;
  int jloc;
};

struct cell_neighbor {
  int cellNum;
  double wireNum;
};

class node_dla {
public:
  node_dla();
  int node_num, w, h;
  std::string orientation;
  bool is_terminal;
  float x0, y0;
  // node_num is the primary key
  // is_terminal can only be 0 or 1, 0 means this node is not a terminal, 1 means it is
  // w is width
  // h is height
  std::vector<size_t> edgelist;
  // used to record which nets this node is connected to
  bool isterminal() const { return is_terminal; }
  int nodenum() const { return node_num; }
  int width() const { return w; }
  int height() const { return h; }
  int area() const { return w*h; }
  float llx() const { return x0 - w/(float)2; }
  float lly() const { return y0 - h/(float)2; }
  float urx() const { return x0 + w/(float)2; }
  float ury() const { return y0 + h/(float)2; }
  bool is_overlap(const node_dla &rhs) const;
  void retrive_info_from_database(const node_t &node_info);
  void write_info_to_database(node_t &node_info);
};



#endif //HPCC_NODEDLA_HPP
