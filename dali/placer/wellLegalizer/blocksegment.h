//
// Created by Yihang Yang on 4/12/21.
//

#ifndef DALI_DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
#define DALI_DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_

#include "dali/circuit/block.h"

namespace dali {

struct BlockSegment {
  private:
    int lx_;
    int width_;
  public:
    BlockSegment(Block *blk_ptr, int loc) : lx_(loc), width_(blk_ptr->Width()) {
      blk_list.push_back(blk_ptr);
      initial_loc.push_back(loc);
    }
    std::vector<Block *> blk_list;
    std::vector<double> initial_loc;

    int LX() const { return lx_; }
    int UX() const { return lx_ + width_; }
    int Width() const { return width_; }

    bool IsOnLeft(BlockSegment &sc) const {
      return sc.LX() < UX();
    }
    void Merge(BlockSegment &sc, int lower_bound, int upper_bound);
    void UpdateBlockLocation();

    void Report() const;
};

}

#endif //DALI_DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
