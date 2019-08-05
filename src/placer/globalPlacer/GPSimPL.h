//
// Created by yihan on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include "placer/placer.h"
#include "circuit/bin.h"

typedef struct {
  size_t pin;
  double weight;
} weightTuple;

class GPSimPL: public Placer {
 private:
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLx_converge = false;
  bool HPWLy_converge = false;
  double cg_precision = 0.01;
  double HPWL_intra_linearSolver_precision = 0.01;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);
  int TotBlockNum();
  double WidthEpsilon();
  double HeightEpsilon();

  std::vector< std::vector<weightTuple> > Ax, Ay;
  std::vector<size_t> kx, ky;
  // track the length of each row of Ax and Ay
  std::vector<double> bx, by;

  std::vector<double> ax;
  std::vector<double> ap;
  std::vector<double> z;
  std::vector<double> p;
  std::vector<double> JP;
  // some static variable used in the CG_solver

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
  void CG_solver(std::string const &dimension, std::vector< std::vector<weightTuple> > &A, std::vector<double> &b, std::vector<size_t> &k);
  void CG_solver_x();
  void CG_solver_y();

  bool draw_block_net_list(std::string const &filename="block_net_list.txt");
  bool StartPlacement() override;
  void report_hpwl();
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
