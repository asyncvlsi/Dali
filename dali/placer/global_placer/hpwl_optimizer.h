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
#include <Eigen/IterativeLinearSolvers>
#include <Eigen/Sparse>
#include <cstddef>
#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"

namespace dali {

/** Translation-invariant Y offset requested between two movable components. */
struct RelativeYConstraint {
  int first_component_id = -1;
  int second_component_id = -1;
  double offset = 0.0;
};

/** Index type used by Eigen sparse matrices. */
using SparseIndex = Eigen::Index;

/** Row-major sparse matrix used by the quadratic placement problem. */
using RowMajorSparseMatrix = Eigen::SparseMatrix<double, Eigen::RowMajor>;

/** One row, column, and value entry used to assemble a sparse matrix. */
using SparseTriplet = Eigen::Triplet<double>;

/** Abstract interface for global-placement HPWL optimizers. */
class HpwlOptimizer {
 public:
  HpwlOptimizer(Circuit* ckt_ptr, int num_threads);
  virtual ~HpwlOptimizer() = default;

  /** Prepare optimizer state before iterative optimization. */
  virtual void Initialize() = 0;

  /** Set number of worker threads. */
  void SetNumThreads(int num_threads) { num_threads_ = num_threads; }

  /** Set current global-placement iteration. */
  void SetIteration(int cur_iter) { cur_iter_ = cur_iter; }

  /** Replace the relative-Y constraints consumed by the next optimization. */
  void SetRelativeYConstraints(
      std::vector<RelativeYConstraint> relative_y_constraints) {
    relative_y_constraints_ = std::move(relative_y_constraints);
  }

  /** Ignore nets at or above this pin count in the quadratic wire model. */
  void SetNetIgnoreThreshold(int net_ignore_threshold);

  /** Optimize component locations and return the resulting HPWL estimate. */
  virtual double OptimizeHpwl() = 0;

  /** Return total optimizer runtime in seconds. */
  virtual double GetTime() = 0;

  /** Release optimizer resources. */
  virtual void Close() = 0;

  /** Return lower-bound HPWL history. */
  std::vector<double>& GetHpwls() { return lower_bound_hpwl_; }

  /** Return x lower-bound HPWL history. */
  std::vector<double>& GetHpwlsX() { return lower_bound_hpwl_x_; }

  /** Return y lower-bound HPWL history. */
  std::vector<double>& GetHpwlsY() { return lower_bound_hpwl_y_; }

  /** Enable or disable intermediate placement dumps. */

 protected:
  Circuit* ckt_ptr_ = nullptr;
  int cur_iter_ = 0;
  int num_threads_ = 1;
  std::vector<double> lower_bound_hpwl_;
  std::vector<double> lower_bound_hpwl_x_;
  std::vector<double> lower_bound_hpwl_y_;

  // Stop updating the net model if cost change is below this value for 3
  // iterations.
  double net_model_update_stop_criterion_ = 0.01;

  size_t net_ignore_threshold_ = 100;
  std::vector<RelativeYConstraint> relative_y_constraints_;
};

/** Bound-to-bound quadratic HPWL optimizer. */
class BoundToBoundHpwlOptimizer : public HpwlOptimizer {
 public:
  BoundToBoundHpwlOptimizer(Circuit* ckt_ptr, int num_threads)
      : HpwlOptimizer(ckt_ptr, num_threads) {}
  ~BoundToBoundHpwlOptimizer() override = default;

  void UpdateEpsilon();
  void Initialize() override;

  virtual void BuildProblemX();
  virtual void BuildProblemY();
  bool IsSeriesConverged(std::vector<double>& data, int window_size,
                         double tolerance);
  /** Whether an HPWL series is oscillating rather than converging. */
  bool IsSeriesOscillate(std::vector<double>& data, int window_size);
  /** Solve the X system by conjugate gradient; loose tolerance since the
   * problem is rebuilt next iteration. @return the resulting X wirelength. */
  virtual double OptimizeQuadraticMetricX(double cg_stop_criterion);
  /** Solve the Y system by conjugate gradient. */
  virtual double OptimizeQuadraticMetricY(double cg_stop_criterion);
  /**
   * Evaluate weighted X HPWL in parallel and sum nets in their original order.
   *
   * Keeping the final reduction serial preserves deterministic convergence
   * decisions across thread counts.
   */
  double EvaluateWeightedHpwlX(int num_threads);
  /** Y counterpart of EvaluateWeightedHpwlX. */
  double EvaluateWeightedHpwlY(int num_threads);
  void PullComponentBackToRegion();

