//
// Created by yihan on 8/4/2019.
//

#include "LGTetris.h"

bool TetrisLegalizer::TetrisLegal() {
  //draw_block_net_list("before_tetris_legalization.m");
  std::cout << "Start tetris legalization" << std::endl;
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
  int maxHeight = GetCircuit()->MaxHeight();
  int minWidth = GetCircuit()->MinWidth();
  int minHeight = GetCircuit()->MinHeight();

  std::cout << "Building tetris space" << std::endl;
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), maxHeight, minWidth);
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), minHeight, minWidth);
  TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), (int)(std::ceil(minHeight/2.0)), minWidth);
  //TetrisSpace tetrisSpace(Left(), Right(), Bottom(), Top(), 1, minWidth);
  //tetrisSpace.show();
  std::cout << "Placing blocks:\n";
  int numPlaced = 0;
  int barWidth = 70;
  int approxNum = (int)(blockXOrder.size())/100;
  int quota = std::max(approxNum,1);
  double progress = 0;
  for (auto &&blockNum: blockXOrder) {
    //std::cout << "Placing block: " << blockNum.num << std::endl;
    Loc2D result = tetrisSpace.findBlockLocation(block_list[blockNum.num].LLX(), block_list[blockNum.num].LLY(), block_list[blockNum.num].Width(), block_list[blockNum.num].Height());
    //std::cout << "Loc to set: " << result.x << " " << result.y << std::endl;
    block_list[blockNum.num].SetLLX(result.x);
    block_list[blockNum.num].SetLLY(result.y);
    //draw_block_net_list("during_tetris.m");
    ++numPlaced;
    if (numPlaced%quota == 0) {
      progress = (double)(numPlaced)/blockXOrder.size();
      std::cout << "[";
      int pos = (int) (barWidth * progress);
      for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
      }
      std::cout << "] " << int(progress * 100.0) << " %\r";
      //std::cout << std::endl;
      std::cout.flush();
    }
  }
  std::cout << "[";
  for (int i = 0; i < barWidth; ++i) {
    if (i < barWidth) std::cout << "=";
    else if (i == barWidth) std::cout << ">";
    else std::cout << " ";
  }
  std::cout << "] " << 100 << " %\n";

  //draw_block_net_list("after_tetris.txt");
  std::cout << "Tetris legalization complete!\n";
  return true;
}


void TetrisLegalizer::StartPlacement() {
  TetrisLegal();
}
