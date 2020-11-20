//
// Created by Yihang Yang on 11/28/19.
//

#include "iopin.h"

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair_ptr) :
    name_num_pair_ptr_(name_num_pair_ptr),
    net_ptr_(nullptr),
    direction_(INPUT_),
    layer_ptr_(nullptr),
    init_place_status_(UNPLACED_),
    place_status_(UNPLACED_),
    lx_(0),
    ly_(0) {}

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair_ptr, double lx, double ly) :
    name_num_pair_ptr_(name_num_pair_ptr),
    net_ptr_(nullptr),
    direction_(INPUT_),
    layer_ptr_(nullptr),
    init_place_status_(PLACED_),
    place_status_(PLACED_),
    lx_(lx),
    ly_(ly) {}

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair_ptr,
             SignalDirection direction,
             PlaceStatus init_place_status,
             double lx,
             double ly) :
    name_num_pair_ptr_(name_num_pair_ptr),
    net_ptr_(nullptr),
    direction_(direction),
    layer_ptr_(nullptr),
    init_place_status_(init_place_status),
    place_status_(init_place_status),
    lx_(lx),
    ly_(ly) {}

void IOPin::Report() const {
  std::string net_name = (net_ptr_ == nullptr) ? "NA" : *(net_ptr_->Name());
  BOOST_LOG_TRIVIAL(info)   << "  I/O PIN name: " << *Name() << "\n"
            << "    Net connected: " << net_name << "\n";
}