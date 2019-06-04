//
// Created by Yihang Yang on 2019-06-01.
//

#include "netdla.hpp"

net_dla_t::net_dla_t(): net_t() {

}

net_dla_t::net_dla_t(std::string &name, double weight): net_t(name, weight) {

}

void net_dla_t::retrieve_info_from_database(net_t &net) {
  m_name = net.name();
  m_weight = net.weight();
  m_num = net.num();
}

int net_dla_t::hpwl_during_dla() {
  int first_placed_block_index = 0;
  bool is_block_in_net_placed = false;
  block_dla_t *blockDla;
  for (size_t i=0; i < pin_list.size(); i++) {
    blockDla = (block_dla_t *)pin_list[i].get_block();
    if (blockDla->is_placed()) {
      first_placed_block_index = i;
      is_block_in_net_placed = true;
    }
  }
  if (!is_block_in_net_placed) {
    return 0;
  }

  int max_x = pin_list[first_placed_block_index].abs_x();
  int min_x = pin_list[first_placed_block_index].abs_x();
  int max_y = pin_list[first_placed_block_index].abs_y();
  int min_y = pin_list[first_placed_block_index].abs_y();

  for (auto &&pin: pin_list) {
    blockDla = (block_dla_t *)pin.get_block();
    if (blockDla->is_placed()) {
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
    } else {
      continue;
    }
  }

  return (max_x - min_x) + (max_y - min_y);
}
