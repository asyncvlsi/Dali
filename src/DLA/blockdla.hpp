//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BLOCKDLA_HPP
#define HPCC_BLOCKDLA_HPP

#include <vector>
#include <string>
#include "bindla.hpp"
#include "circuitblock.hpp"

struct bin_index {
  int iloc;
  int jloc;
  explicit bin_index(int i=0, double j=0): iloc(i), jloc(j){}
};

struct block_neighbor {
  int block_num;
  double wire_num;
  explicit block_neighbor(int n=0, double w=0): block_num(n), wire_num(w){}
};

class block_dla: public block_t {
private:
  double _total_wire; // the number of wires attached to this cell
  bool _placed; // 0 indicates this cell has not been placed, 1 means this cell has been placed
  bool _queued; // 0 indicates this cell has not been in Q_place, 1 means this cell has been in the Q_place
public:
  block_dla();
  double x0, y0;
  double vx, vy;
  std::vector<bin_index> bin; // bins this cell is in
  std::vector<block_neighbor> neblist; // the list of cells this cell is connected to
  std::vector<int> net; // the list of nets this cell is connected to
  // used to record which nets this node is connected to
  bool is_overlap(const block_dla &rhs) const;
  double overlap_area(const  block_dla &rhs) const;
  void random_move(double distance);
  void retrieve_info_from_database(const block_t &node_info);
  void write_info_to_database(block_t &node_info);
};



#endif //HPCC_BLOCKDLA_HPP