  void UpdateAnchorLocation();
  virtual void UpdateAnchorAlpha();
  void UpdateMaxMinX();
  void UpdateMaxMinY();
  virtual void BuildProblemWithAnchorX();
  virtual void BuildProblemWithAnchorY();
  /** Add translation-invariant physical row relationships to the Y problem. */
  void AddRelativeYConstraints();
  void BackUpComponentLocation();
  /** Build and solve the X system with anchor pseudo-nets folded in. */
  void OptimizeHpwlXWithAnchor(int num_threads);
  /** Build and solve the Y system with anchor pseudo-nets folded in. */
  void OptimizeHpwlYWithAnchor(int num_threads);
  double OptimizeHpwl() override;

  double GetTime() override;
  void Close() override;

 protected:
  /**** parameters for CG solver optimization configuration ****/
  // this is to make sure cg_tolerance is the same for different machines
  double cg_tolerance_ = 1e-35;
  // cg solver runs this amount of iterations to optimize the quadratic metric
  // everytime
  int cg_iteration_ = 10;
  // cg solver runs at most this amount of iterations to optimize the quadratic
  // metric, this number should be adaptive to circuit size
  int cg_iteration_max_num_ = 1000;
  // cg solver stops if the cost change is less than this value for 3 iterations
  double cg_stop_criterion_ = 0.0025;
  // stop update net model if the cost change is less than this value for 3
  // iterations
  double net_model_update_stop_criterion_ = 0.01;

  /**** two small positive numbers used to avoid divergence when calculating net
   * weights ****/
  double epsilon_factor_ = 1.5;
  // this value will be set to 1/epsilon_factor_ times the average movable
  // component width
  double width_epsilon_ = 1e-5;
  // this value will be set to 1/epsilon_factor_ times the average movable
  // component height
  double height_epsilon_ = 1e-5;
  // early stop threshold
  double hpwl_early_stop_threshold_ = 1.0;

  Eigen::VectorXd vx, vy;
  Eigen::VectorXd bx, by;
  RowMajorSparseMatrix Ax;
  RowMajorSparseMatrix Ay;
  Eigen::VectorXd x_anchor, y_anchor;
  Eigen::VectorXd x_anchor_weight, y_anchor_weight;
  bool x_anchor_set = false;
  bool y_anchor_set = false;
  std::vector<SparseTriplet> coefficients_x_;
  std::vector<SparseTriplet> coefficients_y_;
  std::vector<double> net_hpwl_x_;
  std::vector<double> net_hpwl_y_;
  Eigen::ConjugateGradient<RowMajorSparseMatrix, Eigen::Lower | Eigen::Upper>
      cg_x_;
  Eigen::ConjugateGradient<RowMajorSparseMatrix, Eigen::Lower | Eigen::Upper>
      cg_y_;

  int b2b_update_max_iteration_ = 50;

  double tot_triplets_time_x = 0;
  double tot_triplets_time_y = 0;
  double tot_matrix_from_triplets_x = 0;
  double tot_matrix_from_triplets_y = 0;
  double tot_cg_solver_time_x = 0;
  double tot_cg_solver_time_y = 0;
  double tot_cg_compute_time_x = 0;
  double tot_cg_compute_time_y = 0;
  double tot_cg_solve_time_x = 0;
  double tot_cg_solve_time_y = 0;
  double tot_intermediate_loc_update_time_x = 0;
  double tot_intermediate_loc_update_time_y = 0;
  double tot_hpwl_evaluation_time_x = 0;
  double tot_hpwl_evaluation_time_y = 0;
  double tot_loc_update_time_x = 0;
  double tot_loc_update_time_y = 0;
  double tot_cg_time = 0;

  /**** anchor weight ****/
  // pseudo-net weight additional factor for anchor pseudo-net
  double alpha = 0.00;
  double alpha_step = 0.00;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_HPWL_OPTIMIZER_H_
