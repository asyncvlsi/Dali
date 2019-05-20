//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
#define HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP

#include <vector>
#include "../circuit.hpp"
#include "dlabin.hpp"

class diffusion_limited_aggregation_placer {
public:
  diffusion_limited_aggregation_placer();
  diffusion_limited_aggregation_placer(circuit_t &input_circuit);
  circuit_t *circuit;

  std::vector< node_t > boundry_list;
  std::vector< std::vector<dla_bin> > bin_list;
  void add_bound();
  void addnebnum(std::vector<node_t> &celllist, int cell1, int cell2, int netsize);
  void update_neighbor_list(std::vector<node_t> &celllist, std::vector<net_t> &NetList);
  bool start_place();
};


#endif //HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
