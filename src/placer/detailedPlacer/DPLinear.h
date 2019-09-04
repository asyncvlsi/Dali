//
// Created by Yihang Yang on 9/3/19.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_H_

#include <vector>
#include "../../placer/placer.h"
#include "DPLinear/scaffoldnet.h"

class DPLinear: public Placer {
 private:

 public:
  DPLinear();
  std::vector< ScaffoldNet > scaffold_net_list;
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_H_
