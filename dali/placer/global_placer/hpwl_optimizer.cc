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

#include <cfloat>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"

namespace dali {

HpwlOptimizer::HpwlOptimizer(Circuit *ckt_ptr, int num_threads) {
  DaliExpects(ckt_ptr != nullptr, "Circuit is a nullptr?");
  ckt_ptr_ = ckt_ptr;
  DaliExpects(num_threads >= 1, "Number of threads less than 1?");
  num_threads_ = num_threads;
}

/****
 * @brief During quadratic placement, net weights are computed by dividing the
 * distance between two pins. To improve numerical stability, a small number is
 * added to this distance.
 *
 * In our implementation, this small number is around the average movable cell
 * width for x and height for y.
 */
void B2BHpwlOptimizer::UpdateEpsilon() {
  width_epsilon_ = ckt_ptr_->AveMovBlkWidth() * epsilon_factor_;
  height_epsilon_ = ckt_ptr_->AveMovBlkHeight() * epsilon_factor_;
}

/****
 * @brief Initialize variables for the conjugate gradient linear solver
 */
void B2BHpwlOptimizer::Initialize() {
  // set a small value for net weight dividend to improve numerical stability
  UpdateEpsilon();

  // initialize containers to store HPWL after each iteration
  lower_bound_hpwl_x_.clear();
  lower_bound_hpwl_y_.clear();
  lower_bound_hpwl_.clear();

  size_t sz = ckt_ptr_->Blocks().size();
  ADx.resize(sz);
  ADy.resize(sz);
  Ax_row_size.assign(sz, 0);
  Ax_row_size.assign(sz, 0);

  EgId eigen_sz = static_cast<EgId>(ckt_ptr_->Blocks().size());
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
  auto &nets = ckt_ptr_->Nets();
  for (auto &net : nets) {
    size_t net_sz = net.PinCnt();
    // if a net has size n, then in total, there will be (2(n-2)+1)*4 non-zero entries for the matrix
    if (net_sz > 1) {
      coefficient_size += (2 * (net_sz - 2) + 1) * 4;
    }
  }
  // this is to reserve space for anchor, because each block may need an anchor
  coefficient_size += sz;
  // this is to reserve space for anchor in the center of the placement region
  coefficient_size += sz;
  coefficients_x_.reserve(coefficient_size);
  Ax.reserve(static_cast<EgId>(coefficient_size));
  coefficients_y_.reserve(coefficient_size);
  Ay.reserve(static_cast<EgId>(coefficient_size));
}

void B2BHpwlOptimizer::BuildProblemX() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);
  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x =
      (ckt_ptr_->RegionLLX() + ckt_ptr_->RegionURX()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdX();
    int max_pin_index = net.MaxBlkPinIdX();
    int min_pin_index = net.MinBlkPinIdX();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetX();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetX();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsX();
      bool is_movable = pair.BlkPtr()->IsMovable();
      double offset = pair.OffsetX();

      if (blk_num != blk_num_max) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + width_epsilon_);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_max) {
          bx[blk_num_max] += (pin_loc - offset_max) * weight;
          coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
        } else if (is_movable && !is_movable_max) {
          bx[blk_num] += (pin_loc_max - offset) * weight;
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_max) {
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
          coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
          coefficients_x_.emplace_back(blk_num, blk_num_max, -weight);
          coefficients_x_.emplace_back(blk_num_max, blk_num, -weight);
          double offset_diff = (offset_max - offset) * weight;
          bx[blk_num] += offset_diff;
          bx[blk_num_max] -= offset_diff;
        }
      }

      if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
        double distance = std::fabs(pin_loc - pin_loc_min);
        double weight = inv_p / (distance + width_epsilon_);
        //weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_min) {
          bx[blk_num_min] += (pin_loc - offset_min) * weight;
          coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
        } else if (is_movable && !is_movable_min) {
          bx[blk_num] += (pin_loc_min - offset) * weight;
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_min) {
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
          coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
          coefficients_x_.emplace_back(blk_num, blk_num_min, -weight);
          coefficients_x_.emplace_back(blk_num_min, blk_num, -weight);
          double offset_diff = (offset_min - offset) * weight;
          bx[blk_num] += offset_diff;
          bx[blk_num_min] -= offset_diff;
        }
      }
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = blocks[i].LLX();
    } else {
      if (blocks[i].LLX() < ckt_ptr_->RegionLLX()
          || blocks[i].URX() > ckt_ptr_->RegionURX()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_x_.capacity(),
      "WARNING: x coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_x_.size()
  );

  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}

