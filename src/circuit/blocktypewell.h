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
  BlockTypeWell *plug_cell_ptr_;
  BlockTypeWell *unplug_cell_ptr_;

 public:
  BlockTypeCluster() : plug_cell_ptr_(nullptr), unplug_cell_ptr_(nullptr) {}
  BlockTypeCluster(BlockTypeWell *plug_cell_ptr, BlockTypeWell *unplug_cell_ptr) :
      plug_cell_ptr_(plug_cell_ptr),
      unplug_cell_ptr_(unplug_cell_ptr) {}

  void SetAll(BlockTypeWell *plug_cell_ptr, BlockTypeWell *unplug_cell_ptr) {
    plug_cell_ptr_ = plug_cell_ptr;
    unplug_cell_ptr_ = unplug_cell_ptr;
  }
  void SetPlug(BlockTypeWell *plug_cell_ptr) { plug_cell_ptr_ = plug_cell_ptr; }
  void SetUnplug(BlockTypeWell *unplug_cell_ptr) { unplug_cell_ptr_ = unplug_cell_ptr; }

  BlockTypeWell *GetPlug() const { return plug_cell_ptr_; }
  BlockTypeWell *GetUnplug() const { return unplug_cell_ptr_; }

  bool Empty() const { return plug_cell_ptr_ == nullptr && unplug_cell_ptr_ == nullptr; }
};

class BlockType;

struct BlockTypeWell {
  BlockType *type_ptr_;
  bool is_plug_;
  bool is_n_set_ = false;
  bool is_p_set_ = false;
  RectI n_rect_, p_rect_;
  int p_n_edge_ = 0;
  BlockTypeCluster *cluster_ptr_;

  explicit BlockTypeWell(BlockType *type_ptr) :
      type_ptr_(type_ptr),
      is_plug_(false),
      cluster_ptr_(nullptr) {}

  void SetCluster(BlockTypeCluster *cluster_ptr) {
    Assert(cluster_ptr != nullptr,
           "Cannot set BlockTypeWell pointing to an empty cluster: BlockTypeWell::SetCluster()");
    cluster_ptr_ = cluster_ptr;
    if (is_plug_) {
      cluster_ptr_->SetPlug(this);
    } else {
      cluster_ptr_->SetUnplug(this);
    }
  }
  BlockTypeCluster *GetCluster() const { return cluster_ptr_; }

  BlockType *Type() const { return type_ptr_; }

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
    std::cout
        << "  Well of BlockType: " << *(type_ptr_->Name()) << "\n"
        << "    Plug: " << is_plug_ << "\n"
        << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  " << n_rect_.URX() << "  " << n_rect_.URY() << "\n"
        << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  " << p_rect_.URX() << "  " << p_rect_.URY() << "\n";
  }
};

struct WellInfo {
  std::list<BlockTypeWell> well_list_;
  std::list<BlockTypeCluster> cluster_list_;
  WellInfo() = default;
};

#endif //DALI_SRC_CIRCUIT_BLOCKTYPEWELL_H_
