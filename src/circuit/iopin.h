//
// Created by Yihang Yang on 11/28/19.
//

#ifndef HPCC_SRC_CIRCUIT_IOPIN_H_
#define HPCC_SRC_CIRCUIT_IOPIN_H_

#include <string>
#include "net.h"
#include "status.h"
#include "rect.h"

class IOPin {
 private:
  std::pair<const std::string, int>* name_num_pair_;
  Net *net_;
  std::string *layer_;
  int lx_, ly_;
  PlaceStatus place_status_;
  Rect rect_;
 public:
  explicit IOPin(std::pair<const std::string, int>* name_num_pair);
  IOPin(std::pair<const std::string, int>* name_num_pair, int lx, int ly);

  void SetSize(double llx, double lly, double urx, double ury) {rect_.SetValue(llx, lly, urx, ury);}
};

#endif //HPCC_SRC_CIRCUIT_IOPIN_H_
