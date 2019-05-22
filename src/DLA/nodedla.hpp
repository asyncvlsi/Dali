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
  explicit bin_index(int i=0, double j=0): iloc(i), jloc(j){}
};

struct cell_neighbor {
  int node_num;
  double wire_num;
  explicit cell_neighbor(int n=0, double w=0): node_num(n), wire_num(w){}
};

class node_dla {
public:
  node_dla();
  int node_num, w, h;
  std::string orientation;
  bool is_terminal;
  double x0, y0;
  // node_num is the primary key
  // is_terminal can only be 0 or 1, 0 means this node is not a terminal, 1 means it is
  // w is width
  // h is height
  double vx;
  double vy;
  std::vector<bin_index> bin; // bins this cell is in
  std::vector<cell_neighbor> neblist; // the list of cells this cell is connected to
  std::vector<int> net; // the list of nets this cell is connected to
  double totalwire; // the number of wires attached to this cell
  bool placed; // 0 indicates this cell has not been placed, 1 means this cell has been placed
  bool queued; // 0 indicates this cell has not been in Q_place, 1 means this cell has been in the Q_place
  // used to record which nets this node is connected to
  bool isterminal() const { return is_terminal; }
  int nodenum() const { return node_num; }
  int width() const { return w; }
  int height() const { return h; }
  int area() const { return w*h; }
  double llx() const { return x0 - w/2.0; }
  double lly() const { return y0 - h/2.0; }
  double urx() const { return x0 + w/2.0; }
  double ury() const { return y0 + h/2.0; }
  bool is_overlap(const node_dla &rhs) const;
  double overlap_area(const  node_dla &rhs) const;
  void retrieve_info_from_database(const node_t &node_info);
  void write_info_to_database(node_t &node_info);
};



#endif //HPCC_NODEDLA_HPP
