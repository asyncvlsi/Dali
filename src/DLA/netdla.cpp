//
// Created by Yihang Yang on 2019-06-01.
//

#include "netdla.hpp"

net_dla_t::net_dla_t(): net_t() {

}

net_dla_t::net_dla_t(std::string &name, double weight): net_t(name, weight) {

}

void net_dla_t::retrieve_info_from_database(net_t &net) {
  _name = net.name();
  _weight = net.weight();
  _num = net.num();
}