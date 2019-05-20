//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
#define HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP

#include <vector>
#include "circuit.hpp"

class diffusion_limited_aggregation_placer {
public:
  diffusion_limited_aggregation_placer();
  diffusion_limited_aggregation_placer(circuit_t &input_circuit);
  circuit_t *circuit;

  std::vector< node_t > boundry_list;
  std::vector< std::vector<bin_t> > bin_list;
  bool start_place();
};


#endif //HPCC_DIFFUSIONLIMITEDAGGREGATIONPLACER_HPP
