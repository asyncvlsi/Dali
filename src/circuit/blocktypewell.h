//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
#define DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_

#include "blocktype.h"
#include "rect.h"
#include "blocktypecluster.h"

class BlockType;
class BlockTypeCluster;

class BlockTypeWell {
 private:
  BlockType *block_type_;
  bool is_plug_;
  Rect *n_well_, *p_well_;
  BlockTypeCluster *cluster_;
 public:
  explicit BlockTypeWell(BlockType *block_type);
  ~BlockTypeWell();

  void SetCluster(BlockTypeCluster *cluster);
  BlockTypeCluster *GetCluster() const {return cluster_;}

  void SetPlug(bool is_plug) {is_plug_ = is_plug;}
  bool IsPlug() const {return is_plug_;}

  void SetNWell(double lx, double ly, double ux, double uy);
  void SetNWell(Rect &rect);
  void SetPWell(double lx, double ly, double ux, double uy);
  void SetPWell(Rect &rect);
};

#endif //DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
