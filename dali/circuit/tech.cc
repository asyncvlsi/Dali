//
// Created by Yihang Yang on 12/22/19.
//

#include "tech.h"

namespace dali {

Tech::Tech()
    : n_set_(false),
      p_set_(false),
      same_diff_spacing_(-1),
      any_diff_spacing_(-1) {}

/****
* This destructor free the memory allocated for unordered_map<key, *T>
* because T is initialized by
*    auto *T = new T();
* ****/
Tech::~Tech() {
    for (auto &pair: block_type_map_) {
        delete pair.second;
    }
}

std::vector<BlockType *> &Tech::WellTapCellPtrs() {
    return well_tap_cell_ptrs_;
}

BlockType *Tech::IoDummyBlkTypePtr() {
    return io_dummy_blk_type_ptr_;
}

WellLayer &Tech::NwellLayer() {
    return nwell_layer_;
}

WellLayer &Tech::PwellLayer() {
    return pwell_layer_;
}

bool Tech::IsNwellSet() const {
    return n_set_;
}

bool Tech::IsPwellSet() const {
    return p_set_;
}

bool Tech::IsWellInfoSet() const {
    return n_set_ || p_set_;
}

void Tech::Report() const {
    if (n_set_) {
        BOOST_LOG_TRIVIAL(info) << "  Layer name: nwell\n";
        nwell_layer_.Report();
    }
    if (p_set_) {
        BOOST_LOG_TRIVIAL(info) << "  Layer name: pwell\n";
        pwell_layer_.Report();
    }
}

}
