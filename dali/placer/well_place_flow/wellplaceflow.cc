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
  InitializeOptimizerAndLegalizer();
  optimizer_->Initialize();
  legalizer_->Initialize(PlacementDensity());
  InitializeBlockLocationAtRandom(0, -1);
  if (ckt_ptr_->Nets().empty()) {
    BOOST_LOG_TRIVIAL(info)
      << "\033[0;36m" << "Global Placement complete\n" << "\033[0m";
    return true;
  }

  optimizer_->QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
  //BOOST_LOG_TRIVIAL(info)   << cg_total_hpwl_ << "  " << circuit_ptr_->HPWL() << "\n";

  //bool old_success = false;
  max_iter_ = 50;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    BOOST_LOG_TRIVIAL(trace) << cur_iter_ << "-th iteration\n";
    legalizer_->LookAheadLegalization();
    if (cur_iter_ > 10) {
      LGTetrisEx legalizer;
      legalizer.TakeOver(this);
      legalizer.StartPlacement();

      StdClusterWellLegalizer well_legalizer;
      well_legalizer.TakeOver(this);
      well_legalizer.SetStripePartitionMode(int(DefaultPartitionMode::SCAVENGE));
      well_legalizer.WellLegalize();
      legalizer_->GetHpwls().back() = ckt_ptr_->WeightedHPWL();

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
      << optimizer_->GetHpwls().back() << " "
      << legalizer_->GetHpwls().back() << "\n";
    optimizer_->QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
  }

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m" << "Global Placement complete\n" << "\033[0m";
  BOOST_LOG_TRIVIAL(info)
    << "(cg time: " << optimizer_->GetTime() << "s, lal time: " << legalizer_->GetTime() << "s)\n";
  legalizer_->Close();
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
