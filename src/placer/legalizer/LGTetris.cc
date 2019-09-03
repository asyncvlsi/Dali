//
// Created by yihan on 8/4/2019.
//

#include "LGTetris.h"

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

  // 2. sort blocks based on their lower left corners. Further optimization is doable here.
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
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), maxHeight, minWidth);
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), minHeight, minWidth);
  TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), (int)(std::ceil(minHeight/2.0)), minWidth);
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), 1, minWidth);
  //tetrisSpace.show();
  for (auto &&blockNum: blockXOrder) {
    //std::cout << "Placing block: " << blockNum.num << std::endl;
    Loc2D result = tetrisSpace.findBlockLocation(block_list[blockNum.num].LLX(), block_list[blockNum.num].LLY(), block_list[blockNum.num].Width(), block_list[blockNum.num].Height());
    //std::cout << "Loc to set: " << result.x << " " << result.y << std::endl;
    block_list[blockNum.num].SetLLX(result.x);
    block_list[blockNum.num].SetLLY(result.y);
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
