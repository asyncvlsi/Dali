//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_PLACERAL_H
#define HPCC_PLACERAL_H

#include <vector>
#include <set>
#include <random>
#include <cmath>
#include <algorithm>
#include "../module/Eigen/Sparse"
#include "../module/Eigen//IterativeLinearSolvers"
#include "placer.h"
#include "blockal.h"
#include "netal.h"
#include "tetris/tetrisspace.h"
#include "circuit/circuitbin.h"

typedef Eigen::SparseMatrix<double> SpMat; // declares a column-major sparse matrix type of double
typedef Eigen::Triplet<double> T; // A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.

class placer_al_t: public placer_t {
private:
  size_t _movable_block_num;
  size_t _terminal_block_num;
  double _width_epsilon;
  double _height_epsilon;
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLx_converge = false;
  bool HPWLy_converge = false;
  double cgTolerance = 0.001;
  double HPWL_intra_linearSolver_precision = 0.01;
  int max_legalization_iteration = 1000;
  int iteration_limit_diffusion = 10;
  int time_step = 1;
  int cgIterMaxNum = 100;
  int b2bIterMaxNum = 15;
public:
  placer_al_t();
  placer_al_t(double aspectRatio, double fillingRate);
  size_t movable_block_num();
  size_t terminal_block_num();
  size_t block_num();
  double width_epsilon();
  double height_epsilon();

  std::vector< block_al_t > block_list;
  std::vector< net_al_t > net_list;
  bool set_input_circuit(circuit_t *circuit) override;

  void uniform_initialization();
  void build_problem_b2b_x(SpMat &eigen_A, Eigen::VectorXd &b);
  void build_problem_b2b_y(SpMat &eigen_A, Eigen::VectorXd &b);
  void eigen_cg_solver();

  void initialize_HPWL_flags();
  void update_max_min_node_x();
  void update_HPWL_x();
  void update_max_min_node_y();
  void update_HPWL_y();

  std::vector< block_al_t > boundary_list;
  void add_boundary_list();
  std::vector< std::vector<bin_t> > bin_list;
  int bin_width, bin_height;
  int bin_list_size_x, bin_list_size_y;
  block_al_t virtual_bin_boundary;
  void initialize_bin_list();
  bool draw_bin_list(std::string const &filename="bin_list.m");

  void shift_to_region_center();
  void expansion_legalization();
  bool draw_block_net_list(std::string const &filename="block_net_list.txt");
  void update_block_in_bin();
  bool check_legal();
  void integerize();
  void update_velocity();
  void update_velocity_force_damping();
  void update_position();
  void diffusion_legalization();
  bool legalization();
  void diffusion_with_gravity();
  void diffusion_with_gravity2();
  bool tetris_legalization();
  bool tetris_legalization2();
  bool gravity_legalization();
  bool post_legalization_optimization();
  bool start_placement() override;
  void report_placement_result() override;
  void report_hpwl();
};


#endif //HPCC_PLACERAL_H
