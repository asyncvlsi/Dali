//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_PLACERDLA_HPP
#define HPCC_PLACERDLA_HPP

#include <vector>
#include <queue>
#include <utility>
#include "circuit.hpp"
#include "circuitblock.hpp"
#include "circuitnet.hpp"
#include "circuitpin.hpp"
#include "placer.hpp"
#include "bindla.hpp"
#include "blockdla.hpp"
#include "netdla.hpp"

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
  int is_legal(int first_node_num);
  void diffuse(int first_node_num);
  bool DLA();

  bool start_placement() override;

  bool draw_bin_list(std::string const &filename="binlist.m");
  bool draw_block_net_list(std::string const &filename="celllist.m");
  bool draw_placed_blocks(std::string const &filename="cellplaced.m");
  bool output_result(std::string const &filename="layout.pl");

  ~placer_dla_t() override;
};


#endif //HPCC_PLACERDLA_HPP
