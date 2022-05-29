/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
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
#include "global_placer.h"

#include <cmath>
#include <omp.h>

#include <algorithm>

#include "dali/common/helper.h"
#include "dali/common/logging.h"

namespace dali {

/****
 * @brief Set the maximum number of iterations
 *
 * @param max_iter: maximum number of iterations
 */
void GlobalPlacer::SetMaxIteration(int max_iter) {
  DaliExpects(max_iter >= 0, "negative number of iterations?");
  max_iter_ = max_iter;
}

/****
 * @brief Load a configuration file for this placer
 *
 * @param config_file: name of the configuration file
 */
void GlobalPlacer::LoadConf(std::string const &config_file) {
  config_read(config_file.c_str());
}

void GlobalPlacer::CheckOptimizerAndLegalizer() {
  if (optimizer_ == nullptr) {
    optimizer_ = new B2BHpwlOptimizer(ckt_ptr_);
  }
  if (legalizer_ == nullptr) {
    legalizer_ = new LookAheadLegalizer(ckt_ptr_);
  }
}

/****
 * @brief Initialize the location of blocks uniformly across the placement region
 */
void GlobalPlacer::InitializeBlockLocationUniform() {
  BlockLocationInitialization_(0, -1.0);
}

/****
 * @brief Initialize the location of blocks around the center of the placement
 * region using normal distribution
 */
void GlobalPlacer::InitializeBlockLocationNormal(double std_dev) {
  BlockLocationInitialization_(1, std_dev);
}

/****
* Returns true of false indicating the convergence of the global placement.
* Stopping criteria (SimPL, option 1):
*    (a). the gap is reduced to 25% of the gap in the tenth iteration and upper-bound solution stops improving
*    (b). the gap is smaller than 10% of the gap in the tenth iteration
* Stopping criteria (POLAR, option 2):
*    the gap between lower bound wirelength and upper bound wirelength is less than 8%
* ****/
bool GlobalPlacer::IsPlacementConverge() {
  bool res = false;
  if (convergence_criteria_ == 1) {
    // (a) and (b) requires at least 10 iterations
    if (lower_bound_hpwl_.size() <= 10) {
      res = false;
    } else {
      double tenth_gap = upper_bound_hpwl_[9] - lower_bound_hpwl_[9];
      double last_gap = upper_bound_hpwl_.back() - lower_bound_hpwl_.back();
      double gap_ratio = last_gap / tenth_gap;
      if (gap_ratio < 0.1) { // (a)
        res = true;
      } else if (gap_ratio < 0.25) { // (b)
        res = IsSeriesConverge(
            upper_bound_hpwl_,
            3,
            simpl_LAL_converge_criterion_
        );
      } else {
        res = false;
      }
    }
  } else if (convergence_criteria_ == 2) {
    if (lower_bound_hpwl_.empty()) {
      res = false;
    } else {
      double lower_bound = lower_bound_hpwl_.back();
      double upper_bound = upper_bound_hpwl_.back();
      res = (lower_bound < upper_bound)
          && (upper_bound / lower_bound - 1 < polar_converge_criterion_);
    }
  } else {
    DaliExpects(false, "Unknown Convergence Criteria!");
  }

  return res;
}

bool GlobalPlacer::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  if (ckt_ptr_->Blocks().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Block list empty? Skip global placement!\n"
      << "\033[0;36m Global Placement complete\033[0m\n";
    return true;
  }
  if (ckt_ptr_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Net list empty? Skip global placement!\n"
      << "\033[0;36m Global Placement complete\033[0m\n";
    return true;
  }

  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start global placement\n";

  SanityCheck();
  CheckOptimizerAndLegalizer();
  optimizer_->Initialize();
  legalizer_->Initialize(PlacementDensity());
  //InitializeBlockLocationNormal();
  InitializeBlockLocationUniform();

