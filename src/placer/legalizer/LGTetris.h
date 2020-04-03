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

#ifndef DALI_SRC_PLACER_LEGALIZER_LGTETRIS_H_
#define DALI_SRC_PLACER_LEGALIZER_LGTETRIS_H_

#include "common/misc.h"
#include "LGTetris/tetrisspace.h"
#include "placer/placer.h"

class TetrisLegalizer : public Placer {
 private:
  int max_iteration_;
  int current_iteration_;
  bool flipped_;
  std::vector<IndexLocPair<double> > index_loc_list_;
 public:
  TetrisLegalizer();
  void InitLegalizer();
  void SetMaxItr(int max_iteration);
  void ResetItr() {current_iteration_ = 0;}
  void FastShift(int failure_point);
  void FlipPlacement();
  bool TetrisLegal();
  bool StartPlacement() override;
};

#endif //DALI_SRC_PLACER_LEGALIZER_LGTETRIS_H_
