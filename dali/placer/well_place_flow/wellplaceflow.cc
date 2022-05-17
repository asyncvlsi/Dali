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
#include "wellplaceflow.h"

#include "dali/common/helper.h"
#include "dali/placer/placer.h"

namespace dali {

WellPlaceFlow::WellPlaceFlow() : GlobalPlacer() {}

bool WellPlaceFlow::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();
  BOOST_LOG_TRIVIAL(info) << "---------------------------------------\n"
                          << "Start global placement\n";
  SanityCheck();
  InitializeConjugateGradientLinearSolver();
  LALInit();
  //BlockLocationNormalInitialization();
  BlockLocationUniformInitialization();
  if (p_ckt_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;36m" << "Global Placement complete\n" << "\033[0m";
    return true;
  }

  double eval_res = QuadraticPlacement(net_model_update_stop_criterion_);
  lower_bound_hpwl_.push_back(eval_res);
  //BOOST_LOG_TRIVIAL(info)   << cg_total_hpwl_ << "  " << circuit_ptr_->HPWL() << "\n";

  //bool old_success = false;
  max_iter_ = 50;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    BOOST_LOG_TRIVIAL(trace) << cur_iter_ << "-th iteration\n";
    eval_res = LookAheadLegalization();
    upper_bound_hpwl_.push_back(eval_res);
    if (cur_iter_ > 10) {
      LGTetrisEx legalizer;
      legalizer.TakeOver(this);
      legalizer.StartPlacement();

      StdClusterWellLegalizer well_legalizer;
      well_legalizer.TakeOver(this);
      well_legalizer.SetStripePartitionMode(int(DefaultPartitionMode::SCAVENGE));
      well_legalizer.WellLegalize();
      upper_bound_hpwl_.back() = p_ckt_->WeightedHPWL();

      //StdClusterWellLegalizer well_legalizer;
      //well_legalizer.TakeOver(this);
      //bool is_success = well_legalizer.StartPlacement();
      //well_legalizer_.GenMatlabClusterTable("sc_result");
      //well_legalizer_.GenMATLABWellTable("scw", 0);
      //if (!is_success && !old_success) {
      //  filling_rate_ = filling_rate_ * 0.99;
      //  BOOST_LOG_TRIVIAL(info)   << "Adjusted filling rate: " << filling_rate_ << "\n";
      //  BOOST_LOG_TRIVIAL(info)   << "White space usage: " << circuit_ptr_->WhiteSpaceUsage() << "\n";
      //}
      //if (!old_success) {
      //  old_success = is_success;
      //}
    }
    BOOST_LOG_TRIVIAL(info)
      << "It " << cur_iter_ << ": \t"
      << lower_bound_hpwl_.back() << " "
      << upper_bound_hpwl_.back() << "\n";
    eval_res =
        QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
    lower_bound_hpwl_.push_back(eval_res);
  }

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m" << "Global Placement complete\n" << "\033[0m";
  BOOST_LOG_TRIVIAL(info)
    << "(cg time: " << tot_cg_time << "s, lal time: " << tot_lal_time << "s)\n";
  LALClose();
  //CheckAndShift();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL();

  well_legalizer_.TakeOver(this);
  well_legalizer_.SetStripePartitionMode(int(DefaultPartitionMode::SCAVENGE));
  well_legalizer_.StartPlacement();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";
  ReportMemory();

  return true;
}

void WellPlaceFlow::EmitDEFWellFile(
    std::string const &name_of_file,
    int well_emit_mode,
    bool enable_emitting_cluster
) {
  well_legalizer_.EmitDEFWellFile(
      name_of_file,
      well_emit_mode,
      enable_emitting_cluster
  );
}

}
