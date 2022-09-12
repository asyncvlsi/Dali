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

#include <algorithm>
#include <cfloat>
#include <memory>

#include "dali/common/logging.h"

namespace dali {

/****
 * @brief Set the maximum number of iterations.
 *
 * @param max_iter: maximum number of iterations.
 */
void GlobalPlacer::SetMaxIteration(int32_t max_iter) {
  DaliExpects(max_iter >= 0, "negative number of iterations?");
  max_iter_ = max_iter;
}

/****
 * @brief Set an internal boolean variable to save or not save intermediate results.
 *
 * @param should_save_intermediate_result : if true, intermediate results will
 * be saved; otherwise, not.
 */
void GlobalPlacer::SetShouldSaveIntermediateResult(bool should_save_intermediate_result) {
  should_save_intermediate_result_ = should_save_intermediate_result;
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
  optimizer_->SetShouldSaveIntermediateResult(should_save_intermediate_result_);
  optimizer_->Initialize();

  delete legalizer_;
  legalizer_ = new LookAheadLegalizer(ckt_ptr_);
  legalizer_->SetShouldSaveIntermediateResult(should_save_intermediate_result_);
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
 * @brief This function is a wrapper to report HPWL before and after block
 * location initialization using different methods.
 *
 * @param mode: the method to initialize the block locations
 * @param std_dev: the standard deviation if normal distribution is used
 */
void GlobalPlacer::InitializeBlockLocation() {
  std::unique_ptr<RandomInitializer> initializer(nullptr);
  switch (initializer_type_) {
    case RandomInitializerType::UNIFORM : {
      initializer = std::make_unique<UniformInitializer>(ckt_ptr_, 1);
      break;
    }
    case RandomInitializerType::GAUSSIAN : {
      initializer = std::make_unique<GaussianInitializer>(ckt_ptr_, 1);
      break;
    }
    case RandomInitializerType::MONTE_CARLO : {
      initializer = std::make_unique<MonteCarloInitializer>(ckt_ptr_, 1);
      break;
    }
    case RandomInitializerType::DENSITY_AWARE : {
      initializer = std::make_unique<DensityAwareInitializer>(ckt_ptr_, 1);
      break;
    }
    default : {
      DaliFatal("Unknown random initializer type");
    }
  }
  initializer->SetShouldSaveIntermediateResult(should_save_intermediate_result_);
  initializer->RandomPlace();
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
  InitializeBlockLocation();
  //ckt_ptr_->GenMATLABTable("init_result.txt");
  //std::cout << std::endl;
  //exit(1);
  InitializeOptimizerAndLegalizer();
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    optimizer_->SetIteration(cur_iter_);
    optimizer_->OptimizeHpwl();
    legalizer_->RemoveCellOverlap();
    PrintHpwl();
    if (IsPlacementConverge()) break;
  }
  UpdateMovableBlkPlacementStatus();

  PrintEndStatement("Global placement", true);
  CloseOptimizerAndLegalizer();
  return true;
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

bool GlobalPlacer::IsSeriesConverge(
    std::vector<double> &series,
    int32_t window_size,
    double tolerance
) {
  auto sz = static_cast<int32_t>(series.size());
  if (sz < window_size) {
    return false;
  }
  double max_val = -DBL_MAX;
  double min_val = DBL_MAX;
  for (int32_t i = 0; i < window_size; ++i) {
    max_val = std::max(max_val, series[sz - 1 - i]);
    min_val = std::min(min_val, series[sz - 1 - i]);
  }
  DaliExpects(max_val >= 0 && min_val >= 0,
              "Negative series series not supported!");
  if (max_val < 1e-10 && min_val <= 1e-10) {
    return true;
  }
  double ratio = max_val / min_val - 1;
  return ratio < tolerance;
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
 * @brief A helper function to format and print HPWL in each iteration.
 */
void GlobalPlacer::PrintHpwl() const {
  if (optimizer_->GetHpwls().empty() || legalizer_->GetHpwls().empty()) return;
  double lo_hpwl = optimizer_->GetHpwls().back();
  double hi_hpwl = legalizer_->GetHpwls().back();
  std::string buffer(1024, '\0');
  int32_t written_length = sprintf(
      &buffer[0],
      "  iter-%-3d: %.4e  %.4e\n", cur_iter_, lo_hpwl, hi_hpwl
  );
  buffer.resize(written_length);
  BOOST_LOG_TRIVIAL(info) << buffer;
  BOOST_LOG_TRIVIAL(debug) << cur_iter_ << "-th iteration completed\n";
}

/****
 * @brief Printf the summary of global placement.
 */
void GlobalPlacer::PrintEndStatement(
    std::string const &name_of_process,
    bool is_success
) {
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
  Placer::PrintEndStatement(name_of_process, is_success);
}

}
