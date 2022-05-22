/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_HPWL_OPTIMIZER_H_
#define DALI_PLACER_GLOBAL_PLACER_HPWL_OPTIMIZER_H_
#include <vector>

#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>

#include "dali/circuit/blkpairnets.h"
#include "dali/circuit/circuit.h"

namespace dali {

typedef Eigen::Index EgId;
typedef std::pair<EgId, EgId> PairEgId;
// declares a row-major sparse matrix type of double
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat;
// A triplet is a simple object representing a non-zero entry as the triplet: row index, column index, value.
typedef Eigen::Triplet<double> T;
// A "doublet" is a simple object representing a non-zero entry as: column index, value, for a given row index.
typedef IndexVal D;

class HpwlOptimizer {
 public:
  explicit HpwlOptimizer(Circuit *ckt_ptr);
  virtual ~HpwlOptimizer() = default;
  virtual void Initialize() = 0;
  void SetIteration(int cur_iter) { cur_iter_ = cur_iter; }
  virtual double QuadraticPlacement(double net_model_update_stop_criterion) = 0;
  virtual double QuadraticPlacementWithAnchor(double net_model_update_stop_criterion) = 0;
 protected:
  Circuit *ckt_ptr_ = nullptr;
  int cur_iter_ = 0;
};

class B2BHpwlOptimizer : public HpwlOptimizer {
 public:
  explicit B2BHpwlOptimizer(Circuit *ckt_ptr): HpwlOptimizer(ckt_ptr) {}
  ~B2BHpwlOptimizer() override = default;

  void UpdateEpsilon();
  void Initialize() override;

  void BuildProblemX();
  void BuildProblemY();
  bool IsSeriesConverge(
      std::vector<double> &data,
      int window_size,
      double tolerance
  );
  bool IsSeriesOscillate(std::vector<double> &data, int window_size);
  double OptimizeQuadraticMetricX(double cg_stop_criterion);
  double OptimizeQuadraticMetricY(double cg_stop_criterion);
  void PullBlockBackToRegion();
  double QuadraticPlacement(double net_model_update_stop_criterion) override;

  void UpdateAnchorLocation();
  void UpdateAnchorAlpha();
  void UpdateMaxMinX();
  void UpdateMaxMinY();
  void BuildProblemWithAnchorX();
  void BuildProblemWithAnchorY();
  void BackUpBlockLocation();
  double QuadraticPlacementWithAnchor(double net_model_update_stop_criterion) override;

  void DumpResult(std::string const &name_of_file);
 private:
  /**** storing the lower and upper bound of hpwl along x and y direction ****/
  std::vector<double> lower_bound_hpwlx_;
  std::vector<double> lower_bound_hpwly_;
  std::vector<double> lower_bound_hpwl_;

  /**** parameters for CG solver optimization configuration ****/
  // this is to make sure cg_tolerance is the same for different machines
  double cg_tolerance_ = 1e-35;
  // cg solver runs this amount of iterations to optimize the quadratic metric everytime
  int cg_iteration_ = 10;
  // cg solver runs at most this amount of iterations to optimize the quadratic metric, this number should be adaptive to circuit size
  int cg_iteration_max_num_ = 1000;
  // cg solver stops if the cost change is less than this value for 3 iterations
  double cg_stop_criterion_ = 0.0025;
  // stop update net model if the cost change is less than this value for 3 iterations
  double net_model_update_stop_criterion_ = 0.01;

  /**** two small positive numbers used to avoid divergence when calculating net weights ****/
  double epsilon_factor_ = 1.5;
  // this value will be set to 1/epsilon_factor_ times the average movable cell width
  double width_epsilon_ = 1e-5;
  // this value will be set to 1/epsilon_factor_ times the average movable cell height
  double height_epsilon_ = 1e-5;

  std::vector<int> Ax_row_size;
  std::vector<int> Ay_row_size;
  std::vector<std::vector<D>> ADx;
  std::vector<std::vector<D>> ADy;

  Eigen::VectorXd vx, vy;
  Eigen::VectorXd bx, by;
  SpMat Ax;
  SpMat Ay;
  Eigen::VectorXd x_anchor, y_anchor;
  Eigen::VectorXd x_anchor_weight, y_anchor_weight;
  bool x_anchor_set = false;
  bool y_anchor_set = false;
  std::vector<T> coefficients_x_;
  std::vector<T> coefficients_y_;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower | Eigen::Upper> cg_x_;
  Eigen::ConjugateGradient<SpMat, Eigen::Lower | Eigen::Upper> cg_y_;
  std::vector<std::vector<BlkPairNets *>> pair_connect;
  std::vector<BlkPairNets> diagonal_pair;
  std::vector<SpMat::InnerIterator> SpMat_diag_x;
  std::vector<SpMat::InnerIterator> SpMat_diag_y;

  int b2b_update_max_iteration_ = 50;
  int max_iter_ = 100;
  size_t net_ignore_threshold_ = 100;

  double tot_triplets_time_x = 0;
  double tot_triplets_time_y = 0;
  double tot_matrix_from_triplets_x = 0;
  double tot_matrix_from_triplets_y = 0;
  double tot_cg_solver_time_x = 0;
  double tot_cg_solver_time_y = 0;
  double tot_loc_update_time_x = 0;
  double tot_loc_update_time_y = 0;
  double tot_cg_time = 0;

  /**** anchor weight ****/
  // pseudo-net weight additional factor for anchor pseudo-net
  double alpha = 0.00;
  double alpha_step = 0.00;

  bool is_dump_ = false;
};

} // dali

#endif //DALI_PLACER_GLOBAL_PLACER_HPWL_OPTIMIZER_H_
