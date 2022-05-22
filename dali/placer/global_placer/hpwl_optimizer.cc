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

#include "dali/common/logging.h"
#include "dali/common/timing.h"

namespace dali {

HpwlOptimizer::HpwlOptimizer(Circuit *ckt_ptr) {
  DaliExpects(ckt_ptr != nullptr, "Circuit is a nullptr?");
  ckt_ptr_ = ckt_ptr;
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
  lower_bound_hpwlx_.clear();
  lower_bound_hpwly_.clear();
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
  double wall_time = get_wall_time();

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

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_x += wall_time;
}

void B2BHpwlOptimizer::BuildProblemY() {
  double wall_time = get_wall_time();

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

  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_y += wall_time;
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
  double wall_time = get_wall_time();
  Ax.setFromTriplets(coefficients_x_.begin(), coefficients_x_.end());
  wall_time = get_wall_time() - wall_time;
  tot_matrix_from_triplets_x += wall_time;

  int sz = vx.size();
  std::vector<Block> &blocks = ckt_ptr_->Blocks();

  wall_time = get_wall_time();
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
  wall_time = get_wall_time() - wall_time;
  tot_cg_solver_time_x += wall_time;

  wall_time = get_wall_time();

  for (int num = 0; num < sz; ++num) {
    blocks[num].SetLLX(vx[num]);
  }
  wall_time = get_wall_time() - wall_time;
  tot_loc_update_time_x += wall_time;

  DaliExpects(
      !eval_history.empty(),
      "Cannot return a valid value because the result is not evaluated!"
  );
  return eval_history.back();
}

double B2BHpwlOptimizer::OptimizeQuadraticMetricY(double cg_stop_criterion) {
  double wall_time = get_wall_time();
  Ay.setFromTriplets(coefficients_y_.begin(), coefficients_y_.end());
  wall_time = get_wall_time() - wall_time;
  tot_matrix_from_triplets_y += wall_time;

  int sz = vx.size();
  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  wall_time = get_wall_time();
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
  wall_time = get_wall_time() - wall_time;
  tot_cg_solver_time_y += wall_time;

  wall_time = get_wall_time();
  for (int num = 0; num < sz; ++num) {
    block_list[num].SetLLY(vy[num]);
  }
  wall_time = get_wall_time() - wall_time;
  tot_loc_update_time_y += wall_time;

  DaliExpects(!eval_history.empty(),
              "Cannot return a valid value because the result is not evaluated!");
  return eval_history.back();
}

void B2BHpwlOptimizer::PullBlockBackToRegion() {
  int sz = vx.size();
  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  double blk_hi_bound_x;
  double blk_hi_bound_y;

  for (int num = 0; num < sz; ++num) {
    if (block_list[num].IsMovable()) {
      if (vx[num] < ckt_ptr_->RegionLLX()) {
        vx[num] = ckt_ptr_->RegionLLX();
      }
      blk_hi_bound_x = ckt_ptr_->RegionURX() - block_list[num].Width();
      if (vx[num] > blk_hi_bound_x) {
        vx[num] = blk_hi_bound_x;
      }

      if (vy[num] < ckt_ptr_->RegionLLY()) {
        vy[num] = ckt_ptr_->RegionLLY();
      }
      blk_hi_bound_y = ckt_ptr_->RegionURY() - block_list[num].Height();
      if (vy[num] > blk_hi_bound_y) {
        vy[num] = blk_hi_bound_y;
      }
    }
  }

  for (int num = 0; num < sz; ++num) {
    block_list[num].SetLoc(vx[num], vy[num]);
  }
}

double B2BHpwlOptimizer::QuadraticPlacement(double net_model_update_stop_criterion) {
  //omp_set_nested(1);
  omp_set_dynamic(0);
  int avail_threads_num = omp_get_max_threads();
  double wall_time = get_wall_time();

//#pragma omp parallel num_threads(std::min(omp_get_max_threads(), 2))
  {
    BOOST_LOG_TRIVIAL(trace)
      << "OpenMP threads, " << omp_get_num_threads() << "\n";
    if (omp_get_thread_num() == 0) {
      omp_set_num_threads(avail_threads_num / 2);
      BOOST_LOG_TRIVIAL(trace)
        << "threads in branch x: "
        << omp_get_max_threads()
        << " Eigen threads: " << Eigen::nbThreads()
        << "\n";

      std::vector<Block> &block_list = ckt_ptr_->Blocks();
      for (size_t i = 0; i < block_list.size(); ++i) {
        vx[i] = block_list[i].LLX();
      }

      std::vector<double> eval_history_x;
      int b2b_update_it_x = 0;
      for (b2b_update_it_x = 0; b2b_update_it_x < b2b_update_max_iteration_;
           ++b2b_update_it_x) {
        BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
        BuildProblemX();
        double evaluate_result = OptimizeQuadraticMetricX(cg_stop_criterion_);
        eval_history_x.push_back(evaluate_result);
        if (eval_history_x.size() >= 3) {
          bool is_converge = IsSeriesConverge(
              eval_history_x,
              3,
              net_model_update_stop_criterion
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
          "Cannot return a valid value because the result is not evaluated!"
      );
      lower_bound_hpwlx_.push_back(eval_history_x.back());
    }

    if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
      omp_set_num_threads(avail_threads_num / 2);
      BOOST_LOG_TRIVIAL(trace)
        << "threads in branch y: "
        << omp_get_max_threads()
        << " Eigen threads: " << Eigen::nbThreads()
        << "\n";

      std::vector<Block> &block_list = ckt_ptr_->Blocks();
      for (size_t i = 0; i < block_list.size(); ++i) {
        vy[i] = block_list[i].LLY();
      }
      std::vector<double> eval_history_y;
      int b2b_update_it_y = 0;
      for (b2b_update_it_y = 0;
           b2b_update_it_y < b2b_update_max_iteration_;
           ++b2b_update_it_y) {
        BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
        BuildProblemY();
        double evaluate_result =
            OptimizeQuadraticMetricY(cg_stop_criterion_);
        eval_history_y.push_back(evaluate_result);
        if (eval_history_y.size() >= 3) {
          bool is_converge = IsSeriesConverge(
              eval_history_y, 3,
              net_model_update_stop_criterion
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
      DaliExpects(
          !eval_history_y.empty(),
          "Cannot return a valid value because the result is not evaluated!"
      );
      lower_bound_hpwly_.push_back(eval_history_y.back());
    }
  }

  PullBlockBackToRegion();

  BOOST_LOG_TRIVIAL(info) << "Initial Placement Complete\n";

  wall_time = get_wall_time() - wall_time;
  tot_cg_time += wall_time;
  //omp_set_nested(0);

  if (is_dump_) DumpResult("cg_result_0.txt");
  BackUpBlockLocation();
  return lower_bound_hpwlx_.back() + lower_bound_hpwly_.back();
}

void B2BHpwlOptimizer::UpdateAnchorLocation() {
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
  if (0 <= cur_iter_ && cur_iter_ < 5) {
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

  double wall_time = get_wall_time();

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
  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_x += wall_time;
}
void B2BHpwlOptimizer::BuildProblemWithAnchorY() {
  UpdateMaxMinY();
  BuildProblemY();

  double wall_time = get_wall_time();

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
  wall_time = get_wall_time() - wall_time;
  tot_triplets_time_y += wall_time;
}

void B2BHpwlOptimizer::BackUpBlockLocation() {
  std::vector<Block> &block_list = ckt_ptr_->Blocks();
  int sz = static_cast<int>(block_list.size());
  for (int i = 0; i < sz; ++i) {
    x_anchor[i] = block_list[i].LLX();
    y_anchor[i] = block_list[i].LLY();
  }
}

double B2BHpwlOptimizer::QuadraticPlacementWithAnchor(double net_model_update_stop_criterion) {
  //omp_set_nested(1);
  omp_set_dynamic(0);
  int avail_threads_num = omp_get_max_threads();
  //BOOST_LOG_TRIVIAL(info)   << "total threads: " << avail_threads_num << "\n";
  double wall_time = get_wall_time();

  std::vector<Block> &block_list = ckt_ptr_->Blocks();

  UpdateAnchorLocation();
  UpdateAnchorAlpha();
  //UpdateAnchorNetWeight();
  BOOST_LOG_TRIVIAL(trace) << "alpha: " << alpha << "\n";

//#pragma omp parallel num_threads(std::min(omp_get_max_threads(), 2))
  {
    BOOST_LOG_TRIVIAL(trace)
      << "OpenMP threads, " << omp_get_num_threads() << "\n";
    if (omp_get_thread_num() == 0) {
      omp_set_num_threads(avail_threads_num / 2);
      BOOST_LOG_TRIVIAL(trace)
        << "threads in branch x: "
        << omp_get_max_threads()
        << " Eigen threads: " << Eigen::nbThreads()
        << "\n";
      for (size_t i = 0; i < block_list.size(); ++i) {
        vx[i] = block_list[i].LLX();
      }
      std::vector<double> eval_history_x;
      int b2b_update_it_x = 0;
      for (b2b_update_it_x = 0;
           b2b_update_it_x < b2b_update_max_iteration_;
           ++b2b_update_it_x) {
        BOOST_LOG_TRIVIAL(trace) << "    Iterative net model update\n";
        BuildProblemWithAnchorX();
        double evaluate_result = OptimizeQuadraticMetricX(cg_stop_criterion_);
        eval_history_x.push_back(evaluate_result);
        //BOOST_LOG_TRIVIAL(trace) << "\tIterative net model update, WeightedHPWLX: " << evaluate_result << "\n";
        if (eval_history_x.size() >= 3) {
          bool is_converge = IsSeriesConverge(
              eval_history_x,
              3,
              net_model_update_stop_criterion
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
      DaliExpects(!eval_history_x.empty(),
                  "Cannot return a valid value because the result is not evaluated!");
      lower_bound_hpwlx_.push_back(eval_history_x.back());
    }
    if (omp_get_thread_num() == 1 || omp_get_num_threads() == 1) {
      omp_set_num_threads(avail_threads_num / 2);
      BOOST_LOG_TRIVIAL(trace)
        << "threads in branch y: "
        << omp_get_max_threads()
        << " Eigen threads: " << Eigen::nbThreads()
        << "\n";
      for (size_t i = 0; i < block_list.size(); ++i) {
        vy[i] = block_list[i].LLY();
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
              net_model_update_stop_criterion
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
      lower_bound_hpwly_.push_back(eval_history_y.back());
    }
  }

  PullBlockBackToRegion();

  BOOST_LOG_TRIVIAL(debug) << "Quadratic Placement With Anchor Complete\n";

//  for (size_t i = 0; i < 10; ++i) {
//    BOOST_LOG_TRIVIAL(info)   << vx[i] << "  " << vy[i] << "\n";
//  }

  wall_time = get_wall_time() - wall_time;
  tot_cg_time += wall_time;
  //omp_set_nested(0);

  if (is_dump_) DumpResult("cg_result_" + std::to_string(cur_iter_) + ".txt");
  BackUpBlockLocation();
  return lower_bound_hpwlx_.back() + lower_bound_hpwly_.back();
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

} // dali
