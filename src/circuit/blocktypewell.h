//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
#define DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_

#include <list>

#include "blocktype.h"
#include "common/misc.h"

class BlockTypeWell;

struct BlockTypeCluster {
 private:
  BlockTypeWell *plug_;
  BlockTypeWell *unplug_;

 public:
  BlockTypeCluster();
  BlockTypeCluster(BlockTypeWell *plug, BlockTypeWell *unplug);

  void SetAll(BlockTypeWell *plug, BlockTypeWell *unplug);
  void SetPlug(BlockTypeWell *plug);
  void SetUnplug(BlockTypeWell *unplug);

  BlockTypeWell *GetPlug() const;
  BlockTypeWell *GetUnplug() const;

  bool Empty() const;
};

class BlockType;

class BlockTypeWell {
 private:
  BlockType *type_;
  bool is_plug_;
  bool is_n_set_ = false;
  bool is_p_set_ = false;
  RectI n_rect_, p_rect_;
  int p_n_edge_ = 0;

 public:
  BlockTypeCluster *cluster_;
  explicit BlockTypeWell(BlockType *block_type);

  void SetCluster(BlockTypeCluster *cluster);
  BlockTypeCluster *GetCluster() const;

  BlockType *Type();

  void SetPlug(bool is_plug);
  bool IsPlug() const;
  bool IsUnplug() const;

  void SetNWellShape(int lx, int ly, int ux, int uy);
  void SetNWellShape(RectI &rect);
  RectI *GetNWellShape();
  void SetPWellShape(int lx, int ly, int ux, int uy);
  void SetPWellShape(RectI &rect);
  RectI *GetPWellShape();

  int GetPNBoundary() const;
  int GetNWellHeight();
  int GetPWellHeight();

  void SetWellShape(bool is_n, int lx, int ly, int ux, int uy);
  void SetWellShape(bool is_n, RectI &rect);

  bool IsNPWellAbutted();

  void Report();
};

struct WellInfo {
  std::list<BlockTypeWell> well_list_;
  std::list<BlockTypeCluster> cluster_list_;
  WellInfo() = default;
};

inline void BlockTypeCluster::SetAll(BlockTypeWell *plug, BlockTypeWell *unplug) {
  plug_ = plug;
  unplug_ = unplug;
}

inline void BlockTypeCluster::SetPlug(BlockTypeWell *plug) {
  plug_ = plug;
}

inline void BlockTypeCluster::SetUnplug(BlockTypeWell *unplug) {
  unplug_ = unplug;
}

inline BlockTypeWell *BlockTypeCluster::GetPlug() const {
  return plug_;
}

inline BlockTypeWell *BlockTypeCluster::GetUnplug() const {
  return unplug_;
}

inline bool BlockTypeCluster::Empty() const {
  return plug_ == nullptr && unplug_ == nullptr;
}

inline BlockTypeCluster *BlockTypeWell::GetCluster() const {
  return cluster_;
}

inline BlockType *BlockTypeWell::Type() {
  return type_;
}

inline void BlockTypeWell::SetPlug(bool is_plug) {
  is_plug_ = is_plug;
}

inline bool BlockTypeWell::IsPlug() const {
  return is_plug_;
}

inline bool BlockTypeWell::IsUnplug() const {
  return !is_plug_;
}

inline void BlockTypeWell::SetNWellShape(int lx, int ly, int ux, int uy) {
  is_n_set_ = true;
  n_rect_.SetValue(lx, ly, ux, uy);
  if (is_p_set_) {
    Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
  } else {
    p_n_edge_ = n_rect_.LLY();
  }
}

inline void BlockTypeWell::SetNWellShape(RectI &rect) {
  is_n_set_ = true;
  n_rect_ = rect;
  if (is_p_set_) {
    Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
  } else {
    p_n_edge_ = n_rect_.LLY();
  }
}

inline RectI *BlockTypeWell::GetNWellShape() {
  return &(n_rect_);
}

inline void BlockTypeWell::SetPWellShape(int lx, int ly, int ux, int uy) {
  is_p_set_ = true;
  p_rect_.SetValue(lx, ly, ux, uy);
  if (is_n_set_) {
    Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
  } else {
    p_n_edge_ = p_rect_.URY();
  }
}

inline void BlockTypeWell::SetPWellShape(RectI &rect) {
  is_p_set_ = true;
  p_rect_ = rect;
  if (is_n_set_) {
    Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
  } else {
    p_n_edge_ = p_rect_.URY();
  }
}

inline RectI *BlockTypeWell::GetPWellShape() {
  return &(p_rect_);
}

inline int BlockTypeWell::GetPNBoundary() const {
  return p_n_edge_;
}

inline int BlockTypeWell::GetNWellHeight() {
  return n_rect_.Height();
}

inline int BlockTypeWell::GetPWellHeight() {
  return p_rect_.Height();
}

inline bool BlockTypeWell::IsNPWellAbutted() {
  if (is_p_set_ && is_n_set_) {
    return p_rect_.URY() == n_rect_.LLY();
  }
  return true;
}

#endif //DALI_SRC_CIRCUIT_WELLBLKTYPEAUX_H_
