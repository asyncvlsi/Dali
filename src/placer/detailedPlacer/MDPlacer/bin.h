//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BIN_HPP
#define HPCC_BIN_HPP

#include <vector>

struct BinIndex {
  int x;
  int y;
  explicit BinIndex(int i=0, int j=0): x(i), y(j){}
  bool operator<(const BinIndex& a) const {
    return (x < a.x) || ((x == a.x)&&(y < a.y));
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

  std::vector<int> CIB; // stands for cell in this bin, used to store the list of cells in this bin
};


#endif //HPCC_BIN_HPP
