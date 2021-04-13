//
// Created by Yihang Yang on 4/12/21.
//

#ifndef DALI_DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
#define DALI_DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_

#include "dali/circuit/block.h"

namespace dali {

struct BlockSegment {
    BlockSegment(Block *blk_ptr, int loc) : lx(loc), width(blk_ptr->Width()) {
      blk_list.push_back(blk_ptr);
      initial_loc.push_back(loc);
    }
    int lx;
    int width;
    std::vector<Block *> blk_list;
    std::vector<double> initial_loc;

    int LX() const { return lx; }
    int UX() const { return lx + width; }

    bool Overlap(BlockSegment &sc) const {
      return sc.LX() < UX();
    }
    void Merge(BlockSegment &sc, int lower_bound, int upper_bound);
    void PlaceBlock();

    void Report() const;
};

}

#endif //DALI_DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
