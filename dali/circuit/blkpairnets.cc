//
// Created by Yihang Yang on 10/27/20.
//

#include "blkpairnets.h"

namespace dali {

void BlkPairNets::ClearX() {
    e00x = 0;
    e01x = 0;
    e10x = 0;
    e11x = 0;
    b0x = 0;
    b1x = 0;
}
void BlkPairNets::WriteX() {
    if (blk_num0 != blk_num1) {
        it01x.valueRef() = e01x;
        it10x.valueRef() = e10x;
    }
}

void BlkPairNets::ClearY() {
    e00y = 0;
    e01y = 0;
    e10y = 0;
    e11y = 0;
    b0y = 0;
    b1y = 0;
}

void BlkPairNets::WriteY() {
    if (blk_num0 != blk_num1) {
        it01y.valueRef() = e01y;
        it10y.valueRef() = e10y;
    }
}

}