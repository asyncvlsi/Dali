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
#include "blocktypewell.h"
#include "layer.h"

namespace dali {

class Tech {
  public:
    /****manufacturing grid****/
    double manufacturing_grid_ = 0;
    int database_microns_ = 0;

    /****grid value in X and Y****/
    bool grid_set_ = false;
    double grid_value_x_ = 0;
    double grid_value_y_ = 0;

    /****metal layers****/
    std::vector<MetalLayer> metal_list_;
    std::unordered_map<std::string, int> metal_name_map_;

    /****macros****/
    std::unordered_map<std::string, BlockType *> block_type_map_;
    std::list<BlockTypeWell> well_list_;
    BlockType *io_dummy_blk_type_ptr_ = nullptr;
    // shared pointers of well tap cells, need to manually clear this vector in destructor
    std::vector<BlockType *> well_tap_cell_ptrs_;

    /****row height****/
    double row_height_ = 0;
    bool row_height_set_ = false;

    /****N/P well info****/
    bool n_set_;
    bool p_set_;
    WellLayer *n_layer_ptr_;
    WellLayer *p_layer_ptr_;
    double same_diff_spacing_;
    double any_diff_spacing_;

    /****temporary data for Si2 LEF/DEF parser****/
    BlockType *last_blk_type_ = nullptr;

    /********/
    Tech();
    ~Tech();

    std::vector<BlockType *> &WellTapCellRef() { return well_tap_cell_ptrs_; }

    WellLayer *GetNLayer() const { return n_layer_ptr_; }
    WellLayer *GetPLayer() const { return p_layer_ptr_; }
    void SetNLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
    void SetPLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
    void SetDiffSpacing(double same_diff, double any_diff) {
        same_diff_spacing_ = same_diff;
        any_diff_spacing_ = any_diff;
    }

    bool IsWellInfoSet() const { return (GetNLayer() == nullptr) && (GetPLayer() == nullptr); }
    void Report() const;
};

}

#endif //DALI_DALI_CIRCUIT_TECH_H_
