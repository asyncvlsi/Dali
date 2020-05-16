//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_
#define DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_

#include <list>

#include "blocktype.h"
#include "common/misc.h"

struct BlockTypeWell;

struct BlockTypeCluster {
 private:
  BlockTypeWell *ptr_plug_;
  BlockTypeWell *ptr_unplug_;

 public:
  BlockTypeCluster() : ptr_plug_(nullptr), ptr_unplug_(nullptr) {}
  BlockTypeCluster(BlockTypeWell *ptr_plug, BlockTypeWell *ptr_unplug) :
      ptr_plug_(ptr_plug),
      ptr_unplug_(ptr_unplug) {}

  void SetAll(BlockTypeWell *ptr_plug, BlockTypeWell *ptr_unplug) {
    ptr_plug_ = ptr_plug;
    ptr_unplug_ = ptr_unplug;
  }
  void SetPlug(BlockTypeWell *ptr_plug) { ptr_plug_ = ptr_plug; }
  void SetUnplug(BlockTypeWell *ptr_unplug) { ptr_unplug_ = ptr_unplug; }

  BlockTypeWell *GetPlug() const { return ptr_plug_; }
  BlockTypeWell *GetUnplug() const { return ptr_unplug_; }

  bool Empty() const { return ptr_plug_ == nullptr && ptr_unplug_ == nullptr; }
};

class BlockType;

struct BlockTypeWell {
  BlockType *ptype_;
  bool is_plug_;
  bool is_n_set_ = false;
  bool is_p_set_ = false;
  RectI n_rect_, p_rect_;
  int p_n_edge_ = 0;
  BlockTypeCluster *pcluster_;

  explicit BlockTypeWell(BlockType *block_type) :
      ptype_(block_type),
      is_plug_(false),
      pcluster_(nullptr) {}

  void SetCluster(BlockTypeCluster *cluster) {
    Assert(cluster != nullptr, "Cannot set BlockTypeWell pointing to an empty cluster!");
    pcluster_ = cluster;
    if (is_plug_) {
      pcluster_->SetPlug(this);
    } else {
      pcluster_->SetUnplug(this);
    }
  }
  BlockTypeCluster *GetCluster() const { return pcluster_; }

  BlockType *Type() const { return ptype_; }

  void SetPlug(bool is_plug) { is_plug_ = is_plug; }
  bool IsPlug() const { return is_plug_; }
  bool IsUnplug() const { return !is_plug_; }

  void SetNWellShape(int lx, int ly, int ux, int uy) {
    is_n_set_ = true;
    n_rect_.SetValue(lx, ly, ux, uy);
    if (is_p_set_) {
      Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
      p_n_edge_ = n_rect_.LLY();
    }
  }
  void SetNWellShape(RectI &rect) {
    is_n_set_ = true;
    n_rect_ = rect;
    if (is_p_set_) {
      Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
      p_n_edge_ = n_rect_.LLY();
    }
  }
  RectI *GetNWellShape() { return &(n_rect_); }
  void SetPWellShape(int lx, int ly, int ux, int uy) {
    is_p_set_ = true;
    p_rect_.SetValue(lx, ly, ux, uy);
    if (is_n_set_) {
      Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
      p_n_edge_ = p_rect_.URY();
    }
  }
  void SetPWellShape(RectI &rect) {
    is_p_set_ = true;
    p_rect_ = rect;
    if (is_n_set_) {
      Assert(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
    } else {
      p_n_edge_ = p_rect_.URY();
    }
  }
  RectI *GetPWellShape() { return &(p_rect_); }

  int GetPNBoundary() const { return p_n_edge_; }
  int GetNWellHeight() const { return n_rect_.Height(); }
  int GetPWellHeight() const { return p_rect_.Height(); }

  void SetWellShape(bool is_n, int lx, int ly, int ux, int uy) {
    if (is_n) {
      SetNWellShape(lx, ly, ux, uy);
    } else {
      SetPWellShape(lx, ly, ux, uy);
    }
  }
  void SetWellShape(bool is_n, RectI &rect) {
    SetWellShape(is_n, rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
  }

  bool IsNPWellAbutted() const {
    if (is_p_set_ && is_n_set_) {
      return p_rect_.URY() == n_rect_.LLY();
    }
    return true;
  }

  void Report() const {
    std::cout << "  Well of BlockType: " << *(ptype_->Name()) << "\n"
              << "    Plug: " << is_plug_ << "\n"
              << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  " << n_rect_.URX() << "  "
              << n_rect_.URY()
              << "\n"
              << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  " << p_rect_.URX() << "  "
              << p_rect_.URY()
              << "\n";
  }
};

struct WellInfo {
  std::list<BlockTypeWell> well_list_;
  std::list<BlockTypeCluster> cluster_list_;
  WellInfo() = default;
};

#endif //DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_