void B2BHpwlOptimizer::BuildProblemY() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);
  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y =
      (ckt_ptr_->RegionLLY() + ckt_ptr_->RegionURY()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdY();
    int max_pin_index = net.MaxBlkPinIdY();
    int min_pin_index = net.MinBlkPinIdY();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetY();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetY();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsY();
      bool is_movable = pair.BlkPtr()->IsMovable();
      double offset = pair.OffsetY();

      if (blk_num != blk_num_max) {
        double distance = std::fabs(pin_loc - pin_loc_max);
        double weight = inv_p / (distance + height_epsilon_);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_max) {
          by[blk_num_max] += (pin_loc - offset_max) * weight;
          coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
        } else if (is_movable && !is_movable_max) {
          by[blk_num] += (pin_loc_max - offset) * weight;
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_max) {
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
          coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
          coefficients_y_.emplace_back(blk_num, blk_num_max, -weight);
          coefficients_y_.emplace_back(blk_num_max, blk_num, -weight);
          double offset_diff = (offset_max - offset) * weight;
          by[blk_num] += offset_diff;
          by[blk_num_max] -= offset_diff;
        }
      }

      if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
        double distance = std::fabs(pin_loc - pin_loc_min);
        double weight = inv_p / (distance + height_epsilon_);
        //weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
        //weight *= weight_adjust;
        if (!is_movable && is_movable_min) {
          by[blk_num_min] += (pin_loc - offset_min) * weight;
          coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
        } else if (is_movable && !is_movable_min) {
          by[blk_num] += (pin_loc_min - offset) * weight;
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && is_movable_min) {
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
          coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
          coefficients_y_.emplace_back(blk_num, blk_num_min, -weight);
          coefficients_y_.emplace_back(blk_num_min, blk_num, -weight);
          double offset_diff = (offset_min - offset) * weight;
          by[blk_num] += offset_diff;
          by[blk_num_min] -= offset_diff;
        }
      }

    }
  }
  // add the diagonal non-zero element for fixed blocks
  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = blocks[i].LLY();
    } else {
      if (blocks[i].LLY() < ckt_ptr_->RegionLLY()
          || blocks[i].URY() > ckt_ptr_->RegionURY()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_y_.capacity(),
      "WARNING: y coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_y_.size()
  );

  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

bool B2BHpwlOptimizer::IsSeriesConverge(
    std::vector<double> &data,
    int window_size,
    double tolerance
) {
  int sz = (int) data.size();
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
  double ratio = max_val / min_val - 1;
  return ratio < tolerance;
}

/****
* Returns if the given series of data is oscillating or not.
* We will only look at the last several data points @param length.
* ****/
bool B2BHpwlOptimizer::IsSeriesOscillate(
    std::vector<double> &data,
    int window_size
) {
  // if the given length is too short, we cannot know whether it is oscillating or not.
  if (window_size < 3) return false;

  // if the given data series is short than the length, we cannot know whether it is oscillating or not.
  int sz = (int) data.size();
  if (sz < window_size) {
    return false;
  }

  // this vector keeps track of the increasing trend (true) and descreasing trend (false).
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

double B2BHpwlOptimizer::OptimizeQuadraticMetricX(double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  Ax.setFromTriplets(coefficients_x_.begin(), coefficients_x_.end());
  elapsed_time.RecordEndTime();
  tot_matrix_from_triplets_x += elapsed_time.GetWallTime();

  int sz = static_cast<int>(vx.size());
  std::vector<Block> &blocks = ckt_ptr_->Blocks();

  elapsed_time.RecordStartTime();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_x_.compute(Ax); // Ax * vx = bx
  for (int i = 0; i < max_rounds; ++i) {
    vx = cg_x_.solveWithGuess(bx, vx);
//#pragma omp for
    for (int num = 0; num < sz; ++num) {
      blocks[num].SetLLX(vx[num]);
    }
    double evaluate_result = ckt_ptr_->WeightedHPWLX();
    eval_history.push_back(evaluate_result);
    //BOOST_LOG_TRIVIAL(info)  <<"  %d WeightedHPWLX: %e\n", i, evaluate_result);
    if (eval_history.size() >= 3) {
      bool is_converge = IsSeriesConverge(eval_history, 3, cg_stop_criterion);
      bool is_oscillate = IsSeriesOscillate(eval_history, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        BOOST_LOG_TRIVIAL(trace) << "oscillation detected\n";
        break;
      }
    }
  }
  BOOST_LOG_TRIVIAL(trace)
    << "      Metric optimization in X, sequence: " << eval_history << "\n";
  elapsed_time.RecordEndTime();
  tot_cg_solver_time_x += elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
//#pragma omp for
  for (int num = 0; num < sz; ++num) {
    blocks[num].SetLLX(vx[num]);
  }
  elapsed_time.RecordEndTime();
  tot_loc_update_time_x += elapsed_time.GetWallTime();

  DaliExpects(
      !eval_history.empty(),
      "Cannot return a valid value because the result is not evaluated!"
  );
  return eval_history.back();
}

double B2BHpwlOptimizer::OptimizeQuadraticMetricY(double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  Ay.setFromTriplets(coefficients_y_.begin(), coefficients_y_.end());
  elapsed_time.RecordEndTime();
  tot_matrix_from_triplets_y += elapsed_time.GetWallTime();

  int sz = static_cast<int>(vy.size());
  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  elapsed_time.RecordStartTime();
  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_y_.compute(Ay);
  for (int i = 0; i < max_rounds; ++i) {
    vy = cg_y_.solveWithGuess(by, vy);
//#pragma omp for
    for (int num = 0; num < sz; ++num) {
      block_list[num].SetLLY(vy[num]);
    }
    double evaluate_result = ckt_ptr_->WeightedHPWLY();
    eval_history.push_back(evaluate_result);
    //BOOST_LOG_TRIVIAL(info)  <<"  %d WeightedHPWLY: %e\n", i, evaluate_result);
    if (eval_history.size() >= 3) {
      bool is_converge = IsSeriesConverge(eval_history, 3, cg_stop_criterion);
      bool is_oscillate = IsSeriesOscillate(eval_history, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        BOOST_LOG_TRIVIAL(trace) << "oscillation detected\n";
        break;
      }
    }
  }
  BOOST_LOG_TRIVIAL(trace) << "      Metric optimization in Y, sequence: "
                           << eval_history << "\n";
  elapsed_time.RecordEndTime();
  tot_cg_solver_time_y += elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
//#pragma omp for
  for (int num = 0; num < sz; ++num) {
    block_list[num].SetLLY(vy[num]);
  }
  elapsed_time.RecordEndTime();
  tot_loc_update_time_y += elapsed_time.GetWallTime();

  DaliExpects(!eval_history.empty(),
              "Cannot return a valid value because the result is not evaluated!");
  return eval_history.back();
}

void B2BHpwlOptimizer::PullBlockBackToRegion() {
  int sz = static_cast<int>(vx.size());
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  double region_llx = ckt_ptr_->RegionLLX();
  double region_urx = ckt_ptr_->RegionURX();
  double region_lly = ckt_ptr_->RegionLLY();
  double region_ury = ckt_ptr_->RegionURY();
#pragma omp parallel num_threads(omp_get_max_threads()) default(none) shared(block_list, sz, region_llx, region_urx, region_lly, region_ury)
  {
#pragma omp for
    for (int i = 0; i < sz; ++i) {
      if (block_list[i].IsMovable()) {
        if (vx[i] < region_llx) {
          vx[i] = region_llx;
        }
        double blk_hi_bound_x = region_urx - block_list[i].Width();
        if (vx[i] > blk_hi_bound_x) {
          vx[i] = blk_hi_bound_x;
        }

        if (vy[i] < region_lly) {
          vy[i] = region_lly;
        }
        double blk_hi_bound_y = region_ury - block_list[i].Height();
        if (vy[i] > blk_hi_bound_y) {
          vy[i] = blk_hi_bound_y;
        }
      }
    }

#pragma omp for
    for (int i = 0; i < sz; ++i) {
      block_list[i].SetLoc(vx[i], vy[i]);
    }
  }
}

void B2BHpwlOptimizer::UpdateAnchorLocation() {
  if (cur_iter_ == 0) return;
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  for (int i = 0; i < sz; ++i) {
    double tmp_loc_x = x_anchor[i];
    x_anchor[i] = block_list[i].LLX();
    block_list[i].SetLLX(tmp_loc_x);

    double tmp_loc_y = y_anchor[i];
    y_anchor[i] = block_list[i].LLY();
    block_list[i].SetLLY(tmp_loc_y);
  }

  x_anchor_set = true;
  y_anchor_set = true;
}

void B2BHpwlOptimizer::UpdateAnchorAlpha() {
  if (cur_iter_ == 0) {
    alpha_step = 0;
  } else if (0 < cur_iter_ && cur_iter_ < 5) {
    alpha_step = 0.005;
  } else if (cur_iter_ < 10) {
    alpha_step = 0.01;
  } else if (cur_iter_ < 15) {
    alpha_step = 0.02;
  } else {
    alpha_step = 0.03;
  }
  alpha += alpha_step;
}

void B2BHpwlOptimizer::UpdateMaxMinX() {
  std::vector<Net> &net_list = ckt_ptr_->Nets();
  size_t sz = net_list.size();
//#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    net_list[i].UpdateMaxMinIdX();
  }
}

void B2BHpwlOptimizer::UpdateMaxMinY() {
  std::vector<Net> &net_list = ckt_ptr_->Nets();
  size_t sz = net_list.size();
//#pragma omp parallel for
  for (size_t i = 0; i < sz; ++i) {
    net_list[i].UpdateMaxMinIdY();
  }
}

void B2BHpwlOptimizer::BuildProblemWithAnchorX() {
  UpdateMaxMinX();
  BuildProblemX();

  if (cur_iter_ == 0) return;
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLX();
    pin_loc1 = x_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    bx[i] += pin_loc1 * weight;
    coefficients_x_.emplace_back(T(i, i, weight));
  }
  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}
