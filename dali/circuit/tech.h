//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_DALI_CIRCUIT_TECH_H_
#define DALI_DALI_CIRCUIT_TECH_H_

#include <list>
#include <unordered_map>
#include <vector>

#include "block.h"
#include "blocktype.h"
#include "layer.h"

namespace dali {

class Tech {
    friend class Circuit;
  public:
    Tech();
    ~Tech();

    // get all kinds of well tap cells
    std::vector<BlockType *> &WellTapCellPtrs();

    // get the dummy BlockType for IOPINs
    BlockType *IoDummyBlkTypePtr();

    // get Nwell layer
    WellLayer &NwellLayer();

    // get Pwell layer
    WellLayer &PwellLayer();

    // is Nwell parameters available?
    bool IsNwellSet() const;

    // is Pwell parameters available?
    bool IsPwellSet() const;

    // is any well layer available?
    bool IsWellInfoSet() const;

    // print information
    void Report() const;

  private:
    /**** manufacturing grid ****/
    double manufacturing_grid_ = 0;
    int database_microns_ = 0;

    /*** *grid value along X and Y ****/
    bool is_grid_set_ = false;
    double grid_value_x_ = 0;
    double grid_value_y_ = 0;

    /**** metal layers ****/
    std::vector<MetalLayer> metal_list_;
    std::unordered_map<std::string, int> metal_name_map_;

    /**** macros ****/
    std::unordered_map<std::string, BlockType *> block_type_map_;
    std::list<BlockTypeWell> well_list_;
    BlockType *io_dummy_blk_type_ptr_ = nullptr;
    std::vector<BlockType *> well_tap_cell_ptrs_;

    /**** row height ****/
    double row_height_ = 0;
    bool row_height_set_ = false;

    /**** N/P well info ****/
    bool n_set_ = false;
    bool p_set_ = false;
    WellLayer nwell_layer_;
    WellLayer pwell_layer_;
    double same_diff_spacing_;
    double any_diff_spacing_;
};

}

#endif //DALI_DALI_CIRCUIT_TECH_H_
