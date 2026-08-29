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
#include <array>
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

/**
 * A component pulled toward one absolute Y by a pseudo-net in the Y system.
 *
 * Used to express a stage band as part of the quadratic problem rather than by
 * repositioning cells after the solve. A rule that moves cells states the
 * answer and leaves the objective to disagree with it on the next iteration; a
 * term in the matrix makes the band one more thing the solve balances, so the
 * placement that comes out already accounts for it.
 *
 * The term is w*(y_i - target)^2, which contributes w to the diagonal and
 * w*target to the right-hand side. Being diagonal it cannot break the
 * positive-definiteness the conjugate-gradient solve depends on, which is why
 * the band is expressed as an attracting target and never as a repulsion.
 */
struct StageBandAnchor {
  int component_id = -1;
  double target_y = 0.0;
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

  /**
   * The anchor pseudo-net targets, one pair per component, and the
   * accumulated strength applied to them.
   *
   * Anchors are what tie each solve to the previous upper bound, and they
   * accumulate across iterations rather than being derived from the current
   * placement. An optimizer rebuilt mid-run therefore starts with none, and the
   * next solve is unconstrained -- measured on bd_pipeline as the lower bound
   * dropping from 63465 to 56975 in one iteration. Carrying this across a
   * rebuild is what makes an unchanged topology resume where it left off.
   *
   * Empty when the optimizer has not anchored yet.
   */
  struct AnchorState {
    std::vector<double> x;
    std::vector<double> y;
    /**
     * Accumulated anchor strength. It rises every iteration and is never
     * recomputed from one, so a rebuilt optimizer restarts it near zero and
     * anchors an order of magnitude weaker than the run had reached.
     */
    double alpha = 0.0;
    bool is_set = false;
  };
  virtual AnchorState ExportAnchorState() const { return {}; }
  virtual void ImportAnchorState(const AnchorState&) {}

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

  /** Replace the stage-band anchors consumed by the next optimization. */
  void SetStageBandAnchors(std::vector<StageBandAnchor> stage_band_anchors) {
    stage_band_anchors_ = std::move(stage_band_anchors);
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
  std::vector<StageBandAnchor> stage_band_anchors_;
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
   * Evaluate weighted X HPWL directly from the current solution vector.
   *
   * Net spans are evaluated in parallel, then summed in their original order
   * to preserve deterministic convergence decisions across thread counts.
   */
  double EvaluateWeightedHpwlX(int num_threads);
  /** Y counterpart of EvaluateWeightedHpwlX. */
  double EvaluateWeightedHpwlY(int num_threads);
  void PullComponentBackToRegion();

  void UpdateAnchorLocation();
  AnchorState ExportAnchorState() const override;
  void ImportAnchorState(const AnchorState& state) override;

  virtual void UpdateAnchorAlpha();
  virtual void BuildProblemWithAnchorX();
  virtual void BuildProblemWithAnchorY();
  /** Add translation-invariant physical row relationships to the Y problem. */
  void AddRelativeYConstraints();
  /** Fold the requested stage-band targets into the Y system. */
  void AddStageBandAnchors();
  void BackUpComponentLocation();
  /** Build and solve the X system with anchor pseudo-nets folded in. */
  void OptimizeHpwlXWithAnchor(int num_threads);
  /** Build and solve the Y system with anchor pseudo-nets folded in. */
  void OptimizeHpwlYWithAnchor(int num_threads);
  double OptimizeHpwl() override;

  double GetTime() override;
  void Close() override;

 protected:
  /** Compact pin data reused by solution-vector HPWL evaluations. */
  struct CachedSolutionPin {
    int component_id;
    bool is_movable;
    std::array<double, 8> offset_x;
    std::array<double, 8> offset_y;

    double OffsetX(ComponentOrient orient) const {
      return offset_x[static_cast<size_t>(orient)];
    }
    double OffsetY(ComponentOrient orient) const {
      return offset_y[static_cast<size_t>(orient)];
    }
  };

  /**** parameters for CG solver optimization configuration ****/
  // this is to make sure cg_tolerance is the same for different machines
  double cg_tolerance_ = 1e-35;
  // cg solver runs this amount of iterations to optimize the quadratic metric
  // everytime
  int cg_iteration_ = 9;
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
  std::vector<size_t> cached_net_pin_begin_;
  std::vector<CachedSolutionPin> cached_solution_pins_;
  std::vector<double> cached_net_weights_;
  std::vector<double> cached_net_inv_p_;
  std::vector<double> cached_component_x_;
  std::vector<double> cached_component_y_;
  /** Current pin coordinates, rebuilt in parallel for each net-model update. */
  std::vector<double> cached_pin_x_;
  std::vector<double> cached_pin_y_;
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
