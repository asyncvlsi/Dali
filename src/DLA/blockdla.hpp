//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BLOCKDLA_HPP
#define HPCC_BLOCKDLA_HPP

#include <vector>
#include <string>
#include "bindla.hpp"
#include "circuitblock.hpp"
#include "netdla.hpp"

struct bin_index {
  int iloc;
  int jloc;
  explicit bin_index(int i=0, int j=0): iloc(i), jloc(j){}
};

class net_dla_t;
class block_dla_t;
struct block_neighbor_t {
  block_dla_t *block;
  double total_wire_weight;
  explicit block_neighbor_t(block_dla_t *b= nullptr, double w=0): block(b), total_wire_weight(w){}
};

class block_dla_t: public block_t {
private:
  bool _placed; // 0 indicates this cell has not been placed, 1 means this cell has been placed
  bool _queued; // 0 indicates this cell has not been in Q_place, 1 means this cell has been in the Q_place
  double x0, y0;
  double vx, vy;
public:
  block_dla_t();
  block_dla_t(std::string &blockName, int w, int h, int lx = 0, int ly = 0, bool movable = true);
  void retrieve_info_from_database(const block_t &node_info);
  void write_info_to_database(block_t &node_info);

  int total_net();

  void set_placed(bool placed);
  bool is_placed();
  void set_queued(bool queued);
  bool is_queued();
  std::vector< bin_index > bin; // bins this cell is in
  std::vector< block_neighbor_t > neb_list; // the list of cells this cell is connected to
  void add_to_neb_list(block_dla_t *block_dla, double net_weight);
  void sort_neb_list();
  std::vector<net_dla_t *> net; // the list of nets this cell is connected to
  void add_to_net(net_dla_t *net_dla);

  // used to record which nets this node is connected to
  bool is_overlap(const block_dla_t &rhs) const;
  double overlap_area(const  block_dla_t &rhs) const;
  int wire_length_during_dla();
  void random_move(int distance);
};



#endif //HPCC_BLOCKDLA_HPP
