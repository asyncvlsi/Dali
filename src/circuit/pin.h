//
// Created by Yihang Yang on 10/31/19.
//

#ifndef HPCC_SRC_CIRCUIT_PIN_H_
#define HPCC_SRC_CIRCUIT_PIN_H_

#include "rect.h"
#include <vector>
#include "status.h"

class Pin {
 private:
  /*** essential data entries ****/
  std::vector<Rect> rect_list_;
  std::pair<const std::string, int>* name_num_pair_ptr_;
  bool manual_set_;
  std::vector<double> x_offset_;
  std::vector<double> y_offset_;

 public:
  explicit Pin(std::pair<const std::string, int>* name_num_pair_ptr);
  Pin(std::pair<const std::string, int>* name_num_pair_ptr, int x_offset, int y_offset);

  const std::string *Name() const {return &(name_num_pair_ptr_->first);}
  int Num() const {return name_num_pair_ptr_->second;}
  void InitOffset();
  void SetOffset(double x_offset, double y_offset);
  double XOffset(BlockOrient orient=N) const;
  double YOffset(BlockOrient orient=N) const;
  void AddRect(Rect &rect);
  void AddRect(double llx, double lly, double urx, double ury);
  bool Empty() const {return rect_list_.empty();}
  void Report() const;
};

#endif //HPCC_SRC_CIRCUIT_PIN_H_
