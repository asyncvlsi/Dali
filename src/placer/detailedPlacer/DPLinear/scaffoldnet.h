//
// Created by Yihang Yang on 9/3/19.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_SCAFFOLDNET_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_SCAFFOLDNET_H_

#include <circuit/block.h>

class ScaffoldNet {
 private:
  double weight_;
  Block *block_0_;
  Block *block_1_;
  double x_offset_0_;
  double y_offset_0_;
  double x_offset_1_;
  double y_offset_1_;
 public:

  ScaffoldNet(double weight,
              Block *block_0,
              Block *block_1,
              double x_offset_0,
              double y_offset_0,
              double x_offset_1,
              double y_offset_1);
  ScaffoldNet(double weight, Block *block_0, Block *block_1);
  double Weight() const;
  Block *Block0() const;
  Block *Block1() const;
  double XOffset0() const;
  double YOffset0() const;
  double XOffset1() const;
  double YOffset1() const;
  double HPWLX() const;
  double HPWLY() const;
  double HPWL() const;
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_DPLINEAR_SCAFFOLDNET_H_
