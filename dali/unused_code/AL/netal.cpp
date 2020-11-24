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

double net_al_t::dhpwlx() {
  if (pin_list.empty()) {
    BOOST_LOG_TRIVIAL(info)   << "Error!\n";
    BOOST_LOG_TRIVIAL(info)   << "net contains no pin\n";
    assert(!pin_list.empty());
  }
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double max_x = block->dllx() + pin_list[0].x_offset();
  double min_x = block->dllx() + pin_list[0].x_offset();

  for (auto &pin: pin_list) {
    if (pin.get_block() == nullptr) {
      BOOST_LOG_TRIVIAL(info)   << "Error!\n";
      BOOST_LOG_TRIVIAL(info)   << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.get_block() != nullptr);
    }
    block = (block_al_t *)(pin.get_block());
    if (max_x < block->dllx() + pin.x_offset()) {
      max_x = block->dllx() + pin.x_offset();
    }
    if (min_x > block->dllx() + pin.x_offset()) {
      min_x = block->dllx() + pin.x_offset();
    }
  }

  return (max_x - min_x);
}

double net_al_t::dhpwly() {
  if (pin_list.empty()) {
    BOOST_LOG_TRIVIAL(info)   << "Error!\n";
    BOOST_LOG_TRIVIAL(info)   << "net contains no pin\n";
    assert(!pin_list.empty());
  }
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double max_y = block->dlly() + pin_list[0].y_offset();
  double min_y = block->dlly() + pin_list[0].y_offset();

  for (auto &pin: pin_list) {
    if (pin.get_block() == nullptr) {
      BOOST_LOG_TRIVIAL(info)   << "Error!\n";
      BOOST_LOG_TRIVIAL(info)   << "attribute block_t* _block is nullptr, it should points to the block containing this pin\n";
      assert(pin.get_block() != nullptr);
    }
    block = (block_al_t *)(pin.get_block());
    if (max_y < block->dlly() + pin.y_offset()) {
      max_y = block->dlly() + pin.y_offset();
    }
    if (min_y > block->dlly() + pin.y_offset()) {
      min_y = block->dlly() + pin.y_offset();
    }
  }

  return (max_y - min_y);
}

size_t net_al_t::max_pin_index_x() {
  size_t max_pin_index = 0;
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double max_x = block->dllx() + pin_list[0].x_offset();
  for (size_t i=0; i<pin_list.size(); i++) {
    block = (block_al_t *)(pin_list[i].get_block());
    if (max_x < block->dllx() + pin_list[i].x_offset()) {
      max_x = block->dllx() + pin_list[i].x_offset();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

size_t net_al_t::min_pin_index_x() {
  size_t min_pin_index = 0;
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double min_x = block->dllx() + pin_list[0].x_offset();
  for (size_t i=0; i<pin_list.size(); i++) {
    block = (block_al_t *)(pin_list[i].get_block());
    if (min_x > block->dllx() + pin_list[i].x_offset()) {
      min_x = block->dllx() + pin_list[i].x_offset();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}

size_t net_al_t::max_pin_index_y() {
  size_t max_pin_index = 0;
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double max_y = block->dlly() + pin_list[0].y_offset();
  for (size_t i=0; i<pin_list.size(); i++) {
    block = (block_al_t *)(pin_list[i].get_block());
    if (max_y < block->dlly() + pin_list[i].y_offset()) {
      max_y = block->dlly() + pin_list[i].y_offset();
      max_pin_index = i;
    }
  }
  return max_pin_index;
}

size_t net_al_t::min_pin_index_y() {
  size_t min_pin_index = 0;
  auto *block = (block_al_t *)(pin_list[0].get_block());
  double min_y = block->dlly() + pin_list[0].y_offset();
  for (size_t i=0; i<pin_list.size(); i++) {
    block = (block_al_t *)(pin_list[i].get_block());
    if (min_y < block->dlly() + pin_list[i].y_offset()) {
      min_y = block->dlly() + pin_list[i].y_offset();
      min_pin_index = i;
    }
  }
  return min_pin_index;
}