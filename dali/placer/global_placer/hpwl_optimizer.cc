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

#include "hpwl_optimizer.h"

#include <algorithm>
#include <cfloat>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"

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

  // initialize containers to store HPWL after each iteration
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

void BoundToBoundHpwlOptimizer::BuildProblemX() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Component>& components = ckt_ptr_->Components();
  std::vector<Net>& nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);
  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x =
      (ckt_ptr_->RegionLLX() + ckt_ptr_->RegionURX()) / 2.0 * center_weight;
  // double decay_length = decay_factor * ckt_ptr_->AverageComponentHeight();

  for (auto& net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdX();
    int max_pin_index = net.MaxComponentPinIdX();
    int min_pin_index = net.MinComponentPinIdX();

    int max_component_id = net.ComponentPins()[max_pin_index].ComponentId();
    double pin_loc_max = net.ComponentPins()[max_pin_index].AbsX();
    bool is_movable_max =
        net.ComponentPins()[max_pin_index].ComponentPtr()->IsMovable();
    double offset_max = net.ComponentPins()[max_pin_index].OffsetX();

    int min_component_id = net.ComponentPins()[min_pin_index].ComponentId();
    double pin_loc_min = net.ComponentPins()[min_pin_index].AbsX();
    bool is_movable_min =
        net.ComponentPins()[min_pin_index].ComponentPtr()->IsMovable();
    double offset_min = net.ComponentPins()[min_pin_index].OffsetX();

    for (auto& pair : net.ComponentPins()) {
      int component_id = pair.ComponentId();
      double pin_loc = pair.AbsX();
      bool is_movable = pair.ComponentPtr()->IsMovable();
      double offset = pair.OffsetX();

      if (component_id != max_component_id) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + width_epsilon_);
        // weight_adjust = base_factor + adjust_factor * (1 - exp(-distance /
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
        // weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
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

void BoundToBoundHpwlOptimizer::BuildProblemY() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Component>& components = ckt_ptr_->Components();
  std::vector<Net>& nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);
  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y =
      (ckt_ptr_->RegionLLY() + ckt_ptr_->RegionURY()) / 2.0 * center_weight;
  // double decay_length = decay_factor * ckt_ptr_->AverageComponentHeight();

  for (auto& net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdY();
    int max_pin_index = net.MaxComponentPinIdY();
    int min_pin_index = net.MinComponentPinIdY();

    int max_component_id = net.ComponentPins()[max_pin_index].ComponentId();
    double pin_loc_max = net.ComponentPins()[max_pin_index].AbsY();
    bool is_movable_max =
        net.ComponentPins()[max_pin_index].ComponentPtr()->IsMovable();
    double offset_max = net.ComponentPins()[max_pin_index].OffsetY();

    int min_component_id = net.ComponentPins()[min_pin_index].ComponentId();
    double pin_loc_min = net.ComponentPins()[min_pin_index].AbsY();
    bool is_movable_min =
        net.ComponentPins()[min_pin_index].ComponentPtr()->IsMovable();
    double offset_min = net.ComponentPins()[min_pin_index].OffsetY();

    for (auto& pair : net.ComponentPins()) {
      int component_id = pair.ComponentId();
      double pin_loc = pair.AbsY();
      bool is_movable = pair.ComponentPtr()->IsMovable();
      double offset = pair.OffsetY();

      if (component_id != max_component_id) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + height_epsilon_);
        // weight_adjust = base_factor + adjust_factor * (1 - exp(-distance /
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
        // weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
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
  // trend (false).
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

double BoundToBoundHpwlOptimizer::OptimizeQuadraticMetricX(
    double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  Ax.setFromTriplets(coefficients_x_.begin(), coefficients_x_.end());
  elapsed_time.RecordEndTime();
  tot_matrix_from_triplets_x += elapsed_time.GetWallTime();

  int sz = static_cast<int>(vx.size());
  std::vector<Component>& components = ckt_ptr_->Components();

  elapsed_time.RecordStartTime();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_x_.compute(Ax);  // Ax * vx = bx
  for (int i = 0; i < max_rounds; ++i) {
    vx = cg_x_.solveWithGuess(bx, vx);
    // #pragma omp for
    for (int num = 0; num < sz; ++num) {
      components[num].SetLLX(vx[num]);
    }
    double evaluate_result = ckt_ptr_->WeightedHPWLX();
    eval_history.push_back(evaluate_result);
    if (evaluate_result < hpwl_early_stop_threshold_) {
      break;
    }
    // LOG(info)  <<"  %d WeightedHPWLX: %e\n", i,
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
  elapsed_time.RecordEndTime();
  tot_cg_solver_time_x += elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
  // #pragma omp for
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

double BoundToBoundHpwlOptimizer::OptimizeQuadraticMetricY(
    double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  Ay.setFromTriplets(coefficients_y_.begin(), coefficients_y_.end());
  elapsed_time.RecordEndTime();
  tot_matrix_from_triplets_y += elapsed_time.GetWallTime();

  int sz = static_cast<int>(vy.size());
  std::vector<Component>& component_list = ckt_ptr_->Components();

  elapsed_time.RecordStartTime();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_y_.compute(Ay);
  for (int i = 0; i < max_rounds; ++i) {
    vy = cg_y_.solveWithGuess(by, vy);
    // #pragma omp for
    for (int num = 0; num < sz; ++num) {
      component_list[num].SetLLY(vy[num]);
    }
    double evaluate_result = ckt_ptr_->WeightedHPWLY();
    eval_history.push_back(evaluate_result);
    if (evaluate_result < hpwl_early_stop_threshold_) {
      break;
    }
    // LOG(info)  <<"  %d WeightedHPWLY: %e\n", i,
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
  elapsed_time.RecordEndTime();
  tot_cg_solver_time_y += elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
  // #pragma omp for
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

void BoundToBoundHpwlOptimizer::UpdateMaxMinX() {
  std::vector<Net>& net_list = ckt_ptr_->Nets();
  size_t sz = net_list.size();
  // #pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    net_list[i].UpdateMaxMinIdX();
  }
}

void BoundToBoundHpwlOptimizer::UpdateMaxMinY() {
  std::vector<Net>& net_list = ckt_ptr_->Nets();
  size_t sz = net_list.size();
  // #pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    net_list[i].UpdateMaxMinIdY();
  }
}

void BoundToBoundHpwlOptimizer::BuildProblemWithAnchorX() {
  UpdateMaxMinX();
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
void BoundToBoundHpwlOptimizer::BuildProblemWithAnchorY() {
  UpdateMaxMinY();
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
  AddRelativeYConstraints();
  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

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

void BoundToBoundHpwlOptimizer::OptimizeHpwlXWithAnchor(int num_threads) {
  Eigen::setNbThreads(num_threads);
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

double BoundToBoundHpwlOptimizer::OptimizeHpwl() {
  omp_set_dynamic(0);
  int avail_threads_num = num_threads_ / 2;
  if (avail_threads_num == 0) {
    avail_threads_num = 1;
  }
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  UpdateAnchorLocation();
  UpdateAnchorAlpha();
  // UpdateAnchorNetWeight();
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
  LOG(debug) << "total loc update time: " << tot_loc_update_time_x << "s, "
             << tot_loc_update_time_y << "s, "
             << tot_loc_update_time_x + tot_loc_update_time_y << "s\n";
  double tot_time_x = tot_triplets_time_x + tot_matrix_from_triplets_x +
                      tot_cg_solver_time_x + tot_loc_update_time_x;
  double tot_time_y = tot_triplets_time_y + tot_matrix_from_triplets_y +
                      tot_cg_solver_time_y + tot_loc_update_time_y;
  LOG(debug) << "total x/y time: " << tot_time_x << "s, " << tot_time_y << "s, "
             << tot_time_x + tot_time_y << "s\n";
}

}  // namespace dali