  // initial placement
  BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";
  double eval_res = optimizer_->QuadraticPlacement(net_model_update_stop_criterion_);
  lower_bound_hpwl_.push_back(eval_res);
  eval_res = legalizer_->LookAheadLegalization();
  upper_bound_hpwl_.push_back(eval_res);
  BOOST_LOG_TRIVIAL(info)
    << "It " << cur_iter_ << ": \t"
    << std::scientific << std::setprecision(4)
    << lower_bound_hpwl_.back() << " "
    << upper_bound_hpwl_.back() << "\n";

  for (cur_iter_ = 1; cur_iter_ < max_iter_; ++cur_iter_) {
    BOOST_LOG_TRIVIAL(debug)
      << "----------------------------------------------\n";
    BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration\n";
    optimizer_->SetIteration(cur_iter_);
    eval_res = optimizer_->QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
    lower_bound_hpwl_.push_back(eval_res);

    eval_res = legalizer_->LookAheadLegalization();
    upper_bound_hpwl_.push_back(eval_res);

    BOOST_LOG_TRIVIAL(info)
      << "It " << cur_iter_ << ": \t"
      << std::scientific << std::setprecision(4)
      << lower_bound_hpwl_.back() << " "
      << upper_bound_hpwl_.back() << "\n";

    if (IsPlacementConverge()) { // if HPWL converges
      BOOST_LOG_TRIVIAL(info)
        << "Iterative look-ahead legalization complete\n";
      BOOST_LOG_TRIVIAL(info)
        << "Total number of iteration: "
        << cur_iter_ + 1 << "\n";
      break;
    }
  }
  BOOST_LOG_TRIVIAL(info)
    << "Lower bound: " << lower_bound_hpwl_ << "\n";
  BOOST_LOG_TRIVIAL(info)
    << "Upper bound: " << upper_bound_hpwl_ << "\n";

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m Global Placement complete\033[0m\n";
  BOOST_LOG_TRIVIAL(info)
    << "(cg time: " << tot_cg_time << "s, lal time: " << tot_lal_time << "s)\n";
  BOOST_LOG_TRIVIAL(info)
    << "total triplets time: "
    << tot_triplets_time_x << "s, "
    << tot_triplets_time_y << "s, "
    << tot_triplets_time_x + tot_triplets_time_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total matrix from triplets time: "
    << tot_matrix_from_triplets_x << "s, "
    << tot_matrix_from_triplets_y << "s, "
    << tot_matrix_from_triplets_x + tot_matrix_from_triplets_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total cg solver time: "
    << tot_cg_solver_time_x << "s, "
    << tot_cg_solver_time_y << "s, "
    << tot_cg_solver_time_x + tot_cg_solver_time_y << "s\n";
  BOOST_LOG_TRIVIAL(info)
    << "total loc update time: "
    << tot_loc_update_time_x << "s, "
    << tot_loc_update_time_y << "s, "
    << tot_loc_update_time_x + tot_loc_update_time_y << "s\n";
  double tot_time_x = tot_triplets_time_x + tot_matrix_from_triplets_x
      + tot_cg_solver_time_x + tot_loc_update_time_x;
  double tot_time_y = tot_triplets_time_y + tot_matrix_from_triplets_y
      + tot_cg_solver_time_y + tot_loc_update_time_y;
  BOOST_LOG_TRIVIAL(info)
    << "total x/y time: "
    << tot_time_x << "s, "
    << tot_time_y << "s, "
    << tot_time_x + tot_time_y << "s\n";
  legalizer_->Close();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";
  ReportMemory();

  return true;
}

void GlobalPlacer::SetDump(bool s_dump) {
  is_dump_ = s_dump;
}

void GlobalPlacer::DumpResult(std::string const &name_of_file) {
  //UpdateGridBinState();
  static int counter = 0;
  //BOOST_LOG_TRIVIAL(info)   << "DumpNum:" << counter << "\n";
  ckt_ptr_->GenMATLABTable(name_of_file);
  //write_not_all_terminal_grid_bins("grid_bin_not_all_terminal" + std::to_string(counter) + ".txt");
  //write_overfill_grid_bins("grid_bin_overfill" + std::to_string(counter) + ".txt");
  ++counter;
}

bool GlobalPlacer::IsSeriesConverge(
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

bool GlobalPlacer::IsSeriesOscillate(std::vector<double> &data, int length) {
  /****
 * Returns if the given series of data is oscillating or not.
 * We will only look at the last several data points @param length.
 * ****/

  // if the given length is too short, we cannot know whether it is oscillating or not.
  if (length < 3) return false;

  // if the given data series is short than the length, we cannot know whether it is oscillating or not.
  int sz = (int) data.size();
  if (sz < length) {
    return false;
  }

  // this vector keeps track of the increasing trend (true) and descreasing trend (false).
  std::vector<bool> trend(length - 1, false);
  for (int i = 0; i < length - 1; ++i) {
    trend[i] = data[sz - 1 - i] > data[sz - 2 - i];
  }
  std::reverse(trend.begin(), trend.end());

  bool is_oscillate = true;
  for (int i = 0; i < length - 2; ++i) {
    if (trend[i] == trend[i + 1]) {
      is_oscillate = false;
      break;
    }
  }

  return is_oscillate;
}
/****
 * @brief This function is a wrapper to report HPWL before and after block
 * location initialization using different methods.
 *
 * @param mode: the method to initialize the block locations
 * @param std_dev: the standard deviation if normal distribution is used
 */
void GlobalPlacer::BlockLocationInitialization_(int mode, double std_dev) {
  BOOST_LOG_TRIVIAL(info)
    << "HPWL before random initialization: " << ckt_ptr_->WeightedHPWL()
    << "\n";

  if (mode == 0) {
    BlockLocationUniformInitialization_();
  } else if (mode == 1) {
    BlockLocationNormalInitialization_(std_dev);
  }

  BOOST_LOG_TRIVIAL(info)
    << "HPWL after random initialization: " << ckt_ptr_->WeightedHPWL() << "\n";

  if (is_dump_) DumpResult("rand_init.txt");
}

/****
 * @brief Initialize the location of blocks uniformly across the placement region.
 */
void GlobalPlacer::BlockLocationUniformInitialization_() {
  // initialize the random number generator
  std::minstd_rand0 generator{1};
  std::uniform_real_distribution<double> distribution(0, 1);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int region_width = RegionWidth();
  int region_height = RegionHeight();
  for (auto &blk : blocks) {
    if (!blk.IsMovable()) continue;
    double init_x = RegionLeft() + region_width * distribution(generator);
    double init_y = RegionBottom() + region_height * distribution(generator);
    blk.SetCenterX(init_x);
    blk.SetCenterY(init_y);
  }
  BOOST_LOG_TRIVIAL(info)
    << "Block location uniform initialization complete\n";
}

/****
 * @brief Initialize the location of blocks using the normal distribution.
 *
 * @param std_dev: the deviation of cells around the center of the placement region
 */
void GlobalPlacer::BlockLocationNormalInitialization_(double std_dev) {
  // initialize the random number generator
  std::minstd_rand0 generator{1};
  std::normal_distribution<double> normal_distribution(0.0, std_dev);

  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  int region_width = RegionWidth();
  int region_height = RegionHeight();
  double region_center_x = (RegionRight() + RegionLeft()) / 2.0;
  double region_center_y = (RegionTop() + RegionBottom()) / 2.0;
  for (auto &block : blocks) {
    if (!block.IsMovable()) continue;
    double x = region_center_x + region_width * normal_distribution(generator);
    double y = region_center_y + region_height * normal_distribution(generator);
    x = std::max(x, (double) RegionLeft());
    x = std::min(x, (double) RegionRight());
    y = std::max(y, (double) RegionBottom());
    y = std::min(y, (double) RegionTop());
    block.SetCenterX(x);
    block.SetCenterY(y);
  }
  BOOST_LOG_TRIVIAL(info)
    << "Block location gaussian initialization complete\n";
}

}
