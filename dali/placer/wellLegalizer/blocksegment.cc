//
// Created by Yihang Yang on 4/12/21.
//

#include "blocksegment.h"

namespace dali {

void BlockSegment::Merge(BlockSegment &sc, int lower_bound, int upper_bound) {
  int sz = (int) sc.blk_list.size();
  DaliExpects(sz == (int) sc.initial_loc.size(),
              "Block number does not match initial location number, BlockSegment::Merge");
  for (int i = 0; i < sz; ++i) {
    blk_list.push_back(sc.blk_list[i]);
    initial_loc.push_back(sc.initial_loc[i]);
  }
  width += sc.width;

  std::vector<double> anchor;
  int accumulative_width = 0;
  sz = (int) blk_list.size();
  for (int i = 0; i < sz; ++i) {
    anchor.push_back(initial_loc[i] - accumulative_width);
    accumulative_width += blk_list[i]->Width();
  }
  DaliExpects(width == accumulative_width, "Something is wrong, width does not match, BlockSegment::Merge()");

  double sum = 0;
  for (auto &num: anchor) {
    sum += num;
  }
  lx = (int)std::round(sum/sz);
  if (lx < lower_bound) {
    lx = lower_bound;
  }
  if (lx + width > upper_bound) {
    lx = upper_bound - width;
  }
}

void BlockSegment::PlaceBlock() {
  int cur_loc = lx;
  for (auto &blk: blk_list) {
    blk->setLLX(cur_loc);
    cur_loc += blk->Width();
  }
}

void BlockSegment::Report() const {
  int sz = (int) blk_list.size();
  for (int i=0; i<sz; ++i) {
    std::cout << blk_list[i]->Name() << "  "
              << blk_list[i]->LLX() << "  "
              << blk_list[i]->Width() << "  "
              << initial_loc[i] << "\n";
  }
}

}