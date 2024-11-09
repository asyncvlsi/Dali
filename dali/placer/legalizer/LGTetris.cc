/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "LGTetris.h"

#include "dali/common/misc.h"

namespace dali {

TetrisLegalizer::TetrisLegalizer()
    : Placer(), max_iteration_(5), current_iteration_(0), flipped_(false) {}

void TetrisLegalizer::InitLegalizer() {
  BlkInitPair init_pair(nullptr, 0, 0);
  index_loc_list_.resize(ckt_ptr_->Blocks().size(), init_pair);
}

void TetrisLegalizer::SetMaxItr(int max_iteration) {
  DaliExpects(max_iteration > 0,
              "Invalid max_iteration value, value must be greater than 0");
  max_iteration_ = max_iteration;
}

void TetrisLegalizer::FastShift(int failure_point) {
  /****
   * This method is to FastShiftLeft() the blocks following the failure_point
   * (included) to reasonable locations in order to keep block orders
   *    1. tetrisSpace.IsSpaceAvail() fails to place a block when the current
   * location of this block is illegal
   *    2. tetrisSpace.FindBlockLoc() fails to place a block when there is no
   * possible legal location on the right hand side of this block, if
   * failure_point is the first block we shift the bounding box of all blocks to
   * the placement region, the bounding box left and bottom boundaries touch the
   * left and bottom boundaries of placement region, note that the bounding box
   * might be larger than the placement region, but should not be much larger
   * else:
   *    we shift the bounding box of the remaining blocks to the right hand side
   * of the block just placed the bottom boundary of the bounding box will not
   * be changed only the left boundary of the bounding box will be shifted to
   * the right hand side of the block just placed
   * ****/
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  double bounding_left;
  if (failure_point == 0) {
    double bounding_bottom;
    bounding_left = blocks[0].LLX();
    bounding_bottom = blocks[0].LLY();
    for (auto &block : blocks) {
      if (block.LLY() < bounding_bottom) {
        bounding_bottom = block.LLY();
      }
    }
    for (auto &block : blocks) {
      block.IncreaseX(left_ - bounding_left);
      block.IncreaseY(bottom_ - bounding_bottom);
    }
  } else {
    double init_diff =
        index_loc_list_[failure_point - 1].x - index_loc_list_[failure_point].x;
    Block *failed_block = index_loc_list_[failure_point].blk_ptr;
    bounding_left = failed_block->LLX();
    Block *last_placed_block = index_loc_list_[failure_point - 1].blk_ptr;
    int left_new = (int)std::round(last_placed_block->LLX());
    // BOOST_LOG_TRIVIAL(info)   << left_new << "  " << bounding_left << "\n";
    for (size_t i = failure_point; i < index_loc_list_.size(); ++i) {
      Block *blk_ptr = index_loc_list_[i].blk_ptr;
      blk_ptr->IncreaseX(left_new + init_diff - bounding_left);
    }
  }
}

/****
 * flip_axis = (left_ + right_)/2;
 * blk_x = block.X();
 * flipped_x = -(blk_x - flip_axis) + flip_axis = 2*flip_axis - blk_x;
 * flipped_llx = flipped_x - block.Width()/2.0
 *             = 2*flip_axis - (block.X() + block.Width()/2.0)
 *             = 2*flip_axis - block.URX()
 *             = (left_ + right_) - block.URX()
 *             = sum_left_right - block.URX()
 * block.SetLLX(flipped_llx);
 *
 * ****/
void TetrisLegalizer::FlipPlacement() {
  flipped_ = !flipped_;
  int sum_left_right = left_ + right_;
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  for (auto &block : blocks) {
    block.SetLLX(sum_left_right - block.URX());
  }
  // GenMATLABScript("flip_result.txt");
}

bool TetrisLegalizer::TetrisLegal() {
  std::vector<Block> &blocks = ckt_ptr_->Blocks();
  // 1. move all blocks into placement region
  /*for (auto &block: blocks) {
    if (block.LLX() < Left()) {
      block.SetLLX(Left());
    }
    if (block.LLY() < Bottom()) {
      block.SetLLY(Bottom());
    }
    if (block.URX() > Right()) {
      block.SetURX(Right());
    }
    if (block.URY() > Top()) {
      block.SetURY(Top());
    }
  }*/

  // 2. sort blocks based on their lower Left corners. Further optimization is
  // doable here.

  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].blk_ptr = &(blocks[i]);
    index_loc_list_[i].x = blocks[i].LLX();
    index_loc_list_[i].y = blocks[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end(),
            [](const BlkInitPair &pair0, const BlkInitPair &pair1) {
              return (pair0.x < pair1.x) ||
                     ((pair0.x == pair1.x) && (pair0.y < pair1.y));
            });

