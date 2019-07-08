//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_PLACERDLA_H
#define HPCC_PLACERDLA_H

#include <vector>
#include <queue>
#include <utility>
#include "circuit/circuit.h"
#include "circuit/circuitblock.h"
#include "circuit/circuitnet.h"
#include "circuit/circuitpin.h"
#include "placer.h"
#include "circuit/circuitbin.h"
#include "blockdla.h"
#include "netdla.h"

class placer_dla_t: public placer_t {
public:
  placer_dla_t();
  placer_dla_t(double aspectRatio, double fillingRate);


  std::vector< block_dla_t > block_list;
  std::vector< net_dla_t > net_list;
  bool set_input_circuit(circuit_t *circuit) override;

  std::vector< block_dla_t > boundary_list;
  void add_boundary_list();
  std::vector< std::vector<bin_t> > bin_list;
  int bin_width, bin_height;
  block_dla_t virtual_bin_boundary;
  void initialize_bin_list();
  void update_neighbor_list();

  std::queue<int> block_to_place_queue;
  std::vector<int> block_out_of_bin; // cells which are out of bins
  void prioritize_block_to_place();
  void update_bin_list(int first_blk_num);
  bool random_release_from_boundaries(int boundary_num, block_dla_t &node);
  bool is_legal(int first_node_num);
  void diffuse(int first_node_num);
  bool DLA();
  void shift_result_to_center();

  void report_hpwl();
  bool start_placement() override;
  void report_placement_result() override;

  bool draw_bin_list(std::string const &filename="bin_list.m");
  bool draw_block_net_list(std::string const &filename="block_net_list.m");
  bool draw_placed_blocks(std::string const &filename="cellplaced.m");
  bool output_result(std::string const &filename="layout.pl");

  ~placer_dla_t() override;
};


#endif //HPCC_PLACERDLA_H
