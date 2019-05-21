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
  std::vector< node_dla > node_list;
  std::vector< net_t > *net_list;

  void set_input(circuit_t &input_circuit);
  void report_result();
  void clear();

  std::vector< node_dla > boundry_list;
  std::vector< std::vector<bin_dla> > bin_list;
  void add_bound(std::vector<node_dla> &celllist, int cell1, int cell2, int netsize);
  void addnebnum(std::vector<node_dla> &celllist, int cell1, int cell2, int netsize);
  void sort_neighbor_list(std::vector<node_dla> &celllist);
  void update_neighbor_list(std::vector<node_dla> &celllist, std::vector<net_t> &NetList);
  bool place();
};


#endif //HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
