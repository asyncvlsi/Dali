//
// Created by Yihang Yang on 2019-08-12.
//

#include "simplblockaux.h"
#include "../../../common/misc.h"

SimPLBlockAux::SimPLBlockAux(Block *block): BlockAux(block) {}

bool SimPLBlockAux::NetExist(Net *net) {
  return net_set.find(net) != net_set.end();
}

void SimPLBlockAux::InsertNet(Net *net) {
  Assert(!NetExist(net), "Insertion not allowed: net " + *(net->Name()) + " has been in net_set of " + *(GetBlock()->Name()));
  net_set.insert(net);
}

int SimPLBlockAux::B2BRowSizeX() {
  /**** this member function returns the number of elements (space) in the Matrix for this row
   * net.UpdateMaxMinX() or net.UpdateMaxMinY() should be called before ****/
  int size = 1;
  Block *blk_ptr = GetBlock();
  if (blk_ptr->IsMovable()) {
    Block *max_blk_ptr = nullptr;
    Block *min_blk_ptr = nullptr;
    for (auto &&net_ptr: net_set) {
      if (net_ptr->P() == 1) continue;
      max_blk_ptr = net_ptr->MaxBlockX();
      min_blk_ptr = net_ptr->MinBlockX();
      if ((blk_ptr == max_blk_ptr) || (blk_ptr == min_blk_ptr)) {
        size += net_ptr->P() - 1;
      } else {
        size += 2;
      }
    }
  }
  return size;
}

int SimPLBlockAux::B2BRowSizeY() {
  /**** this member function returns the number of elements (space) in the Matrix for this row
   * net.UpdateMaxMinX() or net.UpdateMaxMinY() should be called before ****/
  int size = 1;
  Block *blk_ptr = GetBlock();
  if (blk_ptr->IsMovable()) {
    Block *max_blk_ptr = nullptr;
    Block *min_blk_ptr = nullptr;
    for (auto &&net_ptr: net_set) {
      if (net_ptr->P() == 1) continue;
      max_blk_ptr = net_ptr->MaxBlockY();
      min_blk_ptr = net_ptr->MinBlockY();
      if ((blk_ptr == max_blk_ptr) || (blk_ptr == min_blk_ptr)) {
        size += net_ptr->P() - 1;
      } else {
        size += 2;
      }
    }
  }
  return size;
}

void SimPLBlockAux::ReportAux() {
  std::cout << "Linked block: " << *(GetBlock()->Name()) << std::endl;
  for (auto &&net_ptr: net_set) {
    std::cout << "  " << *(net_ptr->Name());
  }
  std::cout << std::endl;
}