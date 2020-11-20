//
// Created by Yihang Yang on 3/5/20.
//

#ifndef DALI_SRC_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
#define DALI_SRC_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_

#include "placer/globalPlacer/GPSimPL.h"
#include "placer/legalizer/LGTetrisEx.h"
#include "placer/wellLegalizer/stdclusterwelllegalizer.h"

class WellPlaceFlow : public GPSimPL {
  StdClusterWellLegalizer well_legalizer_;
 public:
  WellPlaceFlow();

  void SetGridCapacity(int gc) {
    number_of_cell_in_bin = gc;
    BOOST_LOG_TRIVIAL(info) << "Bin area: " << number_of_cell_in_bin << " average cell area\n";
  }
  void SetIteration(int it_num) {
    cg_iteration_ = it_num;
    BOOST_LOG_TRIVIAL(info) << "Max number of iteration: " << cg_iteration_ << " \n";
  }
  bool StartPlacement() override;

  void EmitDEFWellFile(std::string const &name_of_file, int well_emit_mode) override;
};

#endif //DALI_SRC_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
