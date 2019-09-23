//
// Created by Yihang Yang on 8/4/2019.
//

#include "LGTetris.h"
#include "../../common/misc.h"

TetrisLegalizer::TetrisLegalizer(): Placer(), max_interation_(0) {}

bool TetrisLegalizer::TetrisLegal() {
  //draw_block_net_list("before_tetris_legalization.m");
  std::cout << "Start LGTetris legalization" << std::endl;
  std::vector<Block> &block_list = *BlockList();
  // 1. move all blocks into placement region
  for (auto &&block: block_list) {
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
  }

  // 2. sort blocks based on their lower Left corners. Further optimization is doable here.
  struct indexLocPair{
    int num;
    double x;
    double y;
  };
  std::vector< indexLocPair > blockXOrder(block_list.size());
  for (size_t i=0; i<blockXOrder.size(); i++) {
    blockXOrder[i].num = i;
    blockXOrder[i].x = block_list[i].LLX();
    blockXOrder[i].y = block_list[i].LLY();
  }
  struct DIYLess{
    bool operator()(const indexLocPair a, const indexLocPair b) {
      return (a.x < b.x) || ((a.x == b.x) && (a.y < b.y));
    }
  } customLess;
  std::sort(blockXOrder.begin(), blockXOrder.end(), customLess);

  // 3. initialize the data structure to store row usage
  //int maxHeight = GetCircuit()->MaxHeight();
  int minWidth = GetCircuit()->MinWidth();
  int minHeight = GetCircuit()->MinHeight();

  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Building LGTetris space" << std::endl;
  }
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), maxHeight, MinWidth);
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), minHeight, MinWidth);
  TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), (int)(std::ceil(minHeight/2.0)), minWidth);
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), 1, MinWidth);
  int llx, lly;
  int width, height;
  for (auto &&blockNum: blockXOrder) {
    llx = (int)std::round(block_list[blockNum.num].LLX());
    lly = (int)std::round(block_list[blockNum.num].LLY());
    width = block_list[blockNum.num].Width();
    height = block_list[blockNum.num].Height();
    /****
     * 1. if the current location is legal, the location of this block don't have to be changed,
     *  IsSpaceAvail() will mark the space occupied by this block to be "used", and for sure this space is no more available
     * 2. if the current location is illegal,
     *  FindBlockLoc() will find a legal location for this block, and mark that space used.
     * ****/
    bool is_current_loc_legal = tetrisSpace.IsSpaceAvail(llx, lly, width, height);
    if (!is_current_loc_legal) {
      Loc2D result_loc(0, 0);
      bool is_found = tetrisSpace.FindBlockLoc(llx, lly, width, height, result_loc);
      if (is_found) {
        block_list[blockNum.num].SetLLX(result_loc.x);
        block_list[blockNum.num].SetLLY(result_loc.y);
      } else {
        // if a legal location is not found, need to reverse the legalization process
        throw NotImplementedException();
      }
    }
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Tetris legalization complete!\n";
  }
  return true;
}


void TetrisLegalizer::StartPlacement() {
  TetrisLegal();
  ReportHPWL(LOG_CRITICAL);
}
