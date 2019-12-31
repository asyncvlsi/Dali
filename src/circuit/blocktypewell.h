//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
#define DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_

#include <list>
#include "blocktype.h"
#include "rect.h"

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
  Rect n_rect_, p_rect_;
 public:
  BlockTypeCluster *cluster_;
  explicit BlockTypeWell(BlockType *block_type);

  void SetCluster(BlockTypeCluster *cluster);
  BlockTypeCluster *GetCluster() const {return cluster_;}

  BlockType *Type() {return type_;}

  void SetPlug(bool is_plug) {is_plug_ = is_plug;}
  bool IsPlug() const {return is_plug_;}
  bool IsUnplug() const {return !is_plug_;}

  void SetNWellShape(double lx, double ly, double ux, double uy) {n_rect_.SetValue(lx, ly, ux, uy);}
  void SetNWellShape(Rect &rect) { n_rect_ = rect;}
  Rect *GetNWellShape() {return &(n_rect_);}
  void SetPWellShape(double lx, double ly, double ux, double uy) {p_rect_.SetValue(lx, ly, ux, uy);}
  void SetPWellShape(Rect &rect) { p_rect_ =rect;}
  Rect *GetPWellShape() {return &(p_rect_);}

  int GetPNBoundary() {return p_rect_.URY();}

  void SetWellShape(bool is_n, double lx, double ly, double ux, double uy);
  void SetWellShape(bool is_n, Rect &rect);

  void Report();
};

struct WellInfo {
  std::list<BlockTypeWell> well_list_;
  std::list<BlockTypeCluster> cluster_list_;
  WellInfo() = default;
};

#endif //DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
