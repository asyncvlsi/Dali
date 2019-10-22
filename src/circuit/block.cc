//
// Created by Yihang Yang on 2019-05-23.
//

#include "block.h"

Block::Block(BlockType *type, std::pair<const std::string, int>* name_num_pair_ptr, int llx, int lly, bool movable, BlockOrient orient) : type_(
    type), name_num_pair_ptr_(name_num_pair_ptr), llx_(llx), lly_(lly), movable_(movable), orient_(orient) {
  Assert(name_num_pair_ptr != nullptr, "Must provide a valid pointer to the std::pair<std::string, int> element in the block_name_map");
  aux_ = nullptr;
}

std::string Block::OrientStr() const {
  std::string s;
  switch (orient_) {
    case 0: { s = "N"; } break;
    case 1: { s = "S"; } break;
    case 2: { s = "W"; } break;
    case 3: { s = "E"; } break;
    case 4: { s = "FN"; } break;
    case 5: { s = "FS"; } break;
    case 6: { s = "FW"; } break;
    case 7: { s = "FE"; } break;
    default: {
      Assert(false, "Block orientation error! This should not happen!");
    }
  }
  return s;
}

void Block::Report() {
  std::cout << "Block Name: " << *Name() << "\n";
  std::cout << "Block Type: " << *(Type()->Name()) << "\n";
  std::cout << "Width and Height: " << Width() << " " << Height() << "\n";
  std::cout << "lower Left corner: " << LLX() << " " << LLY() << "\n";
  std::cout << "movability: " << IsMovable() << "\n";
  std::cout << "orientation: " << OrientStr() << "\n";
  std::cout << "assigned primary key: " << Num() << "\n";
}
