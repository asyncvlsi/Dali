//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
#define HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP

#include <vector>
#include "../circuit.hpp"
#include "../circuit_node_net.hpp"
#include "bindla.hpp"
#include "nodedla.hpp"

class diffusion_limited_aggregation_placer {
public:
  diffusion_limited_aggregation_placer();
  explicit diffusion_limited_aggregation_placer(circuit_t &input_circuit);
  circuit_t *circuit;
  std::vector< net_t > *net_list;
  std::vector< node_dla > node_list;
  int LEFT, RIGHT, BOTTOM, TOP;

  void set_input(circuit_t &input_circuit);
  void report_result();
  void clear();

  std::vector< node_dla > boundary_list;
  void add_boundary_list();
  std::vector< std::vector<bin_dla> > bin_list;
  int bin_width, bin_height;
  void initialize_bin_list();
  void add_neb_num(int node_num1, int node_num2, int net_size);
  void sort_neighbor_list();
  void update_neighbor_list();
  void order_node_to_place(std::queue<int> &cell_to_place);
  void update_binlist(int firstcell, std::vector<int> &cell_out_bin);
  bool DLA();
  bool place();
};


#endif //HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
