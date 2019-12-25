//
// Created by Yihang Yang on 2019/12/23.
//

#include "common/misc.h"
#include "blocktypewell.h"

BlockTypeWell::BlockTypeWell(BlockType *block_type):
    block_type_(block_type),
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
}

void BlockTypeWell::SetNWell(double lx, double ly, double ux, double uy) {
  if (n_well_== nullptr) {
    n_well_ = new Rect(lx,ly,ux,uy);
  } else {
    n_well_->SetValue(lx,ly,ux,uy);
  }
}

void BlockTypeWell::SetNWell(Rect &rect) {
  SetNWell(rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
}

void BlockTypeWell::SetPWell(double lx, double ly, double ux, double uy) {
  if (p_well_== nullptr) {
    p_well_ = new Rect(lx,ly,ux,uy);
  } else {
    p_well_->SetValue(lx,ly,ux,uy);
  }
}

void BlockTypeWell::SetPWell(Rect &rect) {
  SetPWell(rect.LLX(), rect.LLY(), rect.URX(), rect.URY());
}