void B2BHpwlOptimizer::BuildProblemWithAnchorY() {
  UpdateMaxMinY();
  BuildProblemY();

  if (cur_iter_ == 0) return;
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLY();
    pin_loc1 = y_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    by[i] += pin_loc1 * weight;
    coefficients_y_.emplace_back(T(i, i, weight));
  }
  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

void B2BHpwlOptimizer::BackUpBlockLocation() {
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());
//#pragma omp for
  for (int i = 0; i < sz; ++i) {
    x_anchor[i] = block_list[i].LLX();
    y_anchor[i] = block_list[i].LLY();
  }
}

void B2BHpwlOptimizer::OptimizeHpwlXWithAnchor(int num_threads) {
  Eigen::setNbThreads(num_threads);
  BOOST_LOG_TRIVIAL(trace)
    << "threads in branch x: " << num_threads
    << " actual number of threads: " << omp_get_max_threads()
    << " Eigen threads: " << Eigen::nbThreads() << "\n";

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());
#pragma omp parallel num_threads(num_threads) default(none) shared(block_list, sz)
  {
#pragma omp for
    for (int i = 0; i < sz; ++i) {
      vx[i] = block_list[i].LLX();
    }
  }

  std::vector<double> eval_history_x;
  int b2b_update_it_x = 0;
  for (b2b_update_it_x = 0; b2b_update_it_x < b2b_update_max_iteration_;
       ++b2b_update_it_x) {
    BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
    BuildProblemWithAnchorX();
    double evaluate_result = OptimizeQuadraticMetricX(cg_stop_criterion_);
    eval_history_x.push_back(evaluate_result);
    if (eval_history_x.size() >= 3) {
      bool is_converge = IsSeriesConverge(
          eval_history_x,
          3,
          net_model_update_stop_criterion_
      );
      bool is_oscillate = IsSeriesOscillate(eval_history_x, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        BOOST_LOG_TRIVIAL(trace)
          << "Net model update oscillation detected X\n";
        break;
      }
    }
  }
  BOOST_LOG_TRIVIAL(trace)
    << "  Optimization summary X, iterations x: " << b2b_update_it_x
    << ", " << eval_history_x << "\n";
  DaliExpects(
      !eval_history_x.empty(),
      "Cannot return a valid value because the result is not evaluated!");
  lower_bound_hpwl_x_.push_back(eval_history_x.back());
}

void B2BHpwlOptimizer::OptimizeHpwlYWithAnchor(int num_threads) {
  BOOST_LOG_TRIVIAL(trace)
    << "threads in branch y: "
    << omp_get_max_threads()
    << " Eigen threads: " << Eigen::nbThreads()
    << "\n";

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());
#pragma omp parallel num_threads(num_threads) default(none) shared(block_list, sz)
  {
#pragma omp for
    for (size_t i = 0; i < block_list.size(); ++i) {
      vy[i] = block_list[i].LLY();
    }
  }

  std::vector<double> eval_history_y;
  int b2b_update_it_y = 0;
  for (b2b_update_it_y = 0;
       b2b_update_it_y < b2b_update_max_iteration_;
       ++b2b_update_it_y) {
    BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
    BuildProblemWithAnchorY();
    double evaluate_result =
        OptimizeQuadraticMetricY(cg_stop_criterion_);
    eval_history_y.push_back(evaluate_result);
    if (eval_history_y.size() >= 3) {
      bool is_converge = IsSeriesConverge(
          eval_history_y,
          3,
          net_model_update_stop_criterion_
      );
      bool is_oscillate = IsSeriesOscillate(eval_history_y, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        BOOST_LOG_TRIVIAL(trace)
          << "Net model update oscillation detected Y\n";
        break;
      }
    }
  }
  BOOST_LOG_TRIVIAL(trace)
    << "  Optimization summary Y, iterations y: " << b2b_update_it_y
    << ", " << eval_history_y << "\n";
  DaliExpects(!eval_history_y.empty(),
              "Cannot return a valid value because the result is not evaluated!");
  lower_bound_hpwl_y_.push_back(eval_history_y.back());
}

double B2BHpwlOptimizer::OptimizeHpwl() {
  omp_set_dynamic(0);
  int avail_threads_num = num_threads_ / 2;
  if (avail_threads_num == 0) {
    avail_threads_num = 1;
  }
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  UpdateAnchorLocation();
  UpdateAnchorAlpha();
  //UpdateAnchorNetWeight();
  BOOST_LOG_TRIVIAL(trace) << "alpha: " << alpha << "\n";
  BOOST_LOG_TRIVIAL(trace) << "OpenMP threads, " << num_threads_ << "\n";

#pragma omp parallel num_threads(std::min(num_threads_, 2)) default(none) shared(avail_threads_num)
  {
    if (omp_get_thread_num() == 0) {
      OptimizeHpwlXWithAnchor(avail_threads_num);
    }
    if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
      OptimizeHpwlYWithAnchor(avail_threads_num);
    }
  }

  PullBlockBackToRegion();

  BOOST_LOG_TRIVIAL(trace) << "Quadratic Placement With Anchor Complete\n";

  elapsed_time.RecordEndTime();
  tot_cg_time += elapsed_time.GetWallTime();

  if (is_dump_) DumpResult("cg_result_" + std::to_string(cur_iter_) + ".txt");
  BackUpBlockLocation();
  lower_bound_hpwl_.push_back(
      lower_bound_hpwl_x_.back() + lower_bound_hpwl_y_.back());
  return lower_bound_hpwl_.back();
}

