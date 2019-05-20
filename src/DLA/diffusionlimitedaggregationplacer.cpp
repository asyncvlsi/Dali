//
// Created by Yihang Yang on 2019-05-20.
//

#include "diffusionlimitedaggregationplacer.hpp"

diffusion_limited_aggregation_placer::diffusion_limited_aggregation_placer() {
  circuit = nullptr;
}

diffusion_limited_aggregation_placer::diffusion_limited_aggregation_placer(circuit_t &input_circuit) {
  circuit = &input_circuit;
}