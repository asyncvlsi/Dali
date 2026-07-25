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

/**
 * @file
 * Solves for the wirelength lower bound of a global placement iteration.
 *
 * Wirelength is not differentiable, so each multi-pin net is modelled as a set
 * of pairwise connections weighted so their quadratic cost approximates HPWL --
 * the bound-to-bound model, which weights each pin against the net's extreme
 * pins. X and Y separate into two independent systems, each solved by conjugate
 * gradient.
 *
 * The result is a lower bound: cells overlap freely, because nothing here knows
 * about density. Spreading supplies the matching upper bound, and anchor
 * pseudo-nets pull the two together over successive iterations, their strength
 * rising on the schedule in UpdateAnchorAlpha.
 */

#include "hpwl_optimizer.h"

#include <algorithm>
#include <cfloat>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"

namespace dali {

HpwlOptimizer::HpwlOptimizer(Circuit* ckt_ptr, int num_threads) {
  DaliExpects(ckt_ptr != nullptr, "Circuit is a nullptr?");
  ckt_ptr_ = ckt_ptr;
  DaliExpects(num_threads >= 1, "Number of threads less than 1?");
  num_threads_ = num_threads;
}

void HpwlOptimizer::SetNetIgnoreThreshold(int net_ignore_threshold) {
  DaliExpects(net_ignore_threshold > 1,
              "Net ignore threshold must be greater than one");
  net_ignore_threshold_ = static_cast<size_t>(net_ignore_threshold);
}

/****
 * @brief During quadratic placement, net weights are computed by dividing the
 * distance between two pins. To improve numerical stability, a small number is
 * added to this distance.
 *
 * In our implementation, this small number is around the average movable
 * component width for x and height for y.
 */
void BoundToBoundHpwlOptimizer::UpdateEpsilon() {
  width_epsilon_ = ckt_ptr_->AverageMovableComponentWidth() * epsilon_factor_;
  height_epsilon_ = ckt_ptr_->AverageMovableComponentHeight() * epsilon_factor_;
}

/****
 * @brief Initialize variables for the conjugate gradient linear solver
 */
void BoundToBoundHpwlOptimizer::Initialize() {
  // set a small value for net weight dividend to improve numerical stability
  UpdateEpsilon();

  lower_bound_hpwl_x_.clear();
  lower_bound_hpwl_y_.clear();
  lower_bound_hpwl_.clear();

  size_t sz = ckt_ptr_->Components().size();
  SparseIndex eigen_sz =
      static_cast<SparseIndex>(ckt_ptr_->Components().size());
  vx.resize(eigen_sz);
  vy.resize(eigen_sz);
  bx.resize(eigen_sz);
  by.resize(eigen_sz);
  Ax.resize(eigen_sz, eigen_sz);
  Ay.resize(eigen_sz, eigen_sz);
  x_anchor.resize(eigen_sz);
  y_anchor.resize(eigen_sz);
  x_anchor_weight.resize(eigen_sz);
  y_anchor_weight.resize(eigen_sz);
  net_hpwl_x_.resize(ckt_ptr_->Nets().size());
  net_hpwl_y_.resize(ckt_ptr_->Nets().size());
  cached_component_x_.resize(sz);
  cached_component_y_.resize(sz);

  // Global placement changes locations but not component orientations or net
  // topology. Flatten the pin data used by every convergence check so those
  // checks avoid repeatedly chasing Component and Pin pointers.
  cached_net_pin_begin_.clear();
  cached_solution_pins_.clear();
  cached_net_weights_.clear();
  cached_net_inv_p_.clear();
  cached_net_pin_begin_.reserve(ckt_ptr_->Nets().size() + 1);
  cached_net_weights_.reserve(ckt_ptr_->Nets().size());
  cached_net_inv_p_.reserve(ckt_ptr_->Nets().size());
  cached_net_pin_begin_.push_back(0);
  for (Net& net : ckt_ptr_->Nets()) {
    cached_net_weights_.push_back(net.Weight());
    cached_net_inv_p_.push_back(net.InvP());
    for (NetPin& pin : net.ComponentPins()) {
      cached_solution_pins_.push_back({pin.ComponentId(),
                                       pin.ComponentPtr()->IsMovable(),
                                       pin.OffsetX(), pin.OffsetY()});
    }
    cached_net_pin_begin_.push_back(cached_solution_pins_.size());
  }
  cached_pin_x_.resize(cached_solution_pins_.size());
  cached_pin_y_.resize(cached_solution_pins_.size());

  cg_x_.setMaxIterations(cg_iteration_);
  cg_x_.setTolerance(cg_tolerance_);
  cg_y_.setMaxIterations(cg_iteration_);
  cg_y_.setTolerance(cg_tolerance_);

  size_t coefficient_size = 0;
  auto& nets = ckt_ptr_->Nets();
  for (auto& net : nets) {
    size_t net_sz = net.PinCnt();
    // if a net has size n, then in total, there will be (2(n-2)+1)*4 non-zero
    // entries for the matrix
    if (net_sz > 1) {
      coefficient_size += (2 * (net_sz - 2) + 1) * 4;
    }
  }
  // this is to reserve space for anchor, because each component may need an
  // anchor
  coefficient_size += sz;
  // this is to reserve space for anchor in the center of the placement region
  coefficient_size += sz;
  coefficients_x_.reserve(coefficient_size);
  Ax.reserve(static_cast<SparseIndex>(coefficient_size));
  coefficients_y_.reserve(coefficient_size);
  Ay.reserve(static_cast<SparseIndex>(coefficient_size));
}

/**
 * Build the X system for this iteration from the current placement.
 *
 * Each net contributes pairwise terms under the bound-to-bound model, so the
 * matrix depends on which pins are currently extreme and must be rebuilt as
 * cells move. Anchor pseudo-nets and a weak pull toward the region centre are
 * folded in here, not applied afterwards. Nets at or above the ignore threshold
 * are skipped entirely.
 */
void BoundToBoundHpwlOptimizer::BuildProblemX() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Component>& components = ckt_ptr_->Components();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);
  int sz = static_cast<int>(bx.size());
