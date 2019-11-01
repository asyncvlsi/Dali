//
// Created by Yihang Yang on 10/31/19.
//

#ifndef HPCC_SRC_CIRCUIT_PIN_H_
#define HPCC_SRC_CIRCUIT_PIN_H_

#include "rect.h"
#include <vector>

class Pin {
 private:
  /*** essential data entries ****/
  std::vector<Rect> rect_list_;
  std::pair<const std::string, int>* name_num_pair_ptr_;
  double x_offset_;
  double y_offset_;
  bool manual_set_;

 public:
  explicit Pin(std::pair<const std::string, int>* name_num_pair_ptr);
  Pin(std::pair<const std::string, int>* name_num_pair_ptr, int x_offset, int y_offset);

  const std::string *Name() const;
  int Num() const;
  void InitOffset();
  void SetOffset(double x_offset, double y_offset);
  double XOffset() const;
  double YOffset() const;
  void AddRect(Rect &rect);
  void AddRect(double llx, double lly, double urx, double ury);
  bool Empty();

  /*friend std::ostream& operator<<(std::ostream& os, const Pin &pin) {
    os << pin.Name() << " (" << pin.XOffset() << ", " << pin.YOffset() << ")";
    return os;
  }*/
};

#endif //HPCC_SRC_CIRCUIT_PIN_H_
