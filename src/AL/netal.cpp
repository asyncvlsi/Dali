//
// Created by Yihang Yang on 2019-06-15.
//

#include "netal.hpp"
#include "blockal.hpp"

net_al_t::net_al_t(): net_t() {

}

net_al_t::net_al_t(std::string &name_arg, double weight_arg): net_t(name_arg, weight_arg) {

}

void net_al_t::retrieve_info_from_database(net_t &net) {
  _name = net.name();
  _weight = net.weight();
  _num = net.num();
}

double net_al_t::dhpwl() {
  if (pin_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "net contains no pin\n";
    assert(!pin_list.empty());
  }
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double max_x = block->dllx() + pin_list[0].x_offset();
  double min_x = block->dllx() + pin_list[0].x_offset();
  double max_y = block->dlly() + pin_list[0].y_offset();
  double min_y = block->dlly() + pin_list[0].y_offset();

  for (auto &&pin: pin_list) {
    if (pin.get_block() == nullptr) {
      std::cout << "Error!\n";
      std::cout << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.get_block() != nullptr);
    }
    block = (block_al_t *)(pin_list[0].get_block());
    if (max_x < block->dllx() + pin_list[0].x_offset()) {
      max_x = block->dllx() + pin_list[0].x_offset();
    }
    if (min_x > block->dllx() + pin_list[0].x_offset()) {
      min_x = block->dllx() + pin_list[0].x_offset();
    }
    if (max_y < block->dlly() + pin_list[0].y_offset()) {
      max_y = block->dlly() + pin_list[0].y_offset();
    }
    if (min_y > block->dlly() + pin_list[0].y_offset()) {
      min_y = block->dlly() + pin_list[0].y_offset();
    }
  }

  return (max_x - min_x) + (max_y - min_y);
}