double B2BHpwlOptimizer::GetTime() {
  return tot_cg_time;
}

void B2BHpwlOptimizer::Close() {
  BOOST_LOG_TRIVIAL(debug)
    << "total triplets time: "
    << tot_triplets_time_x << "s, "
    << tot_triplets_time_y << "s, "
    << tot_triplets_time_x + tot_triplets_time_y << "s\n";
  BOOST_LOG_TRIVIAL(debug)
    << "total matrix from triplets time: "
    << tot_matrix_from_triplets_x << "s, "
    << tot_matrix_from_triplets_y << "s, "
    << tot_matrix_from_triplets_x + tot_matrix_from_triplets_y << "s\n";
  BOOST_LOG_TRIVIAL(debug)
    << "total cg solver time: "
    << tot_cg_solver_time_x << "s, "
    << tot_cg_solver_time_y << "s, "
    << tot_cg_solver_time_x + tot_cg_solver_time_y << "s\n";
  BOOST_LOG_TRIVIAL(debug)
    << "total loc update time: "
    << tot_loc_update_time_x << "s, "
    << tot_loc_update_time_y << "s, "
    << tot_loc_update_time_x + tot_loc_update_time_y << "s\n";
  double tot_time_x = tot_triplets_time_x + tot_matrix_from_triplets_x
      + tot_cg_solver_time_x + tot_loc_update_time_x;
  double tot_time_y = tot_triplets_time_y + tot_matrix_from_triplets_y
      + tot_cg_solver_time_y + tot_loc_update_time_y;
  BOOST_LOG_TRIVIAL(debug)
    << "total x/y time: "
    << tot_time_x << "s, "
    << tot_time_y << "s, "
    << tot_time_x + tot_time_y << "s\n";
}

void B2BHpwlOptimizer::DumpResult(std::string const &name_of_file) {
  //UpdateGridBinState();
  static int counter = 0;
  //BOOST_LOG_TRIVIAL(info)   << "DumpNum:" << counter << "\n";
  ckt_ptr_->GenMATLABTable(name_of_file);
  //write_not_all_terminal_grid_bins("grid_bin_not_all_terminal" + std::to_string(counter) + ".txt");
  //write_overfill_grid_bins("grid_bin_overfill" + std::to_string(counter) + ".txt");
  ++counter;
}

void StarHpwlOptimizer::BuildProblemX() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);

  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x =
      (ckt_ptr_->RegionLLX() + ckt_ptr_->RegionURX()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();

    // assuming the 0-th pin in the net is the driver pin
    int driver_blk_num = net.BlockPins()[0].BlkId();
    double driver_pin_loc = net.BlockPins()[0].AbsX();
    bool driver_is_movable = net.BlockPins()[0].BlkPtr()->IsMovable();
    double driver_offset = net.BlockPins()[0].OffsetX();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsX();
      bool is_movable = pair.BlkPtr()->IsMovable();

      if (blk_num != driver_blk_num) {
        double distance = std::fabs(pin_loc - driver_pin_loc);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight = inv_p / (distance + width_epsilon_) * weight_adjust;
        double weight = inv_p / (distance + width_epsilon_);
        if (!is_movable && driver_is_movable) {
          bx[0] += (pin_loc - driver_offset) * weight;
          coefficients_x_.emplace_back(driver_blk_num, driver_blk_num, weight);
        } else if (is_movable && !driver_is_movable) {
          bx[blk_num] += (driver_pin_loc - driver_offset) * weight;
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && driver_is_movable) {
          coefficients_x_.emplace_back(blk_num, blk_num, weight);
          coefficients_x_.emplace_back(driver_blk_num, driver_blk_num, weight);
          coefficients_x_.emplace_back(blk_num, driver_blk_num, -weight);
          coefficients_x_.emplace_back(driver_blk_num, blk_num, -weight);
          double offset_diff = (driver_offset - pair.OffsetX()) * weight;
          bx[blk_num] += offset_diff;
          bx[driver_blk_num] -= offset_diff;
        }
      }
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = blocks[i].LLX();
    } else {
      if (blocks[i].LLX() < ckt_ptr_->RegionLLX()
          || blocks[i].URX() > ckt_ptr_->RegionURX()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_x_.capacity(),
      "WARNING: x coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_x_.size()
  );

  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}

