//
// Created by Yihang Yang on 2019-06-15.
//

#include "netal.hpp"

net_al_t::net_al_t(): net_t() {

}

explicit net_al_t::net_al_t(std::string &name_arg, double weight_arg): net_t(name_arg, weight_arg) {

}

void net_al_t::retrieve_info_from_database(net_t &net) {
  _name = net.name();
  _weight = net.weight();
  _num = net.num();
}