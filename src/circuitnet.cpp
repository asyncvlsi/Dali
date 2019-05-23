//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitnet.hpp"

net_t::net_t() {
  _name = "";
  _num = 0;
}

net_t::net_t(std::string &name) : _name(name) {}

bool net_t::add_pin(pin_t &pin) {

}