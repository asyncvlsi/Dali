//
// Created by Yihang Yang on 3/5/20.
//

#ifndef DALI_SRC_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
#define DALI_SRC_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_

#include "placer/globalPlacer/GPSimPL.h"
#include "placer/wellLegalizer/clusterwelllegalizer.h"

class WellPlaceFlow : public GPSimPL {
 private:
  ClusterWellLegalizer well_legalizer_;
 public:
  WellPlaceFlow();

  void StartPlacement() override;
};

#endif //DALI_SRC_PLACER_WELLPLACEFLOW_WELLPLACEFLOW_H_
