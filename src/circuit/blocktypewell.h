//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
#define DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_

#include <list>

#include "blocktype.h"
#include "common/misc.h"

class BlockType;
class BlockTypeWell;

struct BlockTypeCluster{
 private:
  BlockTypeWell *plug_;
  BlockTypeWell *unplug_;
 public:
  BlockTypeCluster(): plug_(nullptr), unplug_(nullptr) {}
  BlockTypeCluster(BlockTypeWell *plug, BlockTypeWell *unplug): plug_(plug), unplug_(unplug) {}
  void SetAll(BlockTypeWell *plug, BlockTypeWell *unplug) {plug_ = plug; unplug_ = unplug;}
  void SetPlug(BlockTypeWell *plug) {plug_ = plug;}
  void SetUnplug(BlockTypeWell *unplug) {unplug_ = unplug;}
  BlockTypeWell *GetPlug() const {return plug_;}
  BlockTypeWell *GetUnplug() const {return unplug_;}
  bool Empty() {return plug_ == nullptr && unplug_ == nullptr;}
};


class BlockTypeWell {
 private:
  BlockType *type_;
  bool is_plug_;
  RectI n_rect_, p_rect_;
 public:
  BlockTypeCluster *cluster_;
  explicit BlockTypeWell(BlockType *block_type);

  void SetCluster(BlockTypeCluster *cluster);
  BlockTypeCluster *GetCluster() const {return cluster_;}

  BlockType *Type() {return type_;}

  void SetPlug(bool is_plug) {is_plug_ = is_plug;}
  bool IsPlug() const {return is_plug_;}
  bool IsUnplug() const {return !is_plug_;}

  void SetNWellShape(int lx, int ly, int ux, int uy) {n_rect_.SetValue(lx, ly, ux, uy);}
  void SetNWellShape(RectI &rect) { n_rect_ = rect;}
  RectI *GetNWellShape() {return &(n_rect_);}
  void SetPWellShape(int lx, int ly, int ux, int uy) {p_rect_.SetValue(lx, ly, ux, uy);}
  void SetPWellShape(RectI &rect) { p_rect_ =rect;}
  RectI *GetPWellShape() {return &(p_rect_);}

  int GetPNBoundary() {return p_rect_.URY();}

  void SetWellShape(bool is_n, int lx, int ly, int ux, int uy);
  void SetWellShape(bool is_n, RectI &rect);

  void Report();
};

struct WellInfo {
  std::list<BlockTypeWell> well_list_;
  std::list<BlockTypeCluster> cluster_list_;
  WellInfo() = default;
};

#endif //DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
