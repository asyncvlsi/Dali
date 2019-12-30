//
// Created by Yihang Yang on 2019/12/23.
//

#include "common/misc.h"
#include "blocktypewell.h"

BlockTypeWell::BlockTypeWell(BlockType *block_type):
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

void BlockTypeWell::SetWellShape(bool is_n, double lx, double ly, double ux, double uy) {
  if (is_n) {
    SetNWellShape(lx, ly, ux, uy);
  } else {
    SetPWellShape(lx,ly,ux, uy);
  }
}

void BlockTypeWell::SetWellShape(bool is_n, Rect &rect) {
  SetWellShape(is_n, rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
}

void BlockTypeWell::Report() {
  std::cout << "  Well of BlockType: " << *(type_->Name()) << "\n";
  std::cout << "    Plug: " << is_plug_ << "\n";
  std::cout << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  " << n_rect_.URX() << "  " << n_rect_.URY() << "\n";
  std::cout << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  " << p_rect_.URX() << "  " << p_rect_.URY() << "\n";
}
