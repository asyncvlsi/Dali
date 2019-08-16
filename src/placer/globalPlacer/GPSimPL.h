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

class GPSimPL: public Placer {
 private:
  double HPWLX_new = 0;
  double HPWLY_new = 0;
  double HPWLX_old = 1e30;
  double HPWLY_old = 1e30;
  bool HPWLX_converge = false;
  bool HPWLY_converge = false;
  double cg_precision = 0.001;
  int cg_iteration_max_num = 20;
  double HPWL_intra_linearSolver_precision = 0.001;
 public:
  GPSimPL();
  GPSimPL(double aspectRatio, double fillingRate);
  int TotBlockNum();
  double WidthEpsilon();
  double HeightEpsilon();


  void BlockLocInit();
  void InitCGFlags();
  void UpdateCGFlagsX();
  void UpdateHPWLX();
  void UpdateMaxMinX();
  void UpdateMaxMinCtoCX();
  void UpdateCGFlagsY();
  void UpdateHPWLY();
  void UpdateMaxMinY();
  void UpdateMaxMinCtoCY();
  void BuildProblemB2B(bool is_x_direction, SpMat &A, Eigen::VectorXd &b);
  void QuadraticPlacement();

  void DrawBlockNetList(std::string const &name_of_file= "block_net_list.txt");
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_H_