void StarHpwlOptimizer::BuildProblemY() {
  ElapsedTime elapsed_time;
  elapsed_time.GetWallTime();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);

  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y =
      (ckt_ptr_->RegionLLY() + ckt_ptr_->RegionURY()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();

    // assuming the 0-th pin in the net is the driver pin
    int driver_blk_num = net.BlockPins()[0].BlkId();
    double driver_pin_loc = net.BlockPins()[0].AbsY();
    bool driver_is_movable = net.BlockPins()[0].BlkPtr()->IsMovable();
    double driver_offset = net.BlockPins()[0].OffsetY();

    for (auto &pair : net.BlockPins()) {
      int blk_num = pair.BlkId();
      double pin_loc = pair.AbsY();
      bool is_movable = pair.BlkPtr()->IsMovable();

      if (blk_num != driver_blk_num) {
        double distance = std::fabs(pin_loc - driver_pin_loc);
        //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
        //weight = inv_p / (distance + height_epsilon_) * weight_adjust;
        double weight = inv_p / (distance + height_epsilon_);
        if (!is_movable && driver_is_movable) {
          by[0] += (pin_loc - driver_offset) * weight;
          coefficients_y_.emplace_back(driver_blk_num, driver_blk_num, weight);
        } else if (is_movable && !driver_is_movable) {
          by[blk_num] += (driver_pin_loc - driver_offset) * weight;
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
        } else if (is_movable && driver_is_movable) {
          coefficients_y_.emplace_back(blk_num, blk_num, weight);
          coefficients_y_.emplace_back(driver_blk_num, driver_blk_num, weight);
          coefficients_y_.emplace_back(blk_num, driver_blk_num, -weight);
          coefficients_y_.emplace_back(driver_blk_num, blk_num, -weight);
          double offset_diff = (driver_offset - pair.OffsetY()) * weight;
          by[blk_num] += offset_diff;
          by[driver_blk_num] -= offset_diff;
        }
      }
    }
  }

  // add the diagonal non-zero element for fixed blocks
  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = blocks[i].LLY();
    } else {
      if (blocks[i].LLY() < ckt_ptr_->RegionLLY()
          || blocks[i].URY() > ckt_ptr_->RegionURY()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_y_.capacity(),
      "WARNING: y coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_y_.size()
  );

  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

void StarHpwlOptimizer::UpdateAnchorAlpha() {
  alpha = 0.002 * cur_iter_;
}

void HpwlHpwlOptimizer::BuildProblemX() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_x_.capacity();
  coefficients_x_.resize(0);

  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x =
      (ckt_ptr_->RegionLLX() + ckt_ptr_->RegionURX()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdX();
    int max_pin_index = net.MaxBlkPinIdX();
    int min_pin_index = net.MinBlkPinIdX();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetX();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetX();

    double distance = std::fabs(pin_loc_min - pin_loc_max);
    //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
    //weight = inv_p / (distance + width_epsilon_) * weight_adjust;
    double weight = inv_p / (distance + width_epsilon_);
    if (!is_movable_min && is_movable_max) {
      bx[blk_num_max] += (pin_loc_min - offset_max) * weight;
      coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
    } else if (is_movable_min && !is_movable_max) {
      bx[blk_num_min] += (pin_loc_max - offset_min) * weight;
      coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
    } else if (is_movable_min && is_movable_max) {
      coefficients_x_.emplace_back(blk_num_min, blk_num_min, weight);
      coefficients_x_.emplace_back(blk_num_max, blk_num_max, weight);
      coefficients_x_.emplace_back(blk_num_min, blk_num_max, -weight);
      coefficients_x_.emplace_back(blk_num_max, blk_num_min, -weight);
      double offset_diff = (offset_max - offset_min) * weight;
      bx[blk_num_min] += offset_diff;
      bx[blk_num_max] -= offset_diff;
    }
  }

  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      coefficients_x_.emplace_back(i, i, 1);
      bx[i] = blocks[i].LLX();
    } else {
      if (blocks[i].LLX() < ckt_ptr_->RegionLLX()
          || blocks[i].URX() > ckt_ptr_->RegionURX()) {
        coefficients_x_.emplace_back(i, i, center_weight);
        bx[i] += weight_center_x;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_x_.capacity(),
      "WARNING: x coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_x_.size()
  );

  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}

void HpwlHpwlOptimizer::BuildProblemY() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  std::vector<Net> &nets = ckt_ptr_->Nets();
  size_t coefficients_capacity = coefficients_y_.capacity();
  coefficients_y_.resize(0);

  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y =
      (ckt_ptr_->RegionLLY() + ckt_ptr_->RegionURY()) / 2.0 * center_weight;
  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();

  for (auto &net : nets) {
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) continue;
    double inv_p = net.InvP();
    net.UpdateMaxMinIdY();
    int max_pin_index = net.MaxBlkPinIdY();
    int min_pin_index = net.MinBlkPinIdY();

    int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
    double pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
    bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
    double offset_max = net.BlockPins()[max_pin_index].OffsetY();

    int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
    double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
    bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
    double offset_min = net.BlockPins()[min_pin_index].OffsetY();

    double distance = std::fabs(pin_loc_min - pin_loc_max);
    //weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
    //weight = inv_p / (distance + height_epsilon_) * weight_adjust;
    double weight = inv_p / (distance + height_epsilon_);
    if (!is_movable_min && is_movable_max) {
      by[blk_num_max] += (pin_loc_min - offset_max) * weight;
      coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
    } else if (is_movable_min && !is_movable_max) {
      by[blk_num_min] += (pin_loc_max - offset_max) * weight;
      coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
    } else if (is_movable_min && is_movable_max) {
      coefficients_y_.emplace_back(blk_num_min, blk_num_min, weight);
      coefficients_y_.emplace_back(blk_num_max, blk_num_max, weight);
      coefficients_y_.emplace_back(blk_num_min, blk_num_max, -weight);
      coefficients_y_.emplace_back(blk_num_max, blk_num_min, -weight);
      double offset_diff = (offset_max - offset_min) * weight;
      by[blk_num_min] += offset_diff;
      by[blk_num_max] -= offset_diff;
    }
  }
  for (int i = 0; i < sz;
       ++i) { // add the diagonal non-zero element for fixed blocks
    if (blocks[i].IsFixed()) {
      coefficients_y_.emplace_back(i, i, 1);
      by[i] = blocks[i].LLY();
    } else {
      if (blocks[i].LLY() < ckt_ptr_->RegionLLY()
          || blocks[i].URY() > ckt_ptr_->RegionURY()) {
        coefficients_y_.emplace_back(i, i, center_weight);
        by[i] += weight_center_y;
      }
    }
  }

  DaliWarns(
      coefficients_capacity != coefficients_y_.capacity(),
      "WARNING: coefficients capacity changed!\n"
          << "\told capacity: " << coefficients_capacity << "\n"
          << "\tnew capacity: " << coefficients_y_.size()
  );

  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

void HpwlHpwlOptimizer::UpdateAnchorAlpha() {
  alpha = 0.005 * cur_iter_;
}

