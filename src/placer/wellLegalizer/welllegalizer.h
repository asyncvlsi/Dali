//
// Created by Yihang Yang on 12/22/19.
//

#ifndef HPCC_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
#define HPCC_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_

#include "../placer.h"

class WellLegalizer: public Placer {


 public:
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_WELLLEGALIZER_WELLLEGALIZER_H_
