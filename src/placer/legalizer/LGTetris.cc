//
// Created by Yihang Yang on 8/4/2019.
//

#include "LGTetris.h"
#include "../../common/misc.h"

struct DIYLess{
  bool operator()(const indexLocPair a, const indexLocPair b) {
    return (a.x < b.x) || ((a.x == b.x) && (a.y < b.y));
  }
} customLess;

TetrisLegalizer::TetrisLegalizer(): Placer(), max_iteration_(5), current_iteration_(0), flipped_(false) {}

void TetrisLegalizer::Init() {
  std::vector<Block> &block_list = *BlockList();
  indexLocPair init_pair(0,0,0);
  blockXOrder.assign(block_list.size(), init_pair);
}

void TetrisLegalizer::SetMaxItr(int max_iteration) {
  Assert(max_iteration > 0, "Invalid max_iteration value, value must be greater than 0");
  max_iteration_ = max_iteration;
}

void TetrisLegalizer::ResetItr() {
  current_iteration_ = 0;
}

void TetrisLegalizer::FastShift() {

}

void TetrisLegalizer::FlipPlacement() {
  flipped_ = !flipped_;


}

bool TetrisLegalizer::TetrisLegal() {
  //draw_block_net_list("before_tetris_legalization.m");
  std::cout << "Start LGTetris legalization" << std::endl;
  std::vector<Block> &block_list = *BlockList();
  // 1. move all blocks into placement region
  /*for (auto &&block: block_list) {
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

  // 2. sort blocks based on their lower Left corners. Further optimization is doable here.

  for (size_t i=0; i<blockXOrder.size(); ++i) {
    blockXOrder[i].num = i;
    blockXOrder[i].x = block_list[i].LLX();
    blockXOrder[i].y = block_list[i].LLY();
  }
  std::sort(blockXOrder.begin(), blockXOrder.end(), customLess);

  // 3. initialize the data structure to store row usage
  //int maxHeight = GetCircuit()->MaxHeight();
  int minWidth = GetCircuit()->MinWidth();
  int minHeight = GetCircuit()->MinHeight();

  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Building LGTetris space" << std::endl;
  }
  TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), (int)(std::ceil(minHeight/2.0)), minWidth);
  int llx, lly;
  int width, height;
  //int count = 0;
  for (size_t i=0; i<blockXOrder.size(); ++i) {
    int &block_num = blockXOrder[i].num;
    width = block_list[block_num].Width();
    height = block_list[block_num].Height();
    /****
     * After "integerization" of the current location from "double" to "int":
     * 1. if the current location is legal, the location of this block don't have to be changed,
     *  IsSpaceAvail() will mark the space occupied by this block to be "used", and for sure this space is no more available
     * 2. if the current location is illegal,
     *  FindBlockLoc() will find a legal location for this block, and mark that space used.
     * 3. If FindBlocLoc() fails to find a legal location,
     *  FastShift() the remaining blocks to the right hand side of the last placed block, in order to keep block orders
     *  FlipPlacement() will flip the placement in the x-direction
     *  if current_iteration does not reach the maximum allowed number, then do the legalization in a reverse order
     * ****/
    llx = (int)std::round(block_list[block_num].LLX());
    lly = (int)std::round(block_list[block_num].LLY());
    bool is_current_loc_legal = tetrisSpace.IsSpaceAvail(llx, lly, width, height);
    if (is_current_loc_legal) {
      block_list[block_num].SetLLX(llx);
      block_list[block_num].SetLLY(lly);
    } else {
      Loc2D result_loc(0, 0);
      bool is_found = tetrisSpace.FindBlockLoc(llx, lly, width, height, result_loc);
      if (is_found) {
        block_list[block_num].SetLLX(result_loc.x);
        block_list[block_num].SetLLY(result_loc.y);
      } else {
        // if a legal location is not found, need to reverse the legalization process
        FastShift();
        if (current_iteration_ < max_iteration_) {
          ++current_iteration_;
          FlipPlacement();
          TetrisLegal();
        } else {
          std::cout << "Tetris legalization fail!\n";
          return false;
        }
      }
    }
    /*block_list[block_num].is_placed = true;
    std::string file_name = std::to_string(count);
    std::cout << count << "  " << is_current_loc_legal << "\n";
     GenMATLABScriptPlaced(file_name);*/
    //count++;
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Tetris legalization complete!\n";
  }
  return true;
}


void TetrisLegalizer::StartPlacement() {
  Init();
  TetrisLegal();
  ReportHPWL(LOG_CRITICAL);
}
