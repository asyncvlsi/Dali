//
// Created by yihan on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_

#include "placer/placer.h"
#include "circuit/bin.h"
#include "GPSimPL/simplblockaux.h"
#include "../../../module/Eigen/Sparse"
#include "../../../module/Eigen//IterativeLinearSolvers"

typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat; // declares a row-major sparse matrix type of double
typedef Eigen::Triplet<double> T; // A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.

typedef struct {
  int pin;
  double weight;
} WeightTuple;

class GPSimPL: public Placer {
 private:
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLx_converge = false;
  bool HPWLy_converge = false;
  double cg_precision = 0.05;
  int cg_iteration_max_num = 100;
  double HPWL_intra_linearSolver_precision = 0.05;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);
  int TotBlockNum();
  double WidthEpsilon();
  double HeightEpsilon();

  std::vector< std::vector<WeightTuple> > Ax, Ay;
  std::vector<double> bx, by;

  std::vector<double> ax;
  std::vector<double> ap;
  std::vector<double> z;
  std::vector<double> p;
  std::vector<double> JP;
  // some static variable used in the CGSolver

  std::vector< SimPLBlockAux > aux_list;
  void InterLockAuxInfo();
  void ReportAuxInfo();
  void BlockLocInit();
  void CGInit();
  void CGClose();
  void InitCGFlags();
  void BuildProblemCliqueX();
  void BuildProblemCliqueY();
  void UpdateCGFlagsX();
  void UpdateHPWLX();
  void UpdateMaxMinX();
  void BuildProblemB2BX();
  void UpdateMaxMinCtoCX();
  void BuildProblemB2BXNoOffset();
  void UpdateCGFlagsY();
  void UpdateHPWLY();
  void UpdateMaxMinY();
  void BuildProblemB2BY();
  void UpdateMaxMinCtoCY();
  void BuildProblemB2BYNoOffset();
  void CGSolver(std::string const &dimension, std::vector<std::vector<WeightTuple> > &A, std::vector<double> &b);
  void CGSolverX();
  void CGSolverY();
  void build_problem_b2b_x(SpMat &eigen_A, Eigen::VectorXd &b);
  void build_problem_b2b_y(SpMat &eigen_A, Eigen::VectorXd &b);
  void eigen_cg_solver();

  void DrawBlockNetList(std::string const &name_of_file= "block_net_list.txt");
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
