//
// Created by Yihang Yang on 2019-05-20.
//

#ifndef HPCC_BINDLA_HPP
#define HPCC_BINDLA_HPP

#include <vector>

class bin_t {
private:
  int _left;
  int _bottom;
  int _width;
  int _height;
public:
  bin_t();
  bin_t(int left, int bottom, int width, int height);

  void set_left(int left);
  int left();
  void set_bottom(int bottom);
  int bottom();
  void set_width(int width);
  int width();
  void set_height(int height);
  int height();

  std::vector<int> CIB; // stands for cell in this bin, used to store the list of cells in this bin
};


#endif //HPCC_BINDLA_HPP
