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
  double max_x_;
  double min_x_;

  double max_y_;
  double min_y_;

  std::unordered_map<Block *, Pin *> blk2pin_map_;

 public:
  explicit PLOSlideNetAux(Net *net);
  void Init();
  void UpdateMaxMinLocX();
  void UpdateMaxMinLocY();

  void SetMaxX(double max_x) { max_x_ = max_x; }
  void SetMinX(double min_x) { min_x_ = min_x; }
  void SetMaxY(double max_y) { max_y_ = max_y; }
  void SetMinY(double min_y) { min_y_ = min_y; }

  double MaxX() const { return max_x_; }
  double MinX() const { return min_x_; }
  double MaxY() const { return max_y_; }
  double MinY() const { return min_y_; }

  Pin *GetPin(Block *block);
  Pin *GetPin(Block &block) { return GetPin(&block); }

};

#endif //DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_PLOSLIDENETAUX_H_
