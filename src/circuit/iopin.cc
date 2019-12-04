//
// Created by Yihang Yang on 11/28/19.
//

#include "iopin.h"

IOPin::IOPin(std::pair<const std::string, int>* name_num_pair): name_num_pair_(name_num_pair) {}

IOPin::IOPin(std::pair<const std::string, int>* name_num_pair, int lx, int ly): name_num_pair_(name_num_pair), lx_(lx), ly_(ly) {}
