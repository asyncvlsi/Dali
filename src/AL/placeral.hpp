//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_PLACERAL_HPP
#define HPCC_PLACERAL_HPP

#include <vector>
#include <random>
#include "placer.hpp"
#include "blockal.hpp"
#include "netal.hpp"

typedef struct  {
  size_t pin;
  float weight;
} weight_tuple;

class placer_al_t: public placer_t {
private:
  size_t _movable_block_num;
  double _width_epsilon;
  double _height_epsilon;
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLx_converge = false;
  bool HPWLy_converge = false;
  double cg_precision = 0.05;
  double HPWL_intra_linearSolver_precision = 0.05;
public:
  placer_al_t();
  placer_al_t(double aspectRatio, double fillingRate);
  size_t movable_block_num();
  double width_epsilon();
  double height_epsilon();

  std::vector< block_al_t > block_list;
  std::vector< net_al_t > net_list;
  bool set_input_circuit(circuit_t *circuit) override;

  std::vector< std::vector<weight_tuple> > Ax, Ay;
  // declare matrix here, such that every function in this namespace can see these matrix
  std::vector<size_t> kx, ky;
  // track the length of each row of Ax and Ay
  std::vector<double> bx, by;

  void uniform_initialization();

  void cg_init();
  void cg_close();
  void initialize_HPWL_flags();
  void build_problem_clique_x();
  void build_problem_clique_y();
  void update_max_min_node_x();
  void update_HPWL_x();
  void build_problem_b2b_x();
  void build_problem_b2b_x_nooffset();
  void update_max_min_node_y();
  void update_HPWL_y();
  void build_problem_b2b_y();
  void build_problem_b2b_y_nooffset();
  void CG_solver(std::string const &dimension, std::vector< std::vector<weight_tuple> > &A, std::vector<double> &b, std::vector<size_t> &k);
  void CG_solver_x();
  void CG_solver_y();

  bool legalization();
  void shift_cg_solution_to_region_center();
  bool draw_block_net_list(std::string const &filename="block_net_list.m");

  bool start_placement() override;
};


#endif //HPCC_PLACERAL_HPP
