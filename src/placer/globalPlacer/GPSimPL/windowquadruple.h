//
// Created by Yihang Yang on 11/18/19.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_WINDOWQUADRUPLE_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_WINDOWQUADRUPLE_H_

struct WindowQuadruple {
  int llx, lly, urx, ury;
  bool Cover(WindowQuadruple &rhs) {
    return llx <= rhs.llx && lly <= rhs.lly && urx >= rhs.urx && ury >= rhs.ury;
  }
  bool AspectRatioInRange(double lo, double hi) {
    double ratio = (ury - lly + 1) / double(urx - llx + 1);
    return ratio <= hi && ratio >= lo;
  }
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_WINDOWQUADRUPLE_H_
