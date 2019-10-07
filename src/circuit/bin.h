//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BIN_HPP
#define HPCC_BIN_HPP

#include <vector>

struct bin_index {
  int x;
  int y;
  explicit bin_index(int i=0, int j=0): x(i), y(j){}
};

class Bin {
 private:
  int left_;
  int bottom_;
  int width_;
  int height_;
 public:
  Bin();
  Bin(int left, int bottom, int width, int height);
  void SetLeft(int left);
  void SetBottom(int bottom);
  void SetWidth(int width);
  void SetHeight(int height);
  int Left() const;
  int Bottom() const;
  int Width() const;
  int Height() const;
  int Right() const;
  int Top() const;

  std::vector<int> CIB; // stands for cell in this bin, used to store the list of cells in this bin
};


#endif //HPCC_BIN_HPP
