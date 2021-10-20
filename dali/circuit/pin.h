//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_DALI_CIRCUIT_PIN_H_
#define DALI_DALI_CIRCUIT_PIN_H_

#include <vector>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "status.h"

namespace dali {

class BlockType;

class Pin {
  private:
    /*** essential data entries ****/
    std::vector<RectD> rect_list_;
    std::pair<const std::string, int> *name_num_pair_ptr_;
    BlockType *blk_type_ptr_;

    bool is_input_;
    bool manual_set_;
    std::vector<double> x_offset_;
    std::vector<double> y_offset_;

  public:
    Pin(std::pair<const std::string, int> *name_num_pair_ptr,
        BlockType *blk_type_ptr);
    Pin(std::pair<const std::string, int> *name_num_pair_ptr,
        BlockType *blk_type_ptr,
        double x_offset,
        double y_offset);

    const std::string &Name() const;
    int Num() const;

    void InitOffset();
    void CalculateOffset(double x_offset, double y_offset);
    void SetOffset(double x_offset, double y_offset);
    double OffsetX(BlockOrient orient = N) const;
    double OffsetY(BlockOrient orient = N) const;

    void AddRect(RectD &rect);
    void AddRect(double llx, double lly, double urx, double ury);
    void AddRectOnly(double llx, double lly, double urx, double ury);

    bool IsInput() const;
    void SetIOType(bool is_input);

    bool RectEmpty() const;
    void Report() const;
};

}

#endif //DALI_DALI_CIRCUIT_PIN_H_
