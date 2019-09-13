//
// Created by yihan on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_

#include "../../../circuit/blockaux.h"

struct Value2D {
  double x;
  double y;
  explicit Value2D(double x=0, double y=0): x(x), y(y) {}

  // assignment operator modifies object, therefore non-const
  Value2D& operator=(const Value2D& a) = default;

  // addop. doesn't modify object. therefore const.
  Value2D operator+(const Value2D& a) const {
    return Value2D(a.x+x, a.y+y);
  }

  // equality comparison. doesn't modify object. therefore const.
  bool operator==(const Value2D& a) const {
    return (x == a.x && y == a.y);
  }
};

class MDBlkAux: public BlockAux {
 private:
  double fx_;
  double fy_;
  double vx_;
  double vy_;
 public:
  explicit MDBlkAux(Block* blk_ptr);
  void SetFx(double fx);
  void SetFy(double fy);
  void SetVx(double vx);
  void SetVy(double vy);
  double Fx();
  double Fy();
  double Vx();
  double Vy();
  Value2D GetForce(Block &blk);
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
