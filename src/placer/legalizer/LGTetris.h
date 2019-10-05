//
// Created by Yihang Yang on 8/4/2019.
//

/****
 * The default setting of this legalizer expects a reasonably good placement as an input;
 * "Reasonably good" means:
 *  1. All blocks are inside placement region
 *  2. There are few or no location with extremely high block density
 *  3. Maybe something else should be added here
 * There is a setting of this legalizer, which can handle bad placement input really well,
 * but who wants bad input? The Global placer and detailed placer should have done their job well;
 * ****/

#ifndef HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
#define HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_

#include "../placer.h"
#include "LGTetris/tetrisspace.h"

struct indexLocPair{
  int num;
  double x;
  double y;
  explicit indexLocPair(int num_init=0, double x_init=0, double y_init=0): num(num_init), x(x_init), y(y_init) {}
};

class TetrisLegalizer: public Placer {
 private:
  int max_iteration_;
  int current_iteration_;
  bool flipped_;
  std::vector< indexLocPair > ordered_list_;
 public:
  TetrisLegalizer();
  void InitLegalizer();
  void SetMaxItr(int max_iteration);
  void ResetItr();
  void FastShift(int failure_point);
  void FlipPlacement();
  bool TetrisLegal();
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
