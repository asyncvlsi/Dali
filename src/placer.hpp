//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_PLACER_HPP
#define HPCC_PLACER_HPP

#include <string>
#include <iostream>
#include <fstream>
#include "circuit.hpp"

class placer_t {
private:
  /* essential data entries */
  double _aspect_ratio; // placement region height/width
  double _filling_rate;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the _aspect_ratio or _filling_rate might also be changed */
  int _left, _right, _bottom, _top;
  // boundaries of the placement region
  double _white_space_block_area_ratio;
  // the ratio of total_white_space/total_block_area
  circuit_t* _circuit;

public:
  placer_t();

  placer_t(double aspectRatio, double fillingRate);

  bool set_filling_rate(double rate = 2.0/3.0);
  bool set_aspect_ratio(double ratio = 1.0);

  bool set_input_circuit(circuit_t *circuit);
  bool auto_set_boundaries();
  bool set_boundary(int left=0, int right=0, int bottom=0, int top=0);
  void update_aspect_ratio();

  bool write_pl_solution(std::string const &NameOfFile);
  bool write_pl_anchor_solution(std::string const &NameOfFile);
  bool write_node_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
  bool write_anchor_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
};


#endif //HPCC_PLACER_HPP
