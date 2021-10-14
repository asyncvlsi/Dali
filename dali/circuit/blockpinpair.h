//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
#define DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"
#include "dali/common/misc.h"

namespace dali {

/****
 * BlockPinPair is a simple struct containing:
 *  a pointer to a Block, representing a gate or an IOPIN
 *  a pointer to a Pin, representing a gate pin
 *
 * Important:
 *  for the consideration of performance, pointers are stored in this structure.
 *  note that the set of blocks in a Circuit and the set of pins in a BlockType are stored in vectors,
 *  the space for these vectors MUST BE pre-allocated using std::vector::resize() or std::vector::reserve(),
 *  otherwise, these pointers may point to invalid memory locations if not properly handled.
 * ****/

struct BlkPinPair {
    Block *blk_ptr_;
    Pin *pin_ptr_;

    BlkPinPair(Block *block_ptr, Pin *pin_ptr)
        : blk_ptr_(block_ptr), pin_ptr_(pin_ptr) {}

    // get the pointer to the block
    Block *BlkPtr() const { return blk_ptr_; }

    // get the index of this block
    int BlkNum() const { return blk_ptr_->Num(); }

    // get the pointer to the pin
    Pin *PinPtr() const { return pin_ptr_; }

    // get the index of this pin
    int PinNum() const { return pin_ptr_->Num(); }

    // get the offset of this pin in x-direction, the orientation of the block is considered
    double OffsetX() const { return pin_ptr_->OffsetX(blk_ptr_->Orient()); }

    // get the offset of this pin in y-direction, the orientation of the block is considered
    double OffsetY() const { return pin_ptr_->OffsetY(blk_ptr_->Orient()); }

    // get the absolute position of this pin in x-direction, the orientation of the block is considered
    double AbsX() const { return OffsetX() + blk_ptr_->LLX(); }

    // get the absolute position of this pin in y-direction, the orientation of the block is considered
    double AbsY() const { return OffsetY() + blk_ptr_->LLY(); }

    // get location of this pin
    double2d Location() const { return double2d(AbsX(), AbsY()); }

    // get the block name
    const std::string *BlockNamePtr() const { return blk_ptr_->NamePtr(); }

    // get the pin name
    const std::string *PinNamePtr() const { return pin_ptr_->Name(); }

    // some boolean operators
    bool operator<(const BlkPinPair &rhs) const {
        return (BlkNum() < rhs.BlkNum())
            || ((BlkNum() == rhs.BlkNum()) && (PinNum() < rhs.PinNum()));
    }
    bool operator>(const BlkPinPair &rhs) const {
        return (BlkNum() > rhs.BlkNum())
            || ((BlkNum() == rhs.BlkNum()) && (PinNum() > rhs.PinNum()));
    }
    bool operator==(const BlkPinPair &rhs) const {
        return (BlkNum() == rhs.BlkNum()) && (PinNum() == rhs.PinNum());
    }
};

}

#endif //DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
