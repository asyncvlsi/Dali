//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_DALI_CIRCUIT_BLOCKPINPAIR_H_
#define DALI_DALI_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"
#include "dali/common/misc.h"

namespace dali {

/****
 * BlockPinPair is a simple struct containing:
 *  a pointer to a Block, representing a gate or an IOPIN
 *  a pointer to a Pin, representing a gate pin
 *
 * Important:
 *  For the consideration of performance, pointers are stored in this structure.
 *  note that blocks in a Circuit and pins in a BlockType are stored in vectors,
 *  the space for these vectors MUST BE pre-allocated using std::vector::resize()
 *  or std::vector::reserve(), otherwise, these pointers may point to invalid
 *  memory locations if not properly handled.
 *  This also implies that one cannot add blocks to a Circuit or pins to a BlockType
 *  once these BlkPinPairs are constructed.
 * ****/

class BlkPinPair {
  public:
    BlkPinPair(
        Block *block_ptr,
        Pin *pin_ptr
    ) : blk_ptr_(block_ptr),
        pin_ptr_(pin_ptr) {}

    // get the pointer to the block
    Block *BlkPtr() const { return blk_ptr_; }

    // get the index of this block
    int BlkId() const { return blk_ptr_->Id(); }

    // get the pointer to the pin
    Pin *PinPtr() const { return pin_ptr_; }

    // get the index of this pin
    int PinId() const { return pin_ptr_->Num(); }

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
    const std::string &BlockName() const { return blk_ptr_->Name(); }

    // get the pin name
    const std::string &PinName() const { return pin_ptr_->Name(); }

    // some boolean operators
    bool operator<(const BlkPinPair &rhs) const {
        return (BlkId() < rhs.BlkId())
            || ((BlkId() == rhs.BlkId()) && (PinId() < rhs.PinId()));
    }
    bool operator>(const BlkPinPair &rhs) const {
        return (BlkId() > rhs.BlkId())
            || ((BlkId() == rhs.BlkId()) && (PinId() > rhs.PinId()));
    }
    bool operator==(const BlkPinPair &rhs) const {
        return (BlkId() == rhs.BlkId()) && (PinId() == rhs.PinId());
    }
  private:
    Block *blk_ptr_;
    Pin *pin_ptr_;
};

}

#endif //DALI_DALI_CIRCUIT_BLOCKPINPAIR_H_
