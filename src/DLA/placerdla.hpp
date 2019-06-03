//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_PLACERDLA_HPP
#define HPCC_PLACERDLA_HPP

#include <vector>
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
  void initialize_bin_list();
  void update_neighbor_list();
  /*
  void order_node_to_place(std::queue<int> &cell_to_place);
  void update_bin_list(int first_node_num, std::vector<int> &cell_out_bin);
  double net_hwpl_during_dla(net_t *net);
  double wirelength_during_DLA(int first_node_num);
  double net_hwpl(net_t *net);
  int is_legal(int first_node_num, std::vector<int> &cell_out_bin);
  bool random_release_from_boundaries(int boundary_num, block_dla_t &node);
  void diffuse(int first_node_num, std::vector<int> &cell_out_bin);
  bool DLA();
   */

  bool start_placement() override;

  bool draw_bin_list(std::string const &filename="binlist.m");
  bool draw_block_net_list(std::string const &filename="celllist.m");
  bool draw_placed_blocks(std::string const &filename="cellplaced.m");
  bool output_result(std::string const &filename="layout.pl");

  ~placer_dla_t() override;
};


#endif //HPCC_PLACERDLA_HPP