void StarHpwlHpwlOptimizer::InitializeDriverLoadPairs() {
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int sz = static_cast<int>(blocks.size());

  pair_connect.resize(sz);
  for (auto &blk_pair : blk_pair_net_list_) {
    int num0 = blk_pair.blk_num0;
    int num1 = blk_pair.blk_num1;
    //BOOST_LOG_TRIVIAL(info)   << num0 << " " << num1 << "\n";
    pair_connect[num0].push_back(&blk_pair);
    pair_connect[num1].push_back(&blk_pair);
  }

  diagonal_pair.clear();
  diagonal_pair.reserve(sz);
  for (int i = 0; i < sz; ++i) {
    diagonal_pair.emplace_back(i, i);
    pair_connect[i].push_back(&(diagonal_pair[i]));
    std::sort(
        pair_connect[i].begin(),
        pair_connect[i].end(),
        [](const BlkPairNets *blk_pair0,
           const BlkPairNets *blk_pair1) {
          if (blk_pair0->blk_num0 == blk_pair1->blk_num0) {
            return blk_pair0->blk_num1 < blk_pair1->blk_num1;
          } else {
            return blk_pair0->blk_num0 < blk_pair1->blk_num0;
          }
        }
    );
  }

  std::vector<int> row_size(sz, 1);
  for (int i = 0; i < sz; ++i) {
    for (auto &blk_pair : pair_connect[i]) {
      int num0 = blk_pair->blk_num0;
      int num1 = blk_pair->blk_num1;
      if (num0 == num1) continue;
      if (blocks[num0].IsMovable() && blocks[num1].IsMovable()) {
        ++row_size[i];
      }
    }
  }
  Ax.reserve(row_size);
  SpMat_diag_x.resize(sz);
  for (int i = 0; i < sz; ++i) {
    for (auto &blk_pair : pair_connect[i]) {
      int num0 = blk_pair->blk_num0;
      int num1 = blk_pair->blk_num1;
      if (blocks[num0].IsMovable() && blocks[num1].IsMovable()) {
        int col;
        if (num0 == i) {
          col = num1;
        } else {
          col = num0;
        }
        Ax.insertBackUncompressed(i, col) = 0;
      }
    }
    PairEgId key = std::make_pair(0, 0);
    for (SpMat::InnerIterator it(Ax, i); it; ++it) {
      EgId row = it.row();
      EgId col = it.col();
      if (row == col) {
        SpMat_diag_x[i] = it;
        continue;
      }
      key.first = std::max(row, col);
      key.second = std::min(row, col);
      if (blk_pair_map_.find(key) == blk_pair_map_.end()) {
        BOOST_LOG_TRIVIAL(info) << row << " " << col << std::endl;
        DaliExpects(
            false,
            "Cannot find block pair in the database, something wrong happens\n"
        );
      }
      EgId pair_index = blk_pair_map_[key];
      if (blk_pair_net_list_[pair_index].blk_num0 == row) {
        blk_pair_net_list_[pair_index].it01x = it;
      } else {
        blk_pair_net_list_[pair_index].it10x = it;
      }
    }
  }

  Ay.reserve(row_size);
  SpMat_diag_y.resize(sz);
  for (int i = 0; i < sz; ++i) {
    for (auto &blk_pair : pair_connect[i]) {
      int num0 = blk_pair->blk_num0;
      int num1 = blk_pair->blk_num1;
      if (blocks[num0].IsMovable() && blocks[num1].IsMovable()) {
        int col;
        if (num0 == i) {
          col = num1;
        } else {
          col = num0;
        }
        Ay.insertBackUncompressed(i, col) = 0;
      }
    }
    PairEgId key = std::make_pair(0, 0);
    for (SpMat::InnerIterator it(Ay, i); it; ++it) {
      EgId row = it.row();
      EgId col = it.col();
      if (row == col) {
        SpMat_diag_y[i] = it;
        continue;
      }
      key.first = std::max(row, col);
      key.second = std::min(row, col);
      if (blk_pair_map_.find(key) == blk_pair_map_.end()) {
        BOOST_LOG_TRIVIAL(info) << row << " " << col << std::endl;
        DaliExpects(
            false,
            "Cannot find block pair in the database, something wrong happens\n"
        );
      }
      EgId pair_index = blk_pair_map_[key];
      if (blk_pair_net_list_[pair_index].blk_num0 == row) {
        blk_pair_net_list_[pair_index].it01y = it;
      } else {
        blk_pair_net_list_[pair_index].it10y = it;
      }
    }
  }
}

void StarHpwlHpwlOptimizer::Initialize() {
  // set a small value for net weight dividend to improve numerical stability
  UpdateEpsilon();

  // initialize containers to store HPWL after each iteration
  lower_bound_hpwl_x_.clear();
  lower_bound_hpwl_y_.clear();
  lower_bound_hpwl_.clear();

  size_t sz = ckt_ptr_->Blocks().size();
  ADx.resize(sz);
  ADy.resize(sz);
  Ax_row_size.assign(sz, 0);
  Ax_row_size.assign(sz, 0);

  EgId eigen_sz = static_cast<EgId>(ckt_ptr_->Blocks().size());
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
  auto &nets = ckt_ptr_->Nets();
  for (auto &net : nets) {
    size_t net_sz = net.PinCnt();
    // if a net has size n, then in total, there will be (2(n-2)+1)*4 non-zero entries for the matrix
    if (net_sz > 1) {
      coefficient_size += (2 * (net_sz - 2) + 1) * 4;
    }
  }
  // this is to reserve space for anchor, because each block may need an anchor
  coefficient_size += sz;
  // this is to reserve space for anchor in the center of the placement region
  coefficient_size += sz;
  coefficients_x_.reserve(coefficient_size);
  Ax.reserve(static_cast<EgId>(coefficient_size));
  coefficients_y_.reserve(coefficient_size);
  Ay.reserve(static_cast<EgId>(coefficient_size));

  InitializeDriverLoadPairs();
}

void StarHpwlHpwlOptimizer::BuildProblemX() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  UpdateMaxMinX();

  int sz = static_cast<int>(bx.size());
  for (int i = 0; i < sz; ++i) {
    bx[i] = 0;
  }

  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();
  std::vector<BlkPairNets> &blk_pair_net_list = blk_pair_net_list_;
  int pair_sz = blk_pair_net_list.size();
