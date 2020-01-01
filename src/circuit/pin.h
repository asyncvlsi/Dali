//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_PIN_H_
#define DALI_SRC_CIRCUIT_PIN_H_

#include "common//misc.h"
#include <vector>
#include "status.h"
#include "blocktype.h"

class BlockType;

class Pin {
 private:
  /*** essential data entries ****/
  std::vector<RectD> rect_list_;
  std::pair<const std::string, int>* name_num_pair_ptr_;
  BlockType *blk_type_;
  bool manual_set_;
  std::vector<double> x_offset_;
  std::vector<double> y_offset_;

 public:
  Pin(std::pair<const std::string, int>* name_num_pair_ptr, BlockType *blk_type);
  Pin(std::pair<const std::string, int>* name_num_pair_ptr, BlockType *blk_type, double x_offset, double y_offset);

  const std::string *Name() const {return &(name_num_pair_ptr_->first);}
  int Num() const {return name_num_pair_ptr_->second;}
  void InitOffset();
  void CalculateOffset(double x_offset, double y_offset);
  void SetOffset(double x_offset, double y_offset);
  double XOffset(BlockOrient orient=N) const;
  double YOffset(BlockOrient orient=N) const;
  void AddRect(RectD &rect);
  void AddRect(double llx, double lly, double urx, double ury);
  bool Empty() const {return rect_list_.empty();}
  void Report() const;
};

#endif //DALI_SRC_CIRCUIT_PIN_H_
