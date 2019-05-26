//
// Created by Yihang Yang on 2019-05-23.
//

#include <cmath>
#include "placer.hpp"

placer_t::placer_t() {
  _aspect_ratio = 0;
  _filling_rate = 0;
  _left = 0;
  _right = 0;
  _bottom = 0;
  _top = 0;
  _white_space_block_area_ratio = 0;
  _circuit = nullptr;
}

placer_t::placer_t(double aspectRatio, double fillingRate) : _aspect_ratio(aspectRatio), _filling_rate(fillingRate) {
  _left = 0;
  _right = 0;
  _bottom = 0;
  _top = 0;
  _white_space_block_area_ratio = 1/fillingRate;
  _circuit = nullptr;
}

bool placer_t::set_filling_rate(double rate) {
  if ((rate > 1)||(rate <= 0)) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range (0, 1]\n";
    return false;
  } else {
    _filling_rate = rate;
    _white_space_block_area_ratio = 1/_filling_rate;
  }
  return true;
}

bool placer_t::set_aspect_ratio(double ratio){
  if (ratio < 0) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range (0, +infinity)\n";
    return false;
  } else {
    _aspect_ratio = ratio;
  }
  return true;
}

bool placer_t::set_input_circuit(circuit_t *circuit) {
  if (circuit == nullptr) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit, argument cannot be set to nullptr!\n";
    return false;
  }
  _circuit = circuit;
  return true;
}

bool placer_t::auto_set_boundaries() {
  if (_circuit == nullptr) {
    std::cout << "Error!\n";
    std::cout << "Member function set_input_circuit must be called before auto_set_boundaries\n";
    return false;
  }
  int tot_block_area = 0;
  int area;
  for (auto &&node: _circuit->block_list) {
    tot_block_area += node.area();
  }
  int width = std::ceil(std::sqrt(tot_block_area/_aspect_ratio/_filling_rate));
  int height = std::ceil(width * _aspect_ratio);
  std::cout << "Pre-set aspect ratio: " << _aspect_ratio << "\n";
  _aspect_ratio = height/(double)width;
  std::cout << "Adjusted filling rate: " << _aspect_ratio << "\n";

  _left = (int)_circuit->ave_width;
  _right = _left + width;
  _bottom = (int)_circuit->ave_width;
  _top = _bottom + width;
  area = width * width;
  std::cout << "Pre-set filling rate: " << _filling_rate << "\n";
  _filling_rate = tot_block_area/(double)area;
  std::cout << "Adjusted filling rate: " << _filling_rate << "\n";
  return true;
}

bool placer_t::set_boundary(int left, int right, int bottom, int top) {
  int tot_block_area = 0;
  int area;
  for (auto &&node: block_list) {
    tot_block_area += node.area();
  }

  if ((left == 0)&&(right == 0)&&(bottom == 0)&&(top == 0)) {
    // default boundary setting, a square
    int width = std::ceil(std::sqrt(tot_block_area/TARGET_FILLING_RATE));
    LEFT = (int)ave_width;
    RIGHT = LEFT + width;
    BOTTOM = (int)ave_width;
    TOP = BOTTOM + width;
    area = width * width;
    std::cout << "Pre-set filling rate: " << TARGET_FILLING_RATE << "\n";
    TARGET_FILLING_RATE = tot_block_area/(float)area;
    std::cout << "Adjusted filling rate: " << TARGET_FILLING_RATE << "\n";
  } else {
    area = (right - left) * (top - bottom);
    // check if area is large enough and update
    if (area <= tot_block_area) {
      std::cout << "Error: defined boundaries have smaller area than total cell area\n";
      return false;
    } else {
      TARGET_FILLING_RATE = tot_block_area/(float)area;
    }
  }

  return true;
}

