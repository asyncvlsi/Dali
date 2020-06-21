//
// Created by Yihang Yang on 2019-12-23.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_
#define DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_

#include <list>

#include "blocktype.h"
#include "common/misc.h"

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

struct BlockTypeWell {
  BlockType *type_ptr_; // pointer to BlockType
  bool is_n_set_ = false; // whether N-well shape is set or not
  bool is_p_set_ = false; // whether P-well shape is set or not
  RectI n_rect_; // N-well rect
  RectI p_rect_; // P-well rect
  int p_n_edge_ = 0; // cached N/P-well boundary

  explicit BlockTypeWell(BlockType *type_ptr) : type_ptr_(type_ptr){}

  // get the pointer to the BlockType this well belongs to
  BlockType *BlkTypePtr() const { return type_ptr_; }

  // set the rect of N-well
  void setNWellRect(int lx, int ly, int ux, int uy) {
    is_n_set_ = true;
    n_rect_.SetValue(lx, ly, ux, uy);
    if (is_p_set_) {
      Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
      p_n_edge_ = n_rect_.LLY();
    }
  }

  // get the pointer to the rect of N-well
  RectI *NWellRectPtr() { return &(n_rect_); }

  // set the rect of P-well
  void setPWellRect(int lx, int ly, int ux, int uy) {
    is_p_set_ = true;
    p_rect_.SetValue(lx, ly, ux, uy);
    if (is_n_set_) {
      Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
      p_n_edge_ = p_rect_.URY();
    }
  }

  // get the pointer to the rect of P-well
  RectI *PWellRectPtr() { return &(p_rect_); }

  // get the P/N well boundary
  int PNBoundary() const { return p_n_edge_; }

  // get the height of N-well
  int NHeight() const { return type_ptr_->Height() - p_n_edge_; }

  // get the height of P-well
  int PHeight() const { return p_n_edge_; }

  // set the rect of N or P well
  void setWellRect(bool is_n, int lx, int ly, int ux, int uy) {
    if (is_n) {
      setNWellRect(lx, ly, ux, uy);
    } else {
      setPWellRect(lx, ly, ux, uy);
    }
  }

  // set the rect of N or P well
  void SetWellShape(bool is_n, RectI &rect) {
    setWellRect(is_n, rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
  }

  // check if N-well is abutted with P-well, if both exist
  bool IsNPWellAbutted() const {
    if (is_p_set_ && is_n_set_) {
      return p_rect_.URY() == n_rect_.LLY();
    }
    return true;
  }

  // report the information of N/P-well for debugging purposes
  void Report() const {
    std::cout
        << "  Well of BlockType: " << *(type_ptr_->NamePtr()) << "\n"
        << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  " << n_rect_.URX() << "  " << n_rect_.URY() << "\n"
        << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  " << p_rect_.URX() << "  " << p_rect_.URY() << "\n";
  }
};

#endif //DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_
