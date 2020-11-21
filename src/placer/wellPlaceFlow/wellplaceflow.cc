//
// Created by Yihang Yang on 3/5/20.
//

#include "wellplaceflow.h"

#include <placer/placer.h>

namespace dali {

WellPlaceFlow::WellPlaceFlow() : GPSimPL() {}

bool WellPlaceFlow::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();
  BOOST_LOG_TRIVIAL(info) << "---------------------------------------\n"
                          << "Start global placement\n";
  SanityCheck();
  CGInit();
  LALInit();
  //BlockLocCenterInit();
  BlockLocRandomInit();
  if (NetList()->empty()) {
    BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                            << "Global Placement complete\n"
                            << "\033[0m";
    return true;
  }

  double eval_res = QuadraticPlacement(net_model_update_stop_criterion_);
  lower_bound_hpwl_.push_back(eval_res);
  //BOOST_LOG_TRIVIAL(info)   << cg_total_hpwl_ << "  " << circuit_->HPWL() << "\n";

  //well_legalizer_.TakeOver(this);
  //well_legalizer_.Init();
  //bool old_success = false;

  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    BOOST_LOG_TRIVIAL(info) << cur_iter_ << "-th iteration\n";
    eval_res = LookAheadLegalization();
    upper_bound_hpwl_.push_back(eval_res);
    if (cur_iter_ > 50) {
      LGTetrisEx legalizer;
      legalizer.TakeOver(this);
      legalizer.StartPlacement();

      //StdClusterWellLegalizer well_legalizer;
      //well_legalizer.TakeOver(this);
      //well_legalizer.StartPlacement();
      //bool is_success = well_legalizer_.WellLegalize();
      //well_legalizer_.GenMatlabClusterTable("sc_result");
      //well_legalizer_.GenMATLABWellTable("scw", 0);
      /*if (!is_success && !old_success) {
        filling_rate_ = filling_rate_ * 0.99;
        BOOST_LOG_TRIVIAL(info)   << "Adjusted filling rate: " << filling_rate_ << "\n";
        BOOST_LOG_TRIVIAL(info)   << "White space usage: " << circuit_->WhiteSpaceUsage() << "\n";
      }
      if (!old_success) {
        old_success = is_success;
      }*/
    }
    BOOST_LOG_TRIVIAL(info) << "It " << cur_iter_ << ": \t"
                            << lower_bound_hpwl_.back() << " "
                            << upper_bound_hpwl_.back() << "\n";
    if (IsPlacementConverge()) { // if HPWL converges
      BOOST_LOG_TRIVIAL(info) << "Iterative look-ahead legalization complete" << std::endl;
      BOOST_LOG_TRIVIAL(info) << "Total number of iteration: " << cur_iter_ + 1 << std::endl;
      break;
    }
    eval_res = QuadraticPlacementWithAnchor(net_model_update_stop_criterion_);
    lower_bound_hpwl_.push_back(eval_res);
  }

  BOOST_LOG_TRIVIAL(info) << "\033[0;36m"
                          << "Global Placement complete\n"
                          << "\033[0m";
  BOOST_LOG_TRIVIAL(info) << "(cg time: " << tot_cg_time << "s, lal time: " << tot_lal_time << "s)\n";
  LALClose();
  //CheckAndShift();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL();

  well_legalizer_.TakeOver(this);
  well_legalizer_.StartPlacement();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info) << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";
  ReportMemory();

  return true;
}

void WellPlaceFlow::EmitDEFWellFile(std::string const &name_of_file, int well_emit_mode) {
  well_legalizer_.EmitDEFWellFile(name_of_file, well_emit_mode);
}

}