//#pragma omp parallel for
  for (int i = 0; i < pair_sz; ++i) {
    BlkPairNets &blk_pair = blk_pair_net_list[i];
    blk_pair.ClearX();
    for (auto &edge : blk_pair.edges) {
      Net &net = *(edge.net);
      int d = edge.d;
      int l = edge.l;
      int driver_blk_num = net.BlockPins()[d].BlkId();
      double driver_pin_loc = net.BlockPins()[d].AbsX();
      bool driver_is_movable = net.BlockPins()[d].BlkPtr()->IsMovable();
      double driver_offset = net.BlockPins()[d].OffsetX();

      int load_blk_num = net.BlockPins()[l].BlkId();
      double load_pin_loc = net.BlockPins()[l].AbsX();
      bool load_is_movable = net.BlockPins()[l].BlkPtr()->IsMovable();
      double load_offset = net.BlockPins()[l].OffsetX();

      int max_pin_index = net.MaxBlkPinIdX();
      int min_pin_index = net.MinBlkPinIdX();

      int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
      double pin_loc_max = net.BlockPins()[max_pin_index].AbsX();
      //bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
      //double offset_max = net.BlockPins()[max_pin_index].OffsetX();

      int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
      double pin_loc_min = net.BlockPins()[min_pin_index].AbsX();
      //bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
      //double offset_min = net.BlockPins()[min_pin_index].OffsetX();

      double distance = std::fabs(load_pin_loc - driver_pin_loc);
      //double weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
      double inv_p = net.InvP();
      double weight = inv_p / (distance + width_epsilon_);
      //weight *= weight_adjust;
      //weight = inv_p / (distance + width_epsilon_);
      double adjust = 1.0;
      if (driver_blk_num == blk_num_max) {
        adjust = (driver_pin_loc - load_pin_loc)
            / (driver_pin_loc - pin_loc_min + width_epsilon_);
      } else if (driver_blk_num == blk_num_min) {
        adjust = (load_pin_loc - driver_pin_loc)
            / (pin_loc_max - driver_pin_loc + width_epsilon_);
      } else {
        if (load_pin_loc > driver_pin_loc) {
          adjust = (load_pin_loc - driver_pin_loc)
              / (pin_loc_max - driver_pin_loc + width_epsilon_);
        } else {
          adjust = (driver_pin_loc - load_pin_loc)
              / (driver_pin_loc - pin_loc_min + width_epsilon_);
        }
      }
      //int exponent = cur_iter_/5;
      //weight *= std::pow(adjust, exponent);
      weight *= adjust;
      if (!load_is_movable && driver_is_movable) {
        if (driver_blk_num == blk_pair.blk_num0) {
          blk_pair.b0x += (load_pin_loc - driver_offset) * weight;
          blk_pair.e00x += weight;
        } else {
          blk_pair.b1x += (load_pin_loc - driver_offset) * weight;
          blk_pair.e11x += weight;
        }
      } else if (load_is_movable && !driver_is_movable) {
        if (load_blk_num == blk_pair.blk_num0) {
          blk_pair.b0x += (driver_pin_loc - driver_offset) * weight;
          blk_pair.e00x += weight;
        } else {
          blk_pair.b1x += (driver_pin_loc - driver_offset) * weight;
          blk_pair.e11x += weight;
        }
      } else if (load_is_movable && driver_is_movable) {
        double offset_diff = (driver_offset - load_offset) * weight;
        blk_pair.e00x += weight;
        blk_pair.e01x -= weight;
        blk_pair.e10x -= weight;
        blk_pair.e11x += weight;
        if (driver_blk_num == blk_pair.blk_num0) {
          blk_pair.b0x -= offset_diff;
          blk_pair.b1x += offset_diff;
        } else {
          blk_pair.b0x += offset_diff;
          blk_pair.b1x -= offset_diff;
        }
      }
    }
    blk_pair.WriteX();
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x =
      (ckt_ptr_->RegionLLX() + ckt_ptr_->RegionURX()) / 2.0 * center_weight;
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
//#pragma omp parallel for
  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      SpMat_diag_x[i].valueRef() = 1;
      bx[i] = blocks[i].LLX();
    } else {
      double diag_val = 0;
      double b = 0;
      for (auto &blk_pair : pair_connect[i]) {
        if (i == blk_pair->blk_num0) {
          diag_val += blk_pair->e00x;
          b += blk_pair->b0x;
        } else {
          diag_val += blk_pair->e11x;
          b += blk_pair->b1x;
        }
      }
      SpMat_diag_x[i].valueRef() = diag_val;
      bx[i] = b;
      if (blocks[i].LLX() < ckt_ptr_->RegionLLX()
          || blocks[i].URX() > ckt_ptr_->RegionURX()) {
        SpMat_diag_x[i].valueRef() += center_weight;
        bx[i] += weight_center_x;
      }
    }
  }

  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}

void StarHpwlHpwlOptimizer::BuildProblemY() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();
  UpdateMaxMinY();

  int sz = static_cast<int>(by.size());
  for (int i = 0; i < sz; ++i) {
    by[i] = 0;
  }

  //double decay_length = decay_factor * ckt_ptr_->AveBlkHeight();
  std::vector<BlkPairNets> &blk_pair_net_list = blk_pair_net_list_;
  int pair_sz = blk_pair_net_list.size();
//#pragma omp parallel for
  for (int i = 0; i < pair_sz; ++i) {
    BlkPairNets &blk_pair = blk_pair_net_list[i];
    blk_pair.ClearY();
    for (auto &edge : blk_pair.edges) {
      Net &net = *(edge.net);
      int d = edge.d;
      int l = edge.l;
      int driver_blk_num = net.BlockPins()[d].BlkId();
      double driver_pin_loc = net.BlockPins()[d].AbsY();
      bool driver_is_movable = net.BlockPins()[d].BlkPtr()->IsMovable();
      double driver_offset = net.BlockPins()[d].OffsetY();

      int load_blk_num = net.BlockPins()[l].BlkId();
      double load_pin_loc = net.BlockPins()[l].AbsY();
      bool load_is_movable = net.BlockPins()[l].BlkPtr()->IsMovable();
      double load_offset = net.BlockPins()[l].OffsetY();

      int max_pin_index = net.MaxBlkPinIdY();
      int min_pin_index = net.MinBlkPinIdY();

      int blk_num_max = net.BlockPins()[max_pin_index].BlkId();
      double pin_loc_max = net.BlockPins()[max_pin_index].AbsY();
      //bool is_movable_max = net.BlockPins()[max_pin_index].BlkPtr()->IsMovable();
      //double offset_max = net.BlockPins()[max_pin_index].OffsetY();

      int blk_num_min = net.BlockPins()[min_pin_index].BlkId();
      double pin_loc_min = net.BlockPins()[min_pin_index].AbsY();
      //bool is_movable_min = net.BlockPins()[min_pin_index].BlkPtr()->IsMovable();
      //double offset_min = net.BlockPins()[min_pin_index].OffsetY();

      double distance = std::fabs(load_pin_loc - driver_pin_loc);
      //double weight_adjust = base_factor + adjust_factor * (1 - exp(-distance / decay_length));
      double inv_p = net.InvP();
      double weight = inv_p / (distance + height_epsilon_);
      //weight *= weight_adjust;
      //weight = inv_p / (distance + width_epsilon_);
      double adjust = 1.0;
      if (driver_blk_num == blk_num_max) {
        adjust = (driver_pin_loc - load_pin_loc)
            / (driver_pin_loc - pin_loc_min + height_epsilon_);
      } else if (driver_blk_num == blk_num_min) {
        adjust = (load_pin_loc - driver_pin_loc)
            / (pin_loc_max - driver_pin_loc + height_epsilon_);
      } else {
        if (load_pin_loc > driver_pin_loc) {
          adjust = (load_pin_loc - driver_pin_loc)
              / (pin_loc_max - driver_pin_loc + height_epsilon_);
        } else {
          adjust = (driver_pin_loc - load_pin_loc)
              / (driver_pin_loc - pin_loc_min + height_epsilon_);
        }
      }
      //int exponent = cur_iter_/5;
      //weight *= std::pow(adjust, exponent);
      weight *= adjust;
      if (!load_is_movable && driver_is_movable) {
        if (driver_blk_num == blk_pair.blk_num0) {
          blk_pair.b0y += (load_pin_loc - driver_offset) * weight;
          blk_pair.e00y += weight;
        } else {
          blk_pair.b1y += (load_pin_loc - driver_offset) * weight;
          blk_pair.e11y += weight;
        }
      } else if (load_is_movable && !driver_is_movable) {
        if (load_blk_num == blk_pair.blk_num0) {
          blk_pair.b0y += (driver_pin_loc - driver_offset) * weight;
          blk_pair.e00y += weight;
        } else {
          blk_pair.b1y += (driver_pin_loc - driver_offset) * weight;
          blk_pair.e11y += weight;
        }
      } else if (load_is_movable && driver_is_movable) {
        double offset_diff = (driver_offset - load_offset) * weight;
        blk_pair.e00y += weight;
        blk_pair.e01y -= weight;
        blk_pair.e10y -= weight;
        blk_pair.e11y += weight;
        if (driver_blk_num == blk_pair.blk_num0) {
          blk_pair.b0y -= offset_diff;
          blk_pair.b1y += offset_diff;
        } else {
          blk_pair.b0y += offset_diff;
          blk_pair.b1y -= offset_diff;
        }
      }
    }
    blk_pair.WriteY();
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_y =
      (ckt_ptr_->RegionLLY() + ckt_ptr_->RegionURY()) / 2.0 * center_weight;
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
//#pragma omp parallel for
  for (int i = 0; i < sz; ++i) {
    if (blocks[i].IsFixed()) {
      SpMat_diag_y[i].valueRef() = 1;
      by[i] = blocks[i].LLY();
    } else {
      double diag_val = 0;
      double b = 0;
      for (auto &blk_pair : pair_connect[i]) {
        if (i == blk_pair->blk_num0) {
          diag_val += blk_pair->e00y;
          b += blk_pair->b0y;
        } else {
          diag_val += blk_pair->e11y;
          b += blk_pair->b1y;
        }
      }
      SpMat_diag_y[i].valueRef() = diag_val;
      by[i] = b;
      if (blocks[i].LLY() < ckt_ptr_->RegionLLY()
          || blocks[i].URY() > ckt_ptr_->RegionURY()) {
        SpMat_diag_y[i].valueRef() += center_weight;
        by[i] += weight_center_y;
      }
    }
  }

  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

void StarHpwlHpwlOptimizer::BuildProblemWithAnchorX() {
  UpdateMaxMinX();
  BuildProblemX();

  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLX();
    pin_loc1 = x_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    bx[i] += pin_loc1 * weight;
    SpMat_diag_x[i].valueRef() += weight;
  }
  elapsed_time.RecordEndTime();
  tot_triplets_time_x += elapsed_time.GetWallTime();
}

