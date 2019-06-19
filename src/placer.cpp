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
  _circuit = nullptr;
}

placer_t::placer_t(double aspectRatio, double fillingRate) : _aspect_ratio(aspectRatio), _filling_rate(fillingRate) {
  _left = 0;
  _right = 0;
  _bottom = 0;
  _top = 0;
  _circuit = nullptr;
}

bool placer_t::set_filling_rate(double rate) {
  if ((rate > 1)||(rate <= 0)) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range (0, 1]\n";
    return false;
  } else {
    _filling_rate = rate;
  }
  return true;
}

double placer_t::filling_rate() {
  return _filling_rate;
}

double placer_t::aspect_ratio() {
  return _aspect_ratio;
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

double placer_t::space_block_ratio() {
  if (_filling_rate < 1e-3) {
    std::cout << "Warning: filling rate too small, might lead to large numerical error.\n";
  }
  return 1.0/_filling_rate;
}

bool placer_t::set_space_block_ratio(double ratio) {
  if (ratio < 1) {
    std::cout << "Error!\n";
    std::cout << "Invalid value: value should be in range [1, +infinity)\n";
    return false;
  } else {
    _filling_rate = 1./ratio;
  }
  return true;
}

bool placer_t::auto_set_boundaries() {
  if (_circuit == nullptr) {
    std::cout << "Error!\n";
    std::cout << "Member function set_input_circuit must be called before auto_set_boundaries\n";
    return false;
  }
  int tot_block_area = _circuit->tot_block_area();
  int width = std::ceil(std::sqrt(tot_block_area/_aspect_ratio/_filling_rate));
  int height = std::ceil(width * _aspect_ratio);
  std::cout << "Pre-set aspect ratio: " << _aspect_ratio << "\n";
  _aspect_ratio = height/(double)width;
  std::cout << "Adjusted aspect rate: " << _aspect_ratio << "\n";

  _left = (int)_circuit->ave_width();
  _right = _left + width;
  _bottom = (int)_circuit->ave_width();
  _top = _bottom + height;
  int area = height * width;
  std::cout << "Pre-set filling rate: " << _filling_rate << "\n";
  _filling_rate = tot_block_area/(double)area;
  std::cout << "Adjusted filling rate: " << _filling_rate << "\n";
  return true;
}

void placer_t::report_boundaries() {
  std::cout << "\tleft\tright\tbottom\ttop\n";
  std::cout << "\t" << _left << "\t" << _right << "\t" << _bottom << "\t" << _top << "\n";
}

int placer_t::left() {
  return _left;
}

int placer_t::right() {
  return _right;
}

int placer_t::bottom() {
  return _bottom;
}

int placer_t::top() {
  return _top;
}

bool placer_t::update_aspect_ratio() {
  if ((_right - _left == 0) || (_top - _bottom == 0)) {
    std::cout << "Error!\n";
    std::cout << "Zero height or width of placement region!\n";
    report_boundaries();
    return false;
  }
  _aspect_ratio = (_top - _bottom)/(double)(_right - _left);
  return true;
}

bool placer_t::set_boundary(int left_arg, int right_arg, int bottom_arg, int top_arg) {
  if (_circuit == nullptr) {
    std::cout << "Error!\n";
    std::cout << "Member function set_input_circuit must be called before auto_set_boundaries\n";
    return false;
  }

  int tot_block_area = _circuit->tot_block_area();
  int tot_area = (right_arg - left_arg) * (top_arg - bottom_arg);

  if (tot_area <= tot_block_area) {
    std::cout << "Error!\n";
    std::cout << "Given boundaries have smaller area than total block area!\n";
    return false;
  } else {
    std::cout << "Pre-set filling rate: " << _filling_rate << "\n";
    _filling_rate = tot_block_area/(float)tot_area;
    std::cout << "Adjusted filling rate: " << _filling_rate << "\n";
    _left = left_arg;
    _right = right_arg;
    _bottom = bottom_arg;
    _top = top_arg;
    return true;
  }
}

bool placer_t::gen_matlab_disp_file(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: _circuit->block_list) {
    ost << "rectangle('Position',[" << block.llx() << " " << block.lly() << " " << block.width() << " " << block.height() << "], 'LineWidth', 1, 'EdgeColor','blue')\n";
  }
  for (auto &&net: _circuit->net_list) {
    for (size_t i=0; i<net.pin_list.size(); i++) {
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        ost << "line([" << net.pin_list[i].abs_x() << "," << net.pin_list[j].abs_x() << "],[" << net.pin_list[i].abs_y() << "," << net.pin_list[j].abs_y() << "],'lineWidth', 0.5)\n";
      }
    }
  }
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_t::write_pl_solution(std::string const &NameOfFile) {
  std::ofstream ost(NameOfFile.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &&block: _circuit->block_list) {
    if (block.is_movable()) {
      ost << block.name() << "\t" << block.llx() << "\t" << block.lly() << "\t:\tN\n";
    }
    else {
      ost << block.name() << "\t" << block.llx() << "\t" << block.lly() << "\t:\tN\t/FIXED\n";
    }
  }
  ost.close();
  //std::cout << "Output solution file complete\n";
  return true;
}

placer_t::~placer_t() {

}

