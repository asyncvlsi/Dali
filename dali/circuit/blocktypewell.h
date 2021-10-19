//
// Created by Yihang Yang on 12/23/19.
//

#ifndef DALI_DALI_CIRCUIT_BLOCKTYPEWELL_H_
#define DALI_DALI_CIRCUIT_BLOCKTYPEWELL_H_

#include <list>

#include "blocktype.h"
#include "dali/common/logging.h"
#include "dali/common/misc.h"

namespace dali {

/****
 * This struct BlockTypeWell provides the N/P-well geometries for a BlockType.
 * Assumptions:
 *  1. BlockType has at most one rectangular N-well and at most one rectangular P-well.
 *  2. It is allowed to provide only N-well or P-well.
 *  3. If both N-well and P-well are present, they must be abutted. This is for debugging purposes, also for compact physical layout.
 *     +-----------------+
 *     |                 |
 *     |                 |
 *     |   N-well        |
 *     |                 |
 *     |                 |
 *     +-----------------+  p_n_edge_
 *     |                 |
 *     |                 |
 *     |    P-well       |
 *     |                 |
 *     |                 |
 *     +-----------------+
 * ****/

class BlockType;

class BlockTypeWell {
  public:
    explicit BlockTypeWell(BlockType *type_ptr) : type_ptr_(type_ptr) {}

    // get the pointer to the BlockType this well belongs to
    BlockType *BlkTypePtr() const { return type_ptr_; }

    // set the rect of N-well
    void setNWellRect(int lx, int ly, int ux, int uy);

    // get the pointer to the rect of N-well
    RectI *NWellRectPtr() { return &(n_rect_); }

    // set the rect of P-well
    void setPWellRect(int lx, int ly, int ux, int uy);

    // get the pointer to the rect of P-well
    RectI *PWellRectPtr() { return &(p_rect_); }

    // get the P/N well boundary
    int PNBoundary() const { return p_n_edge_; }

    // get the height of N-well
    int NHeight() const { return type_ptr_->Height() - p_n_edge_; }

    // get the height of P-well
    int PHeight() const { return p_n_edge_; }

    // set the rect of N or P well
    void setWellRect(bool is_n, int lx, int ly, int ux, int uy);

    // set the rect of N or P well
    void SetWellShape(bool is_n, RectI &rect);

    // check if N-well is abutted with P-well, if both exist
    bool IsNPWellAbutted() const;

    // report the information of N/P-well for debugging purposes
    void Report() const;

  private:
    BlockType *type_ptr_; // pointer to BlockType
    bool is_n_set_ = false; // whether N-well shape is set or not
    bool is_p_set_ = false; // whether P-well shape is set or not
    RectI n_rect_; // N-well rect
    RectI p_rect_; // P-well rect
    int p_n_edge_ = 0; // cached N/P-well boundary
};

}

#endif //DALI_DALI_CIRCUIT_BLOCKTYPEWELL_H_
