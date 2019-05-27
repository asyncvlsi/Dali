//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitnet.hpp"

net_t::net_t() {
  _name = "";
  _weight = 1;
  _num = 0;
}

net_t::net_t(std::string &name, double weight) : _name(name), _weight(weight) {}

void net_t::set_name(std::string &name) {
  _name = name;
}

std::string net_t::name() {
  return _name;
}

void net_t::set_weight(double weight) {
  _weight = weight;
}

void net_t::set_num(size_t num) {
  _num = num;
}

size_t net_t::num() {
 return _num;
}

double net_t::weight() {
  return _weight;
}

bool net_t::add_pin(pin_t &pin) {
  for (auto &&existing_pin: pin_list) {
    if (existing_pin == pin) {
      std::cout << "Error!\n";
      std::cout << pin << " has already been in net: " << _name << "\n";
      return false;
    }
  }
  pin_list.push_back(pin);
  return true;
}

int net_t::hpwl() {
  if (pin_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "net contains no pin\n";
    assert(!pin_list.empty());
  }
  int max_x = pin_list[0].abs_x();
  int min_x = pin_list[0].abs_x();
  int max_y = pin_list[0].abs_y();
  int min_y = pin_list[0].abs_y();

  for (auto &&pin: pin_list) {
    if (pin.get_block() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.get_block() != nullptr);
    }
    if (max_x < pin.abs_x()) {
      max_x = pin.abs_x();
    }
    if (min_x > pin.abs_x()) {
      min_x = pin.abs_x();
    }
    if (max_y < pin.abs_y()) {
      max_y = pin.abs_y();
    }
    if (min_y > pin.abs_y()) {
      min_y = pin.abs_y();
    }
  }
}