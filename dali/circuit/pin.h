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

/****
 * This class stores basic information of a gate pin for placement.
 */
class Pin {
  public:
    Pin(
        std::pair<const std::string, int> *name_id_pair_ptr,
        BlockType *blk_type_ptr
    );
    Pin(
        std::pair<const std::string, int> *name_id_pair_ptr,
        BlockType *blk_type_ptr,
        double x_offset,
        double y_offset
    );

    // get name of this pin
    const std::string &Name() const;

    // get the internal index
    int Id() const;

    // compute and cache pin offset for different block orientations
    void InitOffset();

    // set offsets for N orientation, and compute offsets for all orientations
    void SetOffset(double x_offset, double y_offset);

    // get the offset along x direction for a given block orientation
    double OffsetX(BlockOrient orient = N) const;

    // get the offset along y direction for a given block orientation
    double OffsetY(BlockOrient orient = N) const;

    // add a rectangle to the pin shape list, and compute offsets for different orientations
    void AddRect(double llx, double lly, double urx, double ury);

    // add a rectangle to the pin shape list without computing offsets
    void AddRectOnly(double llx, double lly, double urx, double ury);

    // set if this pin is an input pin or output pin
    void SetIoType(bool is_input);

    // is this pin an input pin?
    bool IsInput() const;

    // does this pin has any physical shapes?
    bool IsRectEmpty() const;

    // print information of this pin
    void Report() const;
  private:
    std::vector<RectD> rect_list_;
    std::pair<const std::string, int> *name_id_pair_ptr_;
    BlockType *blk_type_ptr_;

    bool is_input_;
    bool manual_set_;
    std::vector<double> x_offset_;
    std::vector<double> y_offset_;

    // calculate and cache offsets for all orientations
    void CalculateOffset(double x_offset, double y_offset);
};

}

#endif //DALI_DALI_CIRCUIT_PIN_H_
