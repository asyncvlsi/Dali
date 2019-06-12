//
// Created by Yihang Yang on 2019-06-11.
//

#ifndef HPCC_CGSOLVER_HPP
#define HPCC_CGSOLVER_HPP

#include <vector>

typedef struct  {
  size_t pin;
  float weight;
} weight_tuple;

class cg_solver {
  cg_solver();
  std::vector< std::vector<weight_tuple> > Ax, Ay;
  // declare matrix here, such that every function in this namespace can see these matrix
  std::vector<size_t> kx, ky;
  // track the length of each row of Ax and Ay
  std::vector<double> bx, by;

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
  void add_anchor_x();
  void add_anchor_y();
  void CG_solver(std::string const &dimension, std::vector< std::vector<weight_tuple> > &A, std::vector<float> &b, std::vector<size_t> &k);
  void CG_solver_x();
  void CG_solver_y();
};


#endif //HPCC_CGSOLVER_HPP