#pragma omp parallel for num_threads(num_threads_) schedule(static)
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
    cached_component_x_[i] = components[i].LLX();
  }
#pragma omp parallel for num_threads(num_threads_) schedule(static)
  for (int i = 0; i < static_cast<int>(cached_solution_pins_.size()); ++i) {
    const CachedSolutionPin& pin = cached_solution_pins_[i];
    cached_pin_x_[i] = cached_component_x_[pin.component_id] + pin.offset_x;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x =
      (ckt_ptr_->RegionLLX() + ckt_ptr_->RegionURX()) / 2.0 * center_weight;
  // double decay_length = decay_factor * ckt_ptr_->AverageComponentHeight();

  int net_count = static_cast<int>(cached_net_weights_.size());
  for (int net_index = 0; net_index < net_count; ++net_index) {
    size_t pin_begin = cached_net_pin_begin_[net_index];
    size_t pin_end = cached_net_pin_begin_[net_index + 1];
    size_t pin_count = pin_end - pin_begin;
    if (pin_count <= 1 || pin_count >= net_ignore_threshold_) continue;

    size_t max_pin_index = pin_begin;
    size_t min_pin_index = pin_begin;
    double pin_loc_max = -DBL_MAX;
    double pin_loc_min = DBL_MAX;
    for (size_t pin_index = pin_begin; pin_index < pin_end; ++pin_index) {
      double pin_loc = cached_pin_x_[pin_index];
      if (pin_loc_max < pin_loc) {
        pin_loc_max = pin_loc;
        max_pin_index = pin_index;
      }
      if (pin_loc_min > pin_loc) {
        pin_loc_min = pin_loc;
        min_pin_index = pin_index;
      }
    }
    if (pin_loc_max == pin_loc_min) {
      max_pin_index = pin_begin;
      min_pin_index = pin_begin + 1;
    }

    double inv_p = cached_net_inv_p_[net_index];
    const CachedSolutionPin& max_pin = cached_solution_pins_[max_pin_index];
    int max_component_id = max_pin.component_id;
    bool is_movable_max = max_pin.is_movable;
    double offset_max = max_pin.offset_x;

    const CachedSolutionPin& min_pin = cached_solution_pins_[min_pin_index];
    int min_component_id = min_pin.component_id;
    bool is_movable_min = min_pin.is_movable;
    double offset_min = min_pin.offset_x;

    for (size_t pin_index = pin_begin; pin_index < pin_end; ++pin_index) {
      const CachedSolutionPin& pin = cached_solution_pins_[pin_index];
      int component_id = pin.component_id;
      double pin_loc = cached_pin_x_[pin_index];
      bool is_movable = pin.is_movable;
      double offset = pin.offset_x;

      if (component_id != max_component_id) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + width_epsilon_);
        // decay_length)); weight *= weight_adjust;
        if (!is_movable && is_movable_max) {
          bx[max_component_id] += (pin_loc - offset_max) * weight;
          coefficients_x_.emplace_back(max_component_id, max_component_id,
                                       weight);
        } else if (is_movable && !is_movable_max) {
          bx[component_id] += (pin_loc_max - offset) * weight;
          coefficients_x_.emplace_back(component_id, component_id, weight);
        } else if (is_movable && is_movable_max) {
          coefficients_x_.emplace_back(component_id, component_id, weight);
          coefficients_x_.emplace_back(max_component_id, max_component_id,
                                       weight);
          coefficients_x_.emplace_back(component_id, max_component_id, -weight);
          coefficients_x_.emplace_back(max_component_id, component_id, -weight);
          double offset_diff = (offset_max - offset) * weight;
          bx[component_id] += offset_diff;
          bx[max_component_id] -= offset_diff;
        }
      }

      if ((component_id != max_component_id) &&
          (component_id != min_component_id)) {
        double distance = std::fabs(pin_loc - pin_loc_min);
        double weight = inv_p / (distance + width_epsilon_);
        // weight *= weight_adjust;
        if (!is_movable && is_movable_min) {
          bx[min_component_id] += (pin_loc - offset_min) * weight;
          coefficients_x_.emplace_back(min_component_id, min_component_id,
                                       weight);
        } else if (is_movable && !is_movable_min) {
          bx[component_id] += (pin_loc_min - offset) * weight;
          coefficients_x_.emplace_back(component_id, component_id, weight);
        } else if (is_movable && is_movable_min) {
          coefficients_x_.emplace_back(component_id, component_id, weight);
          coefficients_x_.emplace_back(min_component_id, min_component_id,
                                       weight);
          coefficients_x_.emplace_back(component_id, min_component_id, -weight);
          coefficients_x_.emplace_back(min_component_id, component_id, -weight);
          double offset_diff = (offset_min - offset) * weight;
          bx[component_id] += offset_diff;
          bx[min_component_id] -= offset_diff;
        }
      }
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (components[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = components[i].LLX();
    } else {
      if (components[i].LLX() < ckt_ptr_->RegionLLX() ||
          components[i].URX() > ckt_ptr_->RegionURX()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(coefficients_capacity != coefficients_x_.capacity(),
            "WARNING: x coefficients capacity changed!\n"
                << "\told capacity: " << coefficients_capacity << "\n"
                << "\tnew capacity: " << coefficients_x_.size());

  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}

/**
 * Build the Y system for this iteration, the Y counterpart of BuildProblemX.
 *
 * Y additionally carries relative-position constraints between components that
 * X does not, so the two build functions are not mirror images.
 */
void BoundToBoundHpwlOptimizer::BuildProblemY() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Component>& components = ckt_ptr_->Components();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);
  int sz = static_cast<int>(by.size());
#pragma omp parallel for num_threads(num_threads_) schedule(static)
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
    cached_component_y_[i] = components[i].LLY();
  }
#pragma omp parallel for num_threads(num_threads_) schedule(static)
  for (int i = 0; i < static_cast<int>(cached_solution_pins_.size()); ++i) {
    const CachedSolutionPin& pin = cached_solution_pins_[i];
    cached_pin_y_[i] = cached_component_y_[pin.component_id] + pin.offset_y;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y =
      (ckt_ptr_->RegionLLY() + ckt_ptr_->RegionURY()) / 2.0 * center_weight;
  // double decay_length = decay_factor * ckt_ptr_->AverageComponentHeight();

  int net_count = static_cast<int>(cached_net_weights_.size());
  for (int net_index = 0; net_index < net_count; ++net_index) {
    size_t pin_begin = cached_net_pin_begin_[net_index];
    size_t pin_end = cached_net_pin_begin_[net_index + 1];
    size_t pin_count = pin_end - pin_begin;
    if (pin_count <= 1 || pin_count >= net_ignore_threshold_) continue;

    size_t max_pin_index = pin_begin;
    size_t min_pin_index = pin_begin;
    double pin_loc_max = -DBL_MAX;
    double pin_loc_min = DBL_MAX;
    for (size_t pin_index = pin_begin; pin_index < pin_end; ++pin_index) {
      double pin_loc = cached_pin_y_[pin_index];
      if (pin_loc_max < pin_loc) {
        pin_loc_max = pin_loc;
        max_pin_index = pin_index;
      }
      if (pin_loc_min > pin_loc) {
        pin_loc_min = pin_loc;
        min_pin_index = pin_index;
      }
    }
    if (pin_loc_max == pin_loc_min) {
      max_pin_index = pin_begin;
      min_pin_index = pin_begin + 1;
    }

    double inv_p = cached_net_inv_p_[net_index];
    const CachedSolutionPin& max_pin = cached_solution_pins_[max_pin_index];
    int max_component_id = max_pin.component_id;
    bool is_movable_max = max_pin.is_movable;
    double offset_max = max_pin.offset_y;

    const CachedSolutionPin& min_pin = cached_solution_pins_[min_pin_index];
    int min_component_id = min_pin.component_id;
    bool is_movable_min = min_pin.is_movable;
    double offset_min = min_pin.offset_y;

    for (size_t pin_index = pin_begin; pin_index < pin_end; ++pin_index) {
      const CachedSolutionPin& pin = cached_solution_pins_[pin_index];
      int component_id = pin.component_id;
      double pin_loc = cached_pin_y_[pin_index];
      bool is_movable = pin.is_movable;
      double offset = pin.offset_y;

      if (component_id != max_component_id) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + height_epsilon_);
        // decay_length)); weight *= weight_adjust;
        if (!is_movable && is_movable_max) {
          by[max_component_id] += (pin_loc - offset_max) * weight;
          coefficients_y_.emplace_back(max_component_id, max_component_id,
                                       weight);
        } else if (is_movable && !is_movable_max) {
          by[component_id] += (pin_loc_max - offset) * weight;
          coefficients_y_.emplace_back(component_id, component_id, weight);
        } else if (is_movable && is_movable_max) {
          coefficients_y_.emplace_back(component_id, component_id, weight);
          coefficients_y_.emplace_back(max_component_id, max_component_id,
                                       weight);
          coefficients_y_.emplace_back(component_id, max_component_id, -weight);
          coefficients_y_.emplace_back(max_component_id, component_id, -weight);
          double offset_diff = (offset_max - offset) * weight;
          by[component_id] += offset_diff;
          by[max_component_id] -= offset_diff;
        }
      }

      if ((component_id != max_component_id) &&
          (component_id != min_component_id)) {
        double distance = std::fabs(pin_loc - pin_loc_min);
        double weight = inv_p / (distance + height_epsilon_);
        // weight *= weight_adjust;
        if (!is_movable && is_movable_min) {
          by[min_component_id] += (pin_loc - offset_min) * weight;
          coefficients_y_.emplace_back(min_component_id, min_component_id,
                                       weight);
        } else if (is_movable && !is_movable_min) {
          by[component_id] += (pin_loc_min - offset) * weight;
          coefficients_y_.emplace_back(component_id, component_id, weight);
        } else if (is_movable && is_movable_min) {
          coefficients_y_.emplace_back(component_id, component_id, weight);
          coefficients_y_.emplace_back(min_component_id, min_component_id,
                                       weight);
          coefficients_y_.emplace_back(component_id, min_component_id, -weight);
          coefficients_y_.emplace_back(min_component_id, component_id, -weight);
          double offset_diff = (offset_min - offset) * weight;
          by[component_id] += offset_diff;
          by[min_component_id] -= offset_diff;
        }
      }
    }
  }
  // add the diagonal non-zero element for fixed components
  for (int i = 0; i < sz; ++i) {
    if (components[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = components[i].LLY();
    } else {
      if (components[i].LLY() < ckt_ptr_->RegionLLY() ||
          components[i].URY() > ckt_ptr_->RegionURY()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(coefficients_capacity != coefficients_y_.capacity(),
            "WARNING: y coefficients capacity changed!\n"
                << "\told capacity: " << coefficients_capacity << "\n"
                << "\tnew capacity: " << coefficients_y_.size());

  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

bool BoundToBoundHpwlOptimizer::IsSeriesConverged(std::vector<double>& data,
                                                  int window_size,
                                                  double tolerance) {
  int sz = (int)data.size();
  if (sz < window_size) {
    return false;
  }
  double max_val = -DBL_MAX;
  double min_val = DBL_MAX;
  for (int i = 0; i < window_size; ++i) {
    max_val = std::max(max_val, data[sz - 1 - i]);
    min_val = std::min(min_val, data[sz - 1 - i]);
  }
  DaliExpects(max_val >= 0 && min_val >= 0,
              "Do not support negative data series!");
  if (max_val < 1e-10 && min_val <= 1e-10) {
    return true;
  }
  if (min_val <= 1e-10) {
    return false;
  }
  double ratio = max_val / min_val - 1;
  return ratio < tolerance;
}

/****
 * Returns if the given series of data is oscillating or not.
 * We will only look at the last several data points @param length.
 * ****/
bool BoundToBoundHpwlOptimizer::IsSeriesOscillate(std::vector<double>& data,
                                                  int window_size) {
  // if the given length is too short, we cannot know whether it is oscillating
  // or not.
  if (window_size < 3) return false;

  // if the given data series is short than the length, we cannot know whether
  // it is oscillating or not.
  int sz = (int)data.size();
  if (sz < window_size) {
    return false;
  }

  // this vector keeps track of the increasing trend (true) and descreasing
  std::vector<bool> trend(window_size - 1, false);
  for (int i = 0; i < window_size - 1; ++i) {
    trend[i] = data[sz - 1 - i] > data[sz - 2 - i];
  }
  std::reverse(trend.begin(), trend.end());

  bool is_oscillate = true;
  for (int i = 0; i < window_size - 2; ++i) {
    if (trend[i] == trend[i + 1]) {
      is_oscillate = false;
      break;
    }
  }

  return is_oscillate;
}

/**
 * Solve the current X system by conjugate gradient to `cg_stop_criterion`.
 *
 * The loose stopping tolerance is deliberate: an exact solve is wasted when the
 * problem is rebuilt next iteration. Returns the resulting X wirelength.
 */
double BoundToBoundHpwlOptimizer::OptimizeQuadraticMetricX(
    double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  Ax.setFromTriplets(coefficients_x_.begin(), coefficients_x_.end());
  elapsed_time.RecordEndTime();
  tot_matrix_from_triplets_x += elapsed_time.GetWallTime();

  int sz = static_cast<int>(vx.size());
  std::vector<Component>& components = ckt_ptr_->Components();

  ElapsedTime solver_elapsed_time;
  solver_elapsed_time.RecordStartTime();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  int solver_threads = std::max(1, num_threads_ / 2);

  ElapsedTime phase_elapsed_time;
  phase_elapsed_time.RecordStartTime();
  cg_x_.compute(Ax);  // Ax * vx = bx
  phase_elapsed_time.RecordEndTime();
  tot_cg_compute_time_x += phase_elapsed_time.GetWallTime();

  for (int i = 0; i < max_rounds; ++i) {
    phase_elapsed_time.RecordStartTime();
    vx = cg_x_.solveWithGuess(bx, vx);
    phase_elapsed_time.RecordEndTime();
    tot_cg_solve_time_x += phase_elapsed_time.GetWallTime();

    phase_elapsed_time.RecordStartTime();
    double evaluate_result = EvaluateWeightedHpwlX(solver_threads);
    phase_elapsed_time.RecordEndTime();
    tot_hpwl_evaluation_time_x += phase_elapsed_time.GetWallTime();

    eval_history.push_back(evaluate_result);
    if (evaluate_result < hpwl_early_stop_threshold_) {
      break;
    }
    // evaluate_result);
    if (eval_history.size() >= 3) {
      bool is_converge = IsSeriesConverged(eval_history, 3, cg_stop_criterion);
      bool is_oscillate = IsSeriesOscillate(eval_history, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        LOG(trace) << "oscillation detected\n";
        break;
      }
    }
  }
  LOG(trace) << "      Metric optimization in X, sequence: " << eval_history
             << "\n";
  solver_elapsed_time.RecordEndTime();
  tot_cg_solver_time_x += solver_elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
#pragma omp parallel for num_threads(solver_threads) schedule(static)
  for (int num = 0; num < sz; ++num) {
    components[num].SetLLX(vx[num]);
  }
  elapsed_time.RecordEndTime();
  tot_loc_update_time_x += elapsed_time.GetWallTime();

  DaliExpects(
      !eval_history.empty(),
      "Cannot return a valid value because the result is not evaluated!");
  return eval_history.back();
}

/**
 * Solve the current Y system by conjugate gradient. The Y counterpart of
 * OptimizeQuadraticMetricX.
 */
double BoundToBoundHpwlOptimizer::OptimizeQuadraticMetricY(
    double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  Ay.setFromTriplets(coefficients_y_.begin(), coefficients_y_.end());
  elapsed_time.RecordEndTime();
  tot_matrix_from_triplets_y += elapsed_time.GetWallTime();

  int sz = static_cast<int>(vy.size());
  std::vector<Component>& component_list = ckt_ptr_->Components();

  ElapsedTime solver_elapsed_time;
  solver_elapsed_time.RecordStartTime();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  int solver_threads = std::max(1, num_threads_ / 2);

  ElapsedTime phase_elapsed_time;
  phase_elapsed_time.RecordStartTime();
  cg_y_.compute(Ay);
  phase_elapsed_time.RecordEndTime();
  tot_cg_compute_time_y += phase_elapsed_time.GetWallTime();

  for (int i = 0; i < max_rounds; ++i) {
    phase_elapsed_time.RecordStartTime();
    vy = cg_y_.solveWithGuess(by, vy);
    phase_elapsed_time.RecordEndTime();
    tot_cg_solve_time_y += phase_elapsed_time.GetWallTime();

    phase_elapsed_time.RecordStartTime();
    double evaluate_result = EvaluateWeightedHpwlY(solver_threads);
    phase_elapsed_time.RecordEndTime();
    tot_hpwl_evaluation_time_y += phase_elapsed_time.GetWallTime();

    eval_history.push_back(evaluate_result);
    if (evaluate_result < hpwl_early_stop_threshold_) {
      break;
    }
    // evaluate_result);
    if (eval_history.size() >= 3) {
      bool is_converge = IsSeriesConverged(eval_history, 3, cg_stop_criterion);
      bool is_oscillate = IsSeriesOscillate(eval_history, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        LOG(trace) << "oscillation detected\n";
        break;
      }
    }
  }
  LOG(trace) << "      Metric optimization in Y, sequence: " << eval_history
             << "\n";
  solver_elapsed_time.RecordEndTime();
  tot_cg_solver_time_y += solver_elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
#pragma omp parallel for num_threads(solver_threads) schedule(static)
  for (int num = 0; num < sz; ++num) {
    component_list[num].SetLLY(vy[num]);
  }
  elapsed_time.RecordEndTime();
  tot_loc_update_time_y += elapsed_time.GetWallTime();

  DaliExpects(
      !eval_history.empty(),
      "Cannot return a valid value because the result is not evaluated!");
  return eval_history.back();
}

double BoundToBoundHpwlOptimizer::EvaluateWeightedHpwlX(int num_threads) {
  int net_count = static_cast<int>(cached_net_weights_.size());

#pragma omp parallel for num_threads(num_threads) schedule(static)
  for (int i = 0; i < net_count; ++i) {
    size_t pin_begin = cached_net_pin_begin_[i];
    size_t pin_end = cached_net_pin_begin_[i + 1];
    if (pin_end - pin_begin <= 1) {
      net_hpwl_x_[i] = 0;
      continue;
    }

    double min_x = DBL_MAX;
    double max_x = -DBL_MAX;
    for (size_t pin_index = pin_begin; pin_index < pin_end; ++pin_index) {
      const CachedSolutionPin& pin = cached_solution_pins_[pin_index];
      double pin_x = vx[pin.component_id] + pin.offset_x;
      min_x = std::min(min_x, pin_x);
      max_x = std::max(max_x, pin_x);
    }
    net_hpwl_x_[i] = (max_x - min_x) * cached_net_weights_[i];
  }

  double hpwl = 0;
  for (double net_hpwl : net_hpwl_x_) {
    hpwl += net_hpwl;
  }
  return hpwl * ckt_ptr_->GridValueX();
}

double BoundToBoundHpwlOptimizer::EvaluateWeightedHpwlY(int num_threads) {
  int net_count = static_cast<int>(cached_net_weights_.size());

#pragma omp parallel for num_threads(num_threads) schedule(static)
  for (int i = 0; i < net_count; ++i) {
    size_t pin_begin = cached_net_pin_begin_[i];
    size_t pin_end = cached_net_pin_begin_[i + 1];
    if (pin_end - pin_begin <= 1) {
      net_hpwl_y_[i] = 0;
      continue;
    }

    double min_y = DBL_MAX;
    double max_y = -DBL_MAX;
    for (size_t pin_index = pin_begin; pin_index < pin_end; ++pin_index) {
      const CachedSolutionPin& pin = cached_solution_pins_[pin_index];
      double pin_y = vy[pin.component_id] + pin.offset_y;
      min_y = std::min(min_y, pin_y);
      max_y = std::max(max_y, pin_y);
    }
    net_hpwl_y_[i] = (max_y - min_y) * cached_net_weights_[i];
  }

  double hpwl = 0;
  for (double net_hpwl : net_hpwl_y_) {
    hpwl += net_hpwl;
  }
  return hpwl * ckt_ptr_->GridValueY();
}

/** Add a weak pull keeping components inside the placement region. */
void BoundToBoundHpwlOptimizer::PullComponentBackToRegion() {
  int sz = static_cast<int>(vx.size());
  std::vector<Component>& component_list = ckt_ptr_->Components();
  double region_llx = ckt_ptr_->RegionLLX();
  double region_urx = ckt_ptr_->RegionURX();
  double region_lly = ckt_ptr_->RegionLLY();
  double region_ury = ckt_ptr_->RegionURY();
#pragma omp parallel num_threads(num_threads_) default(none) \
    shared(component_list, sz, region_llx, region_urx, region_lly, region_ury)
  {
#pragma omp for
    for (int i = 0; i < sz; ++i) {
      if (component_list[i].IsMovable()) {
        if (vx[i] < region_llx) {
          vx[i] = region_llx;
        }
        double component_hi_bound_x = region_urx - component_list[i].Width();
        if (vx[i] > component_hi_bound_x) {
          vx[i] = component_hi_bound_x;
        }

        if (vy[i] < region_lly) {
          vy[i] = region_lly;
        }
        double component_hi_bound_y = region_ury - component_list[i].Height();
        if (vy[i] > component_hi_bound_y) {
          vy[i] = component_hi_bound_y;
        }
      }
    }

#pragma omp for
    for (int i = 0; i < sz; ++i) {
      component_list[i].SetLoc(vx[i], vy[i]);
    }
  }
}

void BoundToBoundHpwlOptimizer::UpdateAnchorLocation() {
  if (cur_iter_ == 0) return;
  std::vector<Component>& component_list = ckt_ptr_->Components();
  int sz = static_cast<int>(component_list.size());

  for (int i = 0; i < sz; ++i) {
    double tmp_loc_x = x_anchor[i];
    x_anchor[i] = component_list[i].LLX();
    component_list[i].SetLLX(tmp_loc_x);

    double tmp_loc_y = y_anchor[i];
    y_anchor[i] = component_list[i].LLY();
    component_list[i].SetLLY(tmp_loc_y);
  }

  x_anchor_set = true;
  y_anchor_set = true;
}

void BoundToBoundHpwlOptimizer::UpdateAnchorAlpha() {
  if (cur_iter_ == 0) {
    alpha_step = 0;
  } else if (0 < cur_iter_ && cur_iter_ < 5) {
    alpha_step = 0.005;
  } else if (cur_iter_ < 10) {
    alpha_step = 0.01;
  } else if (cur_iter_ < 15) {
    alpha_step = 0.02;
  } else {
    alpha_step = 0.04;
  }
  alpha += alpha_step;
  LOG(info) << "    anchor alpha: " << alpha << " (step " << alpha_step
            << ")\n";
}

void BoundToBoundHpwlOptimizer::BuildProblemWithAnchorX() {
  BuildProblemX();

  if (cur_iter_ == 0) return;
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Component>& component_list = ckt_ptr_->Components();
  int sz = static_cast<int>(component_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (component_list[i].IsFixed()) continue;
    pin_loc0 = component_list[i].LLX();
    pin_loc1 = x_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    bx[i] += pin_loc1 * weight;
    coefficients_x_.emplace_back(SparseTriplet(i, i, weight));
  }
  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}
/** Build the Y system including anchor pseudo-nets. */
void BoundToBoundHpwlOptimizer::BuildProblemWithAnchorY() {
  BuildProblemY();

  if (cur_iter_ == 0) return;
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Component>& component_list = ckt_ptr_->Components();
  int sz = static_cast<int>(component_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (component_list[i].IsFixed()) continue;
    pin_loc0 = component_list[i].LLY();
    pin_loc1 = y_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + height_epsilon_);
    by[i] += pin_loc1 * weight;
    coefficients_y_.emplace_back(SparseTriplet(i, i, weight));
  }
  /** Fold the requested relative-Y offsets into the Y system. */
  AddRelativeYConstraints();
  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

/** Fold the requested relative-Y offsets into the Y system. */
void BoundToBoundHpwlOptimizer::AddRelativeYConstraints() {
  std::vector<Component>& components = ckt_ptr_->Components();
  for (const RelativeYConstraint& constraint : relative_y_constraints_) {
    const int first = constraint.first_component_id;
    const int second = constraint.second_component_id;
    DaliExpects(first >= 0 && first < static_cast<int>(components.size()) &&
                    second >= 0 && second < static_cast<int>(components.size()),
                "Relative Y constraint contains an invalid component id");
    DaliExpects(first != second,
                "Relative Y constraint cannot reference one component twice");
    DaliExpects(components[first].IsMovable() && components[second].IsMovable(),
                "Relative Y constraint requires movable components");

    const double current_offset =
        components[first].LLY() - components[second].LLY();
    const double weight =
        alpha /
        (std::fabs(current_offset - constraint.offset) + height_epsilon_);
    coefficients_y_.emplace_back(first, first, weight);
    coefficients_y_.emplace_back(second, second, weight);
    coefficients_y_.emplace_back(first, second, -weight);
    coefficients_y_.emplace_back(second, first, -weight);
    by[first] += constraint.offset * weight;
    by[second] -= constraint.offset * weight;
  }
}

void BoundToBoundHpwlOptimizer::BackUpComponentLocation() {
  std::vector<Component>& component_list = ckt_ptr_->Components();
  int sz = static_cast<int>(component_list.size());
  // #pragma omp for
  for (int i = 0; i < sz; ++i) {
    x_anchor[i] = component_list[i].LLX();
    y_anchor[i] = component_list[i].LLY();
  }
}

/**
 * Build and solve the X system with the anchor pseudo-nets folded in.
 *
 * The anchors pull the lower-bound solve toward the spread placement, so this
 * is what makes successive iterations converge rather than each re-solving from
 * scratch.
 */
void BoundToBoundHpwlOptimizer::OptimizeHpwlXWithAnchor(int num_threads) {
  LOG(trace) << "threads in branch x: " << num_threads
             << " actual number of threads: " << omp_get_max_threads()
             << " Eigen threads: " << Eigen::nbThreads() << "\n";

  std::vector<Component>& component_list = ckt_ptr_->Components();
  int sz = static_cast<int>(component_list.size());
#pragma omp parallel num_threads(num_threads) default(none) \
    shared(component_list, sz)
  {
#pragma omp for
    for (int i = 0; i < sz; ++i) {
      vx[i] = component_list[i].LLX();
    }
  }

  std::vector<double> eval_history_x;
  int b2b_update_it_x = 0;
  for (b2b_update_it_x = 0; b2b_update_it_x < b2b_update_max_iteration_;
       ++b2b_update_it_x) {
    LOG(trace) << "    Iterative net model update\n";
    BuildProblemWithAnchorX();
    double evaluate_result = OptimizeQuadraticMetricX(cg_stop_criterion_);
    eval_history_x.push_back(evaluate_result);
    if (evaluate_result < hpwl_early_stop_threshold_) {
      break;
    }
    if (eval_history_x.size() >= 3) {
      bool is_converge = IsSeriesConverged(eval_history_x, 3,
                                           net_model_update_stop_criterion_);
      bool is_oscillate = IsSeriesOscillate(eval_history_x, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        LOG(trace) << "Net model update oscillation detected X\n";
        break;
      }
    }
  }
  LOG(trace) << "  Optimization summary X, iterations x: " << b2b_update_it_x
             << ", " << eval_history_x << "\n";
  DaliExpects(
      !eval_history_x.empty(),
      "Cannot return a valid value because the result is not evaluated!");
  lower_bound_hpwl_x_.push_back(eval_history_x.back());
}

/**
 * Build and solve the Y system with anchors. The Y counterpart of
 * OptimizeHpwlXWithAnchor.
 */
void BoundToBoundHpwlOptimizer::OptimizeHpwlYWithAnchor(int num_threads) {
  LOG(trace) << "threads in branch y: " << num_threads
             << " actual number of threads: " << omp_get_max_threads()
             << " Eigen threads: " << Eigen::nbThreads() << "\n";

  std::vector<Component>& component_list = ckt_ptr_->Components();
  int sz = static_cast<int>(component_list.size());
#pragma omp parallel num_threads(num_threads) default(none) \
    shared(component_list, sz)
  {
#pragma omp for
    for (int i = 0; i < sz; ++i) {
      vy[i] = component_list[i].LLY();
    }
  }

  std::vector<double> eval_history_y;
  int b2b_update_it_y = 0;
  for (b2b_update_it_y = 0; b2b_update_it_y < b2b_update_max_iteration_;
       ++b2b_update_it_y) {
    LOG(trace) << "    Iterative net model update\n";
    BuildProblemWithAnchorY();
    double evaluate_result = OptimizeQuadraticMetricY(cg_stop_criterion_);
    eval_history_y.push_back(evaluate_result);
    if (evaluate_result < hpwl_early_stop_threshold_) {
      break;
    }
    if (eval_history_y.size() >= 3) {
      bool is_converge = IsSeriesConverged(eval_history_y, 3,
                                           net_model_update_stop_criterion_);
      bool is_oscillate = IsSeriesOscillate(eval_history_y, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        LOG(trace) << "Net model update oscillation detected Y\n";
        break;
      }
    }
  }
  LOG(trace) << "  Optimization summary Y, iterations y: " << b2b_update_it_y
             << ", " << eval_history_y << "\n";
  DaliExpects(
      !eval_history_y.empty(),
      "Cannot return a valid value because the result is not evaluated!");
  lower_bound_hpwl_y_.push_back(eval_history_y.back());
}

/** Solve X and Y for this iteration's wirelength lower bound. */
double BoundToBoundHpwlOptimizer::OptimizeHpwl() {
  omp_set_dynamic(0);
  int avail_threads_num = num_threads_ / 2;
  if (avail_threads_num == 0) {
    avail_threads_num = 1;
  }
  int previous_eigen_threads = Eigen::nbThreads();
  int previous_max_active_levels = omp_get_max_active_levels();

  // X and Y run in parallel, and each Eigen CG solve uses half of the thread
  // budget. Enable the nested Eigen workers only for this solver region.
  Eigen::setNbThreads(avail_threads_num);
  omp_set_max_active_levels(std::max(previous_max_active_levels, 2));

  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  UpdateAnchorLocation();
  UpdateAnchorAlpha();
  LOG(trace) << "alpha: " << alpha << "\n";
  LOG(trace) << "OpenMP threads, " << num_threads_ << "\n";

#pragma omp parallel num_threads(std::min(num_threads_, 2)) default(none) \
    shared(avail_threads_num)
  {
    if (omp_get_thread_num() == 0) {
      OptimizeHpwlXWithAnchor(avail_threads_num);
    }
    if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
      OptimizeHpwlYWithAnchor(avail_threads_num);
    }
  }

  omp_set_max_active_levels(previous_max_active_levels);
  Eigen::setNbThreads(previous_eigen_threads);

  PullComponentBackToRegion();

  LOG(trace) << "Quadratic Placement With Anchor Complete\n";

  elapsed_time.RecordEndTime();
  tot_cg_time += elapsed_time.GetWallTime();

  BackUpComponentLocation();
  relative_y_constraints_.clear();
  lower_bound_hpwl_.push_back(lower_bound_hpwl_x_.back() +
                              lower_bound_hpwl_y_.back());
  return lower_bound_hpwl_.back();
}

double BoundToBoundHpwlOptimizer::GetTime() { return tot_cg_time; }

void BoundToBoundHpwlOptimizer::Close() {
  LOG(debug) << "total triplets time: " << tot_triplets_time_x << "s, "
             << tot_triplets_time_y << "s, "
             << tot_triplets_time_x + tot_triplets_time_y << "s\n";
  LOG(debug) << "total matrix from triplets time: "
             << tot_matrix_from_triplets_x << "s, "
             << tot_matrix_from_triplets_y << "s, "
             << tot_matrix_from_triplets_x + tot_matrix_from_triplets_y
             << "s\n";
  LOG(debug) << "total cg solver time: " << tot_cg_solver_time_x << "s, "
             << tot_cg_solver_time_y << "s, "
             << tot_cg_solver_time_x + tot_cg_solver_time_y << "s\n";
  LOG(debug) << "  cg compute time: " << tot_cg_compute_time_x << "s, "
             << tot_cg_compute_time_y << "s\n";
  LOG(debug) << "  cg solve time: " << tot_cg_solve_time_x << "s, "
             << tot_cg_solve_time_y << "s\n";
  LOG(debug) << "  HPWL evaluation time: " << tot_hpwl_evaluation_time_x
             << "s, " << tot_hpwl_evaluation_time_y << "s\n";
  LOG(debug) << "total loc update time: " << tot_loc_update_time_x << "s, "
             << tot_loc_update_time_y << "s, "
             << tot_loc_update_time_x + tot_loc_update_time_y << "s\n";
  double tot_time_x = tot_triplets_time_x + tot_matrix_from_triplets_x +
                      tot_cg_solver_time_x + tot_loc_update_time_x;
  double tot_time_y = tot_triplets_time_y + tot_matrix_from_triplets_y +
                      tot_cg_solver_time_y + tot_loc_update_time_y;
  LOG(debug) << "total x/y time: " << tot_time_x << "s, " << tot_time_y << "s, "
             << tot_time_x + tot_time_y << "s\n";

  RecordPlacementMetric("time.global_placement.quadratic.wall_s", tot_cg_time);
  RecordPlacementMetric("time.global_placement.quadratic.x.model_build.wall_s",
                        tot_triplets_time_x);
  RecordPlacementMetric("time.global_placement.quadratic.y.model_build.wall_s",
                        tot_triplets_time_y);
  RecordPlacementMetric("time.global_placement.quadratic.x.matrix_build.wall_s",
                        tot_matrix_from_triplets_x);
  RecordPlacementMetric("time.global_placement.quadratic.y.matrix_build.wall_s",
                        tot_matrix_from_triplets_y);
  RecordPlacementMetric("time.global_placement.quadratic.x.compute.wall_s",
                        tot_cg_compute_time_x);
  RecordPlacementMetric("time.global_placement.quadratic.y.compute.wall_s",
                        tot_cg_compute_time_y);
  RecordPlacementMetric("time.global_placement.quadratic.x.solve.wall_s",
                        tot_cg_solve_time_x);
  RecordPlacementMetric("time.global_placement.quadratic.y.solve.wall_s",
                        tot_cg_solve_time_y);
  RecordPlacementMetric(
      "time.global_placement.quadratic.x.location_update.wall_s",
      tot_loc_update_time_x);
  RecordPlacementMetric(
      "time.global_placement.quadratic.y.location_update.wall_s",
      tot_loc_update_time_y);
  RecordPlacementMetric(
      "time.global_placement.quadratic.x.hpwl_evaluation.wall_s",
      tot_hpwl_evaluation_time_x);
  RecordPlacementMetric(
      "time.global_placement.quadratic.y.hpwl_evaluation.wall_s",
      tot_hpwl_evaluation_time_y);
}

}  // namespace dali
