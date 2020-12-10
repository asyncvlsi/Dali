//
// Created by Yihang Yang on 9/11/2019.
//

#ifndef DALI_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
#define DALI_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_

#include <vector>

#include "dali/common/misc.h"
#include "MDPlacer/bin.h"
#include "MDPlacer/mdblkaux.h"
#include "dali/placer/placer.h"

namespace dali {

class MDPlacer : public Placer {
 private:
  double learning_rate_;
  double momentum_term_;
  int max_iteration_num_;
  int bin_width_;
  int bin_height_;
  int bin_cnt_x_;
  int bin_cnt_y_;
 public:
  MDPlacer();
  std::vector<MDBlkAux> blk_aux_list;
  std::vector<std::vector<Bin>> bin_matrix;
  void CreateBlkAuxList();
  void InitGridBin();
  void UpdateBinMatrix();
  void UpdateVelocityLoc(Block &blk);
  int BinWidth() const;
  int BinHeight() const;
  int BinCountX() const;
  int BinCountY() const;
  BinIndex LowLocToIndex(double llx, double lly);
  BinIndex HighLocToIndex(double urx, double ury);
  void BinRegionRemove(int blk_num, BinIndex &ll, BinIndex &ur);
  void BinRegionAdd(int blk_num, BinIndex &ll, BinIndex &ur);
  void UpdateBin(Block &blk);
  bool StartPlacement() override;
};

inline int MDPlacer::BinWidth() const {
  return bin_width_;
}

inline int MDPlacer::BinHeight() const {
  return bin_height_;
}

inline int MDPlacer::BinCountX() const {
  return bin_cnt_x_;
}

inline int MDPlacer::BinCountY() const {
  return bin_cnt_y_;
}

}

#endif //DALI_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_