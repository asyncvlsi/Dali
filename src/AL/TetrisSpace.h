//
// Created by yihan on 7/12/2019.
//

#ifndef HPCC_TETRISSPACE_H
#define HPCC_TETRISSPACE_H

#include <vector>
#include <cmath>
#include "blockal.h"

/* define doubly linked list */
struct SpaceRange {
  int start;
  int end;
  struct SpaceRange* next = nullptr; // Pointer to next space range
  struct SpaceRange* prev = nullptr; // Pointer to previous space range
  explicit SpaceRange(int initStart, int initEnd): start(initStart), end(initEnd) {};
};

class TetrisSpace {
private:
  int _left;
  int _right;
  int _bottom;
  int _top;
  int _rowHeight;
  int _minWidth;
  std::vector< SpaceRange * > availSpaceMap;

  /****derived data entry****/
  int _rowNum;
public:
  TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth);
  bool placeBlock(block_al_t &block);
};


#endif //HPCC_TETRISSPACE_H
