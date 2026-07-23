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
#include "well_place_flow.h"

#include "dali/common/helper.h"
#include "dali/placer/placer.h"

namespace dali {

WellPlaceFlow::WellPlaceFlow() : GlobalPlacer() {}

bool WellPlaceFlow::StartPlacement() {  // TODO: do not use this
  if (ckt_ptr_->Components().empty()) {
    LOG(info) << "Empty component list, nothing to place!\n";
  }
  if (ckt_ptr_->Nets().empty()) {
    LOG(info) << "Empty net list, nothing to optimize during placement!\n";
  }

  PrintStartStatement("well place flow");
  SanityCheck();
  InitializePlacementEngines();
  optimizer_->Initialize();
  spreader_->Initialize(PlacementDensity());
  InitializeComponentLocation();

  optimizer_->OptimizeHpwl();
  // << "\n";

  // bool old_success = false;
  max_iter_ = 50;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    LOG(trace) << cur_iter_ << "-th iteration\n";
    spreader_->Spread();
    if (cur_iter_ > 10) {
      ExtendedTetrisLegalizer legalizer;
      legalizer.CopyPlacementContextFrom(this);
      legalizer.StartPlacement();

      GriddedCellWellLegalizer well_legalizer;
      well_legalizer.CopyPlacementContextFrom(this);
      well_legalizer.SetStripePartitionMode(int(WellPartitionMode::kScavenge));
      well_legalizer.WellLegalize();
      spreader_->Hpwls().back() = ckt_ptr_->WeightedHPWL();

      // GriddedCellWellLegalizer well_legalizer;
      // bool is_success = well_legalizer.StartPlacement();
      //   filling_rate_ << "\n"; LOG(info)   << "White space
      //   usage: " << circuit_ptr_->WhiteSpaceUsage() << "\n";
    }
    LOG(info) << "It " << cur_iter_ << ": \t" << optimizer_->GetHpwls().back()
              << " " << spreader_->Hpwls().back() << "\n";
    optimizer_->OptimizeHpwl();
  }

  LOG(info) << "\033[0;36m" << "Global Placement complete\n"
            << "\033[0m";
  LOG(info) << "(cg time: " << optimizer_->GetTime()
            << "s, lal time: " << spreader_->GetTime() << "s)\n";
  spreader_->Close();
  UpdateMovableComponentPlacementStatus();
  ReportHPWL();

  well_legalizer_.CopyPlacementContextFrom(this);
  well_legalizer_.SetStripePartitionMode(int(WellPartitionMode::kScavenge));
  well_legalizer_.StartPlacement();

  PrintEndStatement("well place flow", true);

  return true;
}

void WellPlaceFlow::EmitDEFWellFile(std::string const& name_of_file,
                                    int well_emit_mode,
                                    bool enable_emitting_cluster) {
  well_legalizer_.EmitDEFWellFile(name_of_file, well_emit_mode,
                                  enable_emitting_cluster);
}

}  // namespace dali
