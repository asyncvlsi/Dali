//
// Created by Yihang Yang on 9/3/19.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_H_

#include <vector>
#include <random>
#include "../placer.h"
#include "../globalPlacer/GPSimPL.h"
#include "../../../module/Eigen/Sparse"
#include "../../../module/Eigen//IterativeLinearSolvers"
#include "DPLinear/scaffoldnet.h"

typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat; // declares a row-major sparse matrix type of double
typedef Eigen::Triplet<double> T; // A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.

class DPLinear: public GPSimPL {
 private:
    double r = 0;

 public:
  DPLinear();
  std::vector< ScaffoldNet > scaffold_net_list;

  void UpdateScaffoldNetList();
  void BuildProblemDPLinear(bool is_x_direction, Eigen::VectorXd &b);
  void BuildProblemDPLinearX();
  void BuildProblemDPLinearY();
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_H_