  /*for (auto &pair: index_loc_list_) {
    BOOST_LOG_TRIVIAL(info)   << blocks[pair.num].LLX() << "\n";
  }*/

  // 3. initialize the data structure to store row usage
  // int maxHeight = GetCircuitRef().MaxBlkHeight();
  int minWidth = ckt_ptr_->MinBlkWidth();
  // int minHeight = GetCircuitRef().MinBlkHeight();

  BOOST_LOG_TRIVIAL(info) << "Building LGTetris space" << std::endl;
  TetrisSpace tetrisSpace(RegionLeft(), RegionRight(), RegionBottom(),
                          RegionTop(), 1, minWidth);
  int llx, lly;
  int width, height;
  // int count = 0;
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    auto bk_ptr = index_loc_list_[i].blk_ptr;
    width = bk_ptr->Width();
    height = bk_ptr->Height();
    /****
     * After "integerization" of the current location from "double" to "int":
     * 1. if the current location is legal, the location of this block don't
     * have to be changed, IsSpaceAvail() will mark the space occupied by this
     * block to be "used", and for sure this space is no more available
     * 2. if the current location is illegal,
     *  FindBlockLoc() will find a legal location for this block, and mark that
     * space used.
     * 3. If FindBlocLoc() fails to find a legal location,
     *  FastShiftLeft() the remaining blocks to the right hand side of the last
     * placed block, in order to keep block orders FlipPlacement() will flip the
     * placement in the x-direction if current_iteration does not reach the
     * maximum allowed number, then do the legalization in a reverse order
     * ****/
    llx = (int)std::round(bk_ptr->LLX());
    lly = (int)std::round(bk_ptr->LLY());
    bool is_current_loc_legal =
        tetrisSpace.IsSpaceAvail(llx, lly, width, height);
    if (is_current_loc_legal) {
      bk_ptr->SetLoc(llx, lly);
    } else {
      int2d result_loc(0, 0);
      bool is_found =
          tetrisSpace.FindBlockLoc(llx, lly, width, height, result_loc);
      if (is_found) {
        bk_ptr->SetLoc(result_loc.x, result_loc.y);
      } else {
        FastShift(i);
        BOOST_LOG_TRIVIAL(info) << "Tetris legalization iteration...\n";
        return false;
      }
    }
    /*blocks[block_num].is_placed = true;
    std::string file_name = std::to_string(count);
    BOOST_LOG_TRIVIAL(info)   << count << "  " << is_current_loc_legal << "\n";
     GenMATLABScriptPlaced(file_name);*/
    // count++;
  }
  return true;
}

bool TetrisLegalizer::StartPlacement() {
  PrintStartStatement("LGTetris legalization");
  InitLegalizer();
  /*for (auto &block: GetCircuitRef().block_list) {
    block.IncreaseX((right_-left_)/2.0);
  }
  max_iter_ = 2;
  GenMATLABScript("shift_result.txt");*/
  bool is_successful = false;
  for (current_iteration_ = 0; current_iteration_ < max_iteration_;
       ++current_iteration_) {
    // if a legal location is not found, need to reverse the legalization
    // process
    is_successful = TetrisLegal();
    if (!is_successful) {
      FlipPlacement();
    } else {
      break;
    }
  }
  if (flipped_) {
    FlipPlacement();
  }

  PrintEndStatement("LGTetris legalization", is_successful);

  return is_successful;
}

}  // namespace dali
