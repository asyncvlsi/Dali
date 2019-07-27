//
// Created by Yihang Yang on 2019-05-23.
//

#include "net.h"

Net::Net(std::string &name, int num, double weight) : name_(name), num_(num), weight_(weight) {}

void Net::set_name(const std::string &name_arg) {
  _name = name_arg;
}

std::string Net::name() {
  return _name;
}

void Net::set_weight(double weight_arg) {
  _weight = weight_arg;
}

void Net::set_num(size_t number) {
  _num = number;
}

size_t Net::num() {
 return _num;
}

double Net::weight() {
  return _weight;
}

bool Net::add_pin(pin_t &pin) {
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

double Net::inv_p() {
  if (pin_list.size() <= 1) {
    std::cout << "Error!\n";
    std::cout << "Invalid net to calculate 1/(p-1)\n";
    std::cout << this << "\n";
    exit(1);
  }
  return 1.0/(pin_list.size() - 1);
}

int Net::p() {
  return (int)pin_list.size();
}

int Net::hpwl() {
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
      std::cout << "attribute Block* _block is nullptr, it should points to the block containing this pin\n";
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

  return (max_x - min_x) + (max_y - min_y);
}

