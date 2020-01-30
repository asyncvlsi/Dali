//
// Created by Yihang Yang on 2019/12/23.
//

#include "blocktypewell.h"

#include "common/misc.h"

BlockTypeCluster::BlockTypeCluster() :
    plug_(nullptr),
    unplug_(nullptr) {}

BlockTypeCluster::BlockTypeCluster(BlockTypeWell *plug, BlockTypeWell *unplug) :
    plug_(plug),
    unplug_(unplug) {}

BlockTypeWell::BlockTypeWell(BlockType *block_type) :
    type_(block_type),
    is_plug_(false),
    cluster_(nullptr) {}

void BlockTypeWell::SetCluster(BlockTypeCluster *cluster) {
  Assert(cluster != nullptr, "Cannot set BlockTypeWell pointing to an empty cluster!");
  cluster_ = cluster;
  if (is_plug_) {
    cluster_->SetPlug(this);
  } else {
    cluster_->SetUnplug(this);
  }
}

void BlockTypeWell::SetWellShape(bool is_n, int lx, int ly, int ux, int uy) {
  if (is_n) {
    SetNWellShape(lx, ly, ux, uy);
  } else {
    SetPWellShape(lx, ly, ux, uy);
  }
}

void BlockTypeWell::SetWellShape(bool is_n, RectI &rect) {
  SetWellShape(is_n, rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
}

void BlockTypeWell::Report() {
  std::cout << "  Well of BlockType: " << *(type_->Name()) << "\n"
            << "    Plug: " << is_plug_ << "\n"
            << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  " << n_rect_.URX() << "  " << n_rect_.URY()
            << "\n"
            << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  " << p_rect_.URX() << "  " << p_rect_.URY()
            << "\n";
}
