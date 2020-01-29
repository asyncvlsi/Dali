//
// Created by Yihang Yang on 1/29/20.
//

#ifndef DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_PLOSLIDENETAUX_H_
#define DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_PLOSLIDENETAUX_H_

#include <unordered_map>
#include <unordered_map>

#include "circuit/block.h"
#include "circuit/net.h"

class PLOSlideNetAux : public NetAux {
 private:
  int max_x_;
  int min_x_;

  int max_y_;
  int min_y_;

  std::unordered_map<Block *, int> blk2num_map_;
 public:
  void Init();
  void UpdateMaxMinX();
  void UpdateMaxMinY();

  void SetMaxX(int max_x) { max_x_ = max_x; }
  void SetMinX(int min_x) { min_x_ = min_x; }
  void SetMaxY(int max_y) { max_y_ = max_y; }
  void SetMinY(int min_y) { min_y_ = min_y; }

  int MaxX() const { return max_x_; }
  int MinX() const { return min_x_; }
  int MaxY() const { return max_y_; }
  int MinY() const { return min_y_; }

};

#endif //DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_PLOSLIDENETAUX_H_
