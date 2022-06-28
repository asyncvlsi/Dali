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

#include <omp.h>

#include <algorithm>

#include "dali/common/logging.h"

namespace dali {

/****
 * @brief Set the maximum number of iterations.
 *
 * @param max_iter: maximum number of iterations.
 */
void GlobalPlacer::SetMaxIteration(int max_iter) {
  DaliExpects(max_iter >= 0, "negative number of iterations?");
  max_iter_ = max_iter;
}

/****
 * @brief Load a configuration file for this placer.
 *
 * @param config_file: name of the configuration file.
 */
void GlobalPlacer::LoadConf(std::string const &config_file) {
  config_read(config_file.c_str());
  DaliFatal("This function is not fully implemented");
}

/****
 * @brief Initialize HPWL optimizer and rough legalizer. If optimizer/legalizer
 * has been initialized, delete them and create a new instance.
 */
void GlobalPlacer::InitializeOptimizerAndLegalizer() {
  delete optimizer_;
  optimizer_ = new B2BHpwlOptimizer(ckt_ptr_, num_threads_);
  optimizer_->Initialize();

  delete legalizer_;
  legalizer_ = new LookAheadLegalizer(ckt_ptr_);
  legalizer_->Initialize(PlacementDensity());
}

/****
 * @brief Close and delete both optimizer and legalizer.
 */
void GlobalPlacer::CloseOptimizerAndLegalizer() {
  if (optimizer_ != nullptr) {
    optimizer_->Close();
    delete optimizer_;
  }
  if (legalizer_ != nullptr) {
    legalizer_->Close();
    delete legalizer_;
  }
}

/****
 * @brief Returns true or false indicating the convergence of the global placement.
 *
 * Stopping criteria (SimPL, option 1):
 *    (a). the gap is reduced to 25% of the gap in the tenth iteration and
 *    upper-bound solution stops improving
 *    (b). the gap is smaller than 10% of the gap in the tenth iteration
 * Stopping criteria (POLAR, option 2):
 *    the gap between lower bound wire-length and upper bound wire-length is
 *    less than 8%
 * ****/
bool GlobalPlacer::IsPlacementConverge() {
  bool res;
  auto &lower_bound_hpwl = optimizer_->GetHpwls();
  auto &upper_bound_hpwl = legalizer_->GetHpwls();
  if (convergence_criteria_ == 1) {
    // (a) and (b) requires at least 10 iterations
    if (lower_bound_hpwl.size() <= 10) {
      res = false;
    } else {
      double tenth_gap = upper_bound_hpwl[9] - lower_bound_hpwl[9];
      double last_gap = upper_bound_hpwl.back() - lower_bound_hpwl.back();
      double gap_ratio = last_gap / tenth_gap;
      if (gap_ratio < 0.1) { // (a)
        res = true;
      } else if (gap_ratio < 0.25) { // (b)
        res = IsSeriesConverge(
            upper_bound_hpwl,
            3,
            simpl_LAL_converge_criterion_
        );
      } else {
        res = false;
      }
    }
  } else if (convergence_criteria_ == 2) {
    if (lower_bound_hpwl.empty()) {
      res = false;
    } else {
      double lower_bound = lower_bound_hpwl.back();
      double upper_bound = upper_bound_hpwl.back();
      res = (lower_bound < upper_bound)
          && (upper_bound / lower_bound - 1 < polar_converge_criterion_);
    }
  } else {
    DaliExpects(false, "Unknown Convergence Criteria!");
  }

  return res;
}

/****
 * @brief Check if block_list is empty or net_list is empty. If either of them
 * is empty, return true, so that the global placement can be skipped.
 * @return a boolean value indicate whether block_list or net_list is empty or not.
 */
bool GlobalPlacer::IsBlockListOrNetListEmpty() const {
  if (ckt_ptr_->Blocks().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Empty block list, nothing to place! Skip global placement!\n";
    return true;
  }
  if (ckt_ptr_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "Empty net list, nothing to optimize! Skip global placement!\n";
    return true;
  }
  return false;
}

/****
 * @brief A helper function to format and print HPWL in each iteration.
 *
 * @param iter: the current iteration of global placement.
 * @param lo: the lower bound of HPWL.
 * @param hi: the upper bound of HPWL.
 */
void GlobalPlacer::PrintHpwl(int iter, double lo, double hi) const {
  std::string buffer(1024, '\0');
  int written_length = sprintf(
      &buffer[0],
      "  Iter %-3d: %.4e  %.4e\n", iter, lo, hi
  );
  buffer.resize(written_length);
  BOOST_LOG_TRIVIAL(info) << buffer;
  BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration completed\n";
}

/****
 * @brief Printf the summary of global placement.
 */
void GlobalPlacer::PrintPlacementSummary() const {
  BOOST_LOG_TRIVIAL(debug)
    << "  Iterative look-ahead legalization complete\n";
  BOOST_LOG_TRIVIAL(debug)
    << "  Total number of iteration: " << cur_iter_ + 1 << "\n";
  BOOST_LOG_TRIVIAL(debug)
    << "  Lower bound: " << optimizer_->GetHpwls() << "\n";
  BOOST_LOG_TRIVIAL(debug)
    << "  Upper bound: " << legalizer_->GetHpwls() << "\n";
  BOOST_LOG_TRIVIAL(debug)
    << "cg time: " << optimizer_->GetTime()
    << "s, lal time: " << legalizer_->GetTime() << "s\n";
}

/****
 * @brief The entry point of global placement.
 * @return A boolean value indicating whether global placement can be
 * successfully performed.
 */
bool GlobalPlacer::StartPlacement() {
  if (IsBlockListOrNetListEmpty()) return true;

  PrintStartStatement("global placement");
  SanityCheck();
  InitializeOptimizerAndLegalizer();
  InitializeBlockLocationAtRandom(0, -1);

  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    optimizer_->SetIteration(cur_iter_);
    double lo_hpwl = optimizer_->OptimizeHpwl(net_model_update_stop_criterion_);
    double hi_hpwl = legalizer_->LookAheadLegalization();

    PrintHpwl(cur_iter_, lo_hpwl, hi_hpwl);
    if (IsPlacementConverge()) {
      break;
    }
  }

  PrintPlacementSummary();
  CloseOptimizerAndLegalizer();
  PrintEndStatement("global placement", true);
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
void GlobalPlacer::InitializeBlockLocationAtRandom(int mode, double std_dev) {
  BOOST_LOG_TRIVIAL(info)
    << "  HPWL before random initialization: " << ckt_ptr_->WeightedHPWL()
    << "\n";

  if (mode == 0) {
    BlockLocationUniformInitialization();
  } else if (mode == 1) {
    BlockLocationNormalInitialization(std_dev);
  }

  BOOST_LOG_TRIVIAL(info)
    << "  HPWL after random initialization: " << ckt_ptr_->WeightedHPWL()
    << "\n";

  if (is_dump_) DumpResult("rand_init.txt");
}

/****
 * @brief Initialize the location of blocks uniformly across the placement region.
 */
void GlobalPlacer::BlockLocationUniformInitialization() {
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
  BOOST_LOG_TRIVIAL(debug)
    << "  Block location uniform initialization complete\n";
}

/****
 * @brief Initialize the location of blocks using the normal distribution.
 *
 * @param std_dev: the deviation of cells around the center of the placement region
 */
void GlobalPlacer::BlockLocationNormalInitialization(double std_dev) {
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
  BOOST_LOG_TRIVIAL(debug)
    << "  Block location gaussian initialization complete\n";
}

}
