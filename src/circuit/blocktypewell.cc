//
// Created by Yihang Yang on 2019/12/23.
//

#include "common/misc.h"
#include "blocktypewell.h"

BlockTypeWell::BlockTypeWell(BlockType *block_type):
    type_(block_type),
    is_plug_(false),
    n_well_(nullptr),
    p_well_(nullptr),
    cluster_(nullptr) {}

BlockTypeWell::~BlockTypeWell() {
  delete n_well_;
  delete p_well_;
}
void BlockTypeWell::SetCluster(BlockTypeCluster *cluster) {
  Assert(cluster != nullptr, "Cannot set BlockTypeWell pointing to an empty cluster!");
  cluster_ = cluster;
  if (is_plug_) {
    cluster_->SetPlug(this);
  } else {
    cluster_->SetUnplug(this);
  }
}

void BlockTypeWell::SetNWellShape(double lx, double ly, double ux, double uy) {
  if (n_well_== nullptr) {
    n_well_ = new Rect(lx,ly,ux,uy);
  } else {
    n_well_->SetValue(lx,ly,ux,uy);
  }
}

void BlockTypeWell::SetNWellShape(Rect &rect) {
  SetNWellShape(rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
}

void BlockTypeWell::SetPWellShape(double lx, double ly, double ux, double uy) {
  if (p_well_== nullptr) {
    p_well_ = new Rect(lx,ly,ux,uy);
  } else {
    p_well_->SetValue(lx,ly,ux,uy);
  }
}

void BlockTypeWell::SetPWellShape(Rect &rect) {
  SetPWellShape(rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
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
  if (n_well_ != nullptr) {
    std::cout << "    Nwell: " << n_well_->LLX() << "  " << n_well_->LLY() << "  " << n_well_->URX() << "  " << n_well_->URY() << "\n";
  }
  if (p_well_ != nullptr) {
    std::cout << "    Pwell: " << p_well_->LLX() << "  " << p_well_->LLY() << "  " << p_well_->URX() << "  " << p_well_->URY() << "\n";
  }
}
