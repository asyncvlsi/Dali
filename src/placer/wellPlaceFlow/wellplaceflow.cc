//
// Created by Yihang Yang on 3/5/20.
//

#include "wellplaceflow.h"

#include <placer/placer.h>

WellPlaceFlow::WellPlaceFlow() : GPSimPL() {}

bool WellPlaceFlow::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start global placement\n";
  }
  SanityCheck();
  CGInit();
  LookAheadLgInit();
  //BlockLocCenterInit();
  BlockLocRandomInit();
  if (NetList()->empty()) {
    if (globalVerboseLevel >= LOG_CRITICAL) {
      std::cout << "\033[0;36m"
                << "Global Placement complete\n"
                << "\033[0m";
    }
    return true;
  }

  InitialPlacement();
  //std::cout << cg_total_hpwl_ << "  " << circuit_->HPWL() << "\n";

  well_legalizer_.TakeOver(this);
  well_legalizer_.Init();
  bool old_success = false;

  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    if (globalVerboseLevel >= LOG_DEBUG) {
      std::cout << cur_iter_ << "-th iteration\n";
    }
    LookAheadLegalization();
    if (globalVerboseLevel >= LOG_CRITICAL) {
      printf("It %d: \t%e  %e\n", cur_iter_, cg_total_hpwl_, lal_total_hpwl_);
    }
    if (cur_iter_ > 15) {
      LGTetrisEx legalizer;
      legalizer.TakeOver(this);
      legalizer.StartPlacement();

      /*StdClusterWellLegalizer well_legalizer;
      well_legalizer.TakeOver(this);
      bool is_success = well_legalizer.StartPlacement();*/
      bool is_success = well_legalizer_.WellLegalize();
      well_legalizer_.GenMatlabClusterTable("sc_result");
      well_legalizer_.GenMATLABWellTable("scw", 0);
      if (!is_success && !old_success) {
        filling_rate_ = filling_rate_ * 0.99;
        std::cout << "Adjusted filling rate: " << filling_rate_ << "\n";
        std::cout << "White space usage: " << circuit_->WhiteSpaceUsage() << "\n";
      }
      if (!old_success) {
        old_success = is_success;
      }
    }
    UpdateLALConvergeState();
    if (HPWL_LAL_converge) { // if HPWL sconverges
      if (cur_iter_ >= 30) {
        if (globalVerboseLevel >= LOG_CRITICAL) {
          std::cout << "Iterative look-ahead legalization complete" << std::endl;
          std::cout << "Total number of iteration: " << cur_iter_ + 1 << std::endl;
        }
        break;
      }
    }
    QuadraticPlacementWithAnchor();
  }

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Global Placement complete\n"
              << "\033[0m";
    printf("(cg time: %.4fs, lal time: %.4fs)\n", tot_cg_time, tot_lal_time);
  }
  LookAheadClose();
  //CheckAndShift();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL(LOG_CRITICAL);

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("(wall time: %.4fs, cpu time: %.4fs)\n", wall_time, cpu_time);
  }
  ReportMemory(LOG_CRITICAL);

  return true;
}

void WellPlaceFlow::EmitDEFWellFile(std::string const &name_of_file,
                                    std::string const &input_def_file,
                                    int well_emit_mode) {
  well_legalizer_.EmitDEFWellFile(name_of_file, input_def_file, well_emit_mode);
}