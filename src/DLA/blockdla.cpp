//
// Created by Yihang Yang on 2019-05-20.
//

#include <algorithm>
#include <cstdlib>
#include "blockdla.hpp"

block_dla_t::block_dla_t() {
  m_num = 0;
  _w = 0;
  _h = 0;
  _orientation = "N";
  _movable = true;
  _placed = false;
  _queued = false;
  x0 = 0;
  y0 = 0;
  vx = 0;
  vy = 0;
}

block_dla_t::block_dla_t(std::string &blockName, int w, int h, int lx, int ly, bool movable):block_t(blockName, w, h, lx, ly, movable) {
  _placed = false;
  _queued = false;
  x0 = 0;
  y0 = 0;
  vx = 0;
  vy = 0;
}

void block_dla_t::retrieve_info_from_database(const block_t &block){
  _name = block.name();
  m_num = block.num();
  _w = block.width();
  _h = block.height();
  _orientation = block.orientation();
  _movable = block.is_movable();
}

void block_dla_t::write_info_to_database(block_t &block) {
  block.set_llx(_llx);
  block.set_lly(_lly);
}

int block_dla_t::total_net() {
  return net.size();
}

void block_dla_t::set_placed(bool placed) {
  _placed = placed;
}

bool block_dla_t::is_placed() {
  return _placed;
}

void block_dla_t::set_queued(bool queued) {
  _queued = queued;
}

bool block_dla_t::is_queued() {
  return _queued;
}

void block_dla_t::add_to_neb_list(block_dla_t *block_dla, double net_weight) {
  bool is_new_block_in_neb = false;
  for (auto &&neb: neb_list) {
    if (block_dla == neb.block) {
      is_new_block_in_neb = true;
      neb.total_wire_weight += net_weight;
      break;
    }
  }
  if (!is_new_block_in_neb) {
    block_neighbor_t block_neighbor(block_dla, net_weight);
    neb_list.push_back(block_neighbor);
  }
}

void block_dla_t::sort_neb_list() {
  int max_wire_node_index;
  block_neighbor_t tmp_neb;
  for (size_t i=0; i<neb_list.size(); i++) {
    max_wire_node_index = i;
    for (size_t j=i+1; j<neb_list.size(); j++) {
      if (neb_list[j].total_wire_weight > neb_list[max_wire_node_index].total_wire_weight) {
        max_wire_node_index = j;
      }
    }
    tmp_neb = neb_list[i];
    neb_list[i] = neb_list[max_wire_node_index];
    neb_list[max_wire_node_index] = tmp_neb;
  }
}

void block_dla_t::add_to_net(net_dla_t *net_dla) {
  bool is_in_net = false;
  for (auto &&net_ptr: net) {
    if (net_ptr == net_dla) {
      is_in_net = true;
    }
  }
  if (!is_in_net) {
    net.push_back(net_dla);
  }
}

bool block_dla_t::is_overlap(const  block_dla_t &rhs) const{
  bool not_overlap;
  not_overlap = llx() >= rhs.urx() || rhs.llx() >= urx() || lly() >= rhs.ury() || rhs.lly() >= ury();
  // If one rectangle is on left side of another or if one rectangle is above another
  return !not_overlap;
}

double block_dla_t::overlap_area(const  block_dla_t &rhs) const{
  if (is_overlap(rhs)) {
    double l1x, l1y, r1x, r1y, l2x, l2y, r2x, r2y;
    l1x = llx();
    l1y = lly();
    r1x = urx();
    r1y = ury();
    l2x = rhs.llx();
    l2y = rhs.lly();
    r2x = rhs.urx();
    r2y = rhs.ury();
    return (std::min(r1x, r2x) - std::max(l1x, l2x)) * (std::min(r1y, r2y) - std::max(l1y, l2y));
  } else {
    return 0;
  }
}

int block_dla_t::wire_length_during_dla() {
  int WL = 0;
  for (auto &&net_ptr: net) {
    WL += net_ptr->hpwl_during_dla();
  }
  return WL;
}

void block_dla_t::random_move(int distance) {
  int rand_num = std::rand();
  if (rand_num % 4 == 0) {
    // leftward move
    _llx -= distance;
  } else if (rand_num % 4 == 1) {
    // rightward move
    _llx += distance;
  } else if (rand_num % 4 == 2) {
    // downward move
    _lly -= distance;
  } else {
    // upward move
    _lly += distance;
  }
}
