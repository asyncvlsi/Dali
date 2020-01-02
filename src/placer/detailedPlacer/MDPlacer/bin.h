//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef DALI_BIN_HPP
#define DALI_BIN_HPP

#include <iostream>
#include <unordered_set>

struct BinIndex {
  int x;
  int y;
  explicit BinIndex(int i=0, int j=0): x(i), y(j){}
  bool operator<(const BinIndex& a) const {
    return (x < a.x) || ((x == a.x)&&(y < a.y));
  }
  bool operator==(const BinIndex& a) const {
    return (x == a.x) && (y == a.y);
  }
};

class Bin {
 private:
  int left_;
  int bottom_;
  int right_;
  int top_;
 public:
  Bin();
  Bin(int left, int bottom, int right, int top);
  void SetLeft(int left);
  void SetBottom(int bottom);
  void SetRight(int right);
  void SetTop(int top);
  int Left() const;
  int Bottom() const;
  int Right() const;
  int Top() const;
  int Width() const;
  int Height() const;

  std::unordered_set<int> block_set; // stands for cell in this bin, used to store the list of cells in this bin
  void AddBlk(int blk_num);
  void RemoveBlk(int blk_num);
};

inline void Bin::SetLeft(int left) {
  left_ = left;
}

inline void Bin::SetBottom(int bottom) {
  bottom_ = bottom;
}

inline void Bin::SetRight(int right) {
  right_ = right;
}

inline void Bin::SetTop(int top) {
  top_ = top;
}

inline int Bin::Left() const {
  return left_;
}

inline int Bin::Bottom() const {
  return bottom_;
}

inline int Bin::Right() const {
  return right_;
}

inline int Bin::Top() const {
  return top_;
}

inline int Bin::Width() const {
  return right_ - left_;
}

inline int Bin::Height() const {
  return  top_ - bottom_;
}

inline void Bin::AddBlk(int blk_num) {
  //std::cout << block_set.size() << std::endl;
  if(block_set.find(blk_num) != block_set.end()) {
    std::cout << "Error, block already in set!" << std::endl;
    exit(1);
  }
  block_set.insert(blk_num);
}

inline void Bin::RemoveBlk(int blk_num) {
  if(block_set.find(blk_num) == block_set.end()) {
    std::cout << "Error, block not in set, cannot be removed!" << std::endl;
    exit(1);
  }
  block_set.erase(blk_num);
}


#endif //DALI_BIN_HPP
