//
// Created by Yihang Yang on 2019-05-23.
//

#include <cmath>
#include "placer.hpp"

placer_t::placer_t() {
  m_aspect_ratio = 0;
  m_filling_rate = 0;
  m_left = 0;
  m_right = 0;
  m_bottom = 0;
  m_top = 0;
  m_circuit = nullptr;
}

placer_t::placer_t(double aspectRatio, double fillingRate) : m_aspect_ratio(aspectRatio), m_filling_rate(fillingRate) {
  m_left = 0;
  m_right = 0;
  m_bottom = 0;
  m_top = 0;
  m_circuit = nullptr;
}

bool placer_t::set_filling_rate(double rate) {
  if ((rate > 1)||(rate <= 0)) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range (0, 1]\n";
    return false;
  } else {
    m_filling_rate = rate;
  }
  return true;
}

double placer_t::filling_rate() {
  return m_filling_rate;
}

double placer_t::aspect_ratio() {
  return m_aspect_ratio;
}

bool placer_t::set_aspect_ratio(double ratio){
  if (ratio < 0) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range (0, +infinity)\n";
    return false;
  } else {
    m_aspect_ratio = ratio;
  }
  return true;
}

double placer_t::space_block_ratio() {
  if (m_filling_rate < 1e-3) {
    std::cout << "Warning: filling rate too small, might lead to large numerical error.\n";
  }
  return 1.0/m_filling_rate;
}

bool placer_t::set_space_block_ratio(double ratio) {
  if (ratio < 1) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range [1, +infinity)\n";
    return false;
  } else {
    m_filling_rate = 1./ratio;
  }
  return true;
}

std::vector<net_t>* placer_t::net_list() {
  return &m_circuit->net_list;
}

std::vector<block_t>* placer_t::block_list() {
  return &m_circuit->block_list;
}

bool placer_t::auto_set_boundaries() {
  if (m_circuit == nullptr) {
    std::cout << "Error!\n";
    std::cout << "Member function set_input_circuit must be called before auto_set_boundaries\n";
    return false;
  }
  int tot_block_area = m_circuit->tot_block_area();
  int width = std::ceil(std::sqrt(tot_block_area/m_aspect_ratio/m_filling_rate));
  int height = std::ceil(width * m_aspect_ratio);
  std::cout << "Pre-set aspect ratio: " << m_aspect_ratio << "\n";
  m_aspect_ratio = height/(double)width;
  std::cout << "Adjusted aspect rate: " << m_aspect_ratio << "\n";

  m_left = (int)m_circuit->ave_width();
  m_right = m_left + width;
  m_bottom = (int)m_circuit->ave_width();
  m_top = m_bottom + height;
  int area = height * width;
  std::cout << "Pre-set filling rate: " << m_filling_rate << "\n";
  m_filling_rate = tot_block_area/(double)area;
  std::cout << "Adjusted filling rate: " << m_filling_rate << "\n";
  return true;
}

void placer_t::report_boundaries() {
  std::cout << "\tleft\tright\tbottom\ttop\n";
  std::cout << "\t" << m_left << "\t" << m_right << "\t" << m_bottom << "\t" << m_top << "\n";
}

int placer_t::left() {
  return m_left;
}

int placer_t::right() {
  return m_right;
}

int placer_t::bottom() {
  return m_bottom;
}

int placer_t::top() {
  return m_top;
}

bool placer_t::update_aspect_ratio() {
  if ((m_right - m_left == 0) || (m_top - m_bottom == 0)) {
    std::cout << "Error!\n";
    std::cout << "Zero height or width of placement region!\n";
    report_boundaries();
    return false;
  }
  m_aspect_ratio = (m_top - m_bottom)/(double)(m_right - m_left);
  return true;
}

bool placer_t::set_boundary(int left, int right, int bottom, int top) {
  if (m_circuit == nullptr) {
    std::cout << "Error!\n";
    std::cout << "Member function set_input_circuit must be called before auto_set_boundaries\n";
    return false;
  }

  int tot_block_area = m_circuit->tot_block_area();
  int tot_area = (right - left) * (top - bottom);

  if (tot_area <= tot_block_area) {
    std::cout << "Error!\n";
    std::cout << "Given boundaries have smaller area than total block area!\n";
    return false;
  } else {
    std::cout << "Pre-set filling rate: " << m_filling_rate << "\n";
    m_filling_rate = tot_block_area/(float)tot_area;
    std::cout << "Adjusted filling rate: " << m_filling_rate << "\n";
    m_left = left;
    m_right = right;
    m_bottom = bottom;
    m_top = top;
    return true;
  }
}

placer_t::~placer_t() {

}

