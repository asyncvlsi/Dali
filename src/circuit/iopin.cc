//
// Created by Yihang Yang on 11/28/19.
//

#include "iopin.h"

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair) :
    name_num_pair_(name_num_pair),
    net_(nullptr),
    direction_(INPUT_),
    layer_(nullptr),
    place_status_(UNPLACED),
    lx_(0),
    ly_(0) {}

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair, double lx, double ly) :
    name_num_pair_(name_num_pair),
    net_(nullptr),
    direction_(INPUT_),
    layer_(nullptr),
    place_status_(UNPLACED),
    lx_(lx),
    ly_(ly) {}

IOPin::IOPin(std::pair<const std::string, int> *name_num_pair,
             SignalDirection direction,
             PlaceStatus place_status,
             double lx,
             double ly) :
    name_num_pair_(name_num_pair),
    net_(nullptr),
    direction_(direction),
    layer_(nullptr),
    place_status_(place_status),
    lx_(lx),
    ly_(ly) {}

void IOPin::Report() const {
  std::string net_name = (net_ == nullptr) ? "NA" : *(net_->Name());
  std::cout << "  I/O PIN name: " << *Name() << "\n"
            << "    Net connected: " << net_name << "\n";
}