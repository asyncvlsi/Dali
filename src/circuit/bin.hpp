//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BIN_HPP
#define HPCC_BIN_HPP

#include <vector>

struct bin_index {
  int iloc;
  int jloc;
  explicit bin_index(int i=0, int j=0): iloc(i), jloc(j){}
};

class bin_t {
private:
  int _left;
  int _bottom;
  int _width;
  int _height;
public:
  bin_t();
  bin_t(int left_arg, int bottom_arg, int width_arg, int height_arg);

  void set_left(int left_arg);
  int left();
  void set_bottom(int bottom_arg);
  int bottom();
  void set_width(int width_arg);
  int width();
  void set_height(int height_arg);
  int height();
  int right();
  int top();

  std::vector<int> CIB; // stands for cell in this bin, used to store the list of cells in this bin
};


#endif //HPCC_BIN_HPP
