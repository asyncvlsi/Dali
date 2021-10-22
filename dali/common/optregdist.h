//
// Created by Yihang Yang on 9/9/20.
//

#ifndef DALI_DALI_COMMON_OPTREGDIST_H_
#define DALI_DALI_COMMON_OPTREGDIST_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "logging.h"

namespace dali {

// optimal region distance
class OptRegDist {
  public:
    Circuit *circuit_ = nullptr;
    void FindOptimalRegionX(
        Block &blk,
        double &lx,
        double &ly,
        double &ux,
        double &uy
    ) const;

    void SaveFile(std::string const &file_name) const;
};

}

#endif //DALI_DALI_COMMON_OPTREGDIST_H_