void StarHpwlHpwlOptimizer::BuildProblemWithAnchorY() {
  UpdateMaxMinY();
  BuildProblemY();

  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (int i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLY();
    pin_loc1 = y_anchor[i];
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + width_epsilon_);
    by[i] += pin_loc1 * weight;
    SpMat_diag_y[i].valueRef() += weight;
  }
  elapsed_time.RecordEndTime();
  tot_triplets_time_y += elapsed_time.GetWallTime();
}

double StarHpwlHpwlOptimizer::OptimizeQuadraticMetricX(double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  int sz = vx.size();
  std::vector<Block> &blocks = ckt_ptr_->Blocks();

  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_x_.compute(Ax); // Ax * vx = bx
  for (int i = 0; i < max_rounds; ++i) {
    vx = cg_x_.solveWithGuess(bx, vx);
    for (int num = 0; num < sz; ++num) {
      blocks[num].SetLLX(vx[num]);
    }
    double evaluate_result = ckt_ptr_->WeightedHPWLX();
    eval_history.push_back(evaluate_result);
    //BOOST_LOG_TRIVIAL(info)  <<"  %d WeightedHPWLX: %e\n", i, evaluate_result);
    if (eval_history.size() >= 3) {
      bool is_converge =
          IsSeriesConverge(eval_history, 3, cg_stop_criterion);
      bool is_oscillate = IsSeriesOscillate(eval_history, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        BOOST_LOG_TRIVIAL(trace) << "oscillation detected\n";
        break;
      }
    }
  }
  BOOST_LOG_TRIVIAL(trace)
    << "      Metric optimization in X, sequence: " << eval_history << "\n";
  elapsed_time.RecordEndTime();
  tot_cg_solver_time_x += elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
  for (int num = 0; num < sz; ++num) {
    blocks[num].SetLLX(vx[num]);
  }
  elapsed_time.RecordEndTime();
  tot_loc_update_time_x += elapsed_time.GetWallTime();

  DaliExpects(
      !eval_history.empty(),
      "Cannot return a valid value because the result is not evaluated!"
  );
  return eval_history.back();
}

double StarHpwlHpwlOptimizer::OptimizeQuadraticMetricY(double cg_stop_criterion) {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  int sz = vx.size();
  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  std::vector<double> eval_history;
  int max_rounds = cg_iteration_max_num_ / cg_iteration_;
  cg_y_.compute(Ay);
  for (int i = 0; i < max_rounds; ++i) {
    vy = cg_y_.solveWithGuess(by, vy);
    for (int num = 0; num < sz; ++num) {
      block_list[num].SetLLY(vy[num]);
    }
    double evaluate_result = ckt_ptr_->WeightedHPWLY();
    eval_history.push_back(evaluate_result);
    //BOOST_LOG_TRIVIAL(info)  <<"  %d WeightedHPWLY: %e\n", i, evaluate_result);
    if (eval_history.size() >= 3) {
      bool is_converge =
          IsSeriesConverge(eval_history, 3, cg_stop_criterion);
      bool is_oscillate = IsSeriesOscillate(eval_history, 5);
      if (is_converge) {
        break;
      }
      if (is_oscillate) {
        BOOST_LOG_TRIVIAL(trace) << "oscillation detected\n";
        break;
      }
    }
  }
  BOOST_LOG_TRIVIAL(trace) << "      Metric optimization in Y, sequence: "
                           << eval_history << "\n";
  elapsed_time.RecordEndTime();
  tot_cg_solver_time_y += elapsed_time.GetWallTime();

  elapsed_time.RecordStartTime();
  for (int num = 0; num < sz; ++num) {
    block_list[num].SetLLY(vy[num]);
  }
  elapsed_time.RecordEndTime();
  tot_loc_update_time_y += elapsed_time.GetWallTime();

  DaliExpects(!eval_history.empty(),
              "Cannot return a valid value because the result is not evaluated!");
  return eval_history.back();
}

void StarHpwlHpwlOptimizer::UpdateAnchorAlpha() {
  alpha = 0.002 * cur_iter_;
}

} // dali
