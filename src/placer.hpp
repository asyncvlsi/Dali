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
protected:
  /* essential data entries */
  double m_aspect_ratio; // placement region height/width
  double m_filling_rate;

  /* the following entries are derived data
   * note that the following entries can be manually changed
   * if so, the m_aspect_ratio or m_filling_rate might also be changed */
  int m_left, m_right, m_bottom, m_top;
  // boundaries of the placement region
  circuit_t* m_circuit;

public:
  placer_t();
  placer_t(double aspectRatio, double fillingRate);

  bool set_filling_rate(double rate = 2.0/3.0);
  double filling_rate();
  bool set_aspect_ratio(double ratio = 1.0);
  double aspect_ratio();

  double space_block_ratio();
  bool set_space_block_ratio(double ratio);
  // the ratio of total_white_space/total_block_area

  virtual bool set_input_circuit(circuit_t *circuit) = 0;
  std::vector<net_t>* net_list();
  std::vector<block_t>* block_list();

  bool auto_set_boundaries();
  void report_boundaries();
  int left();
  int right();
  int bottom();
  int top();
  bool update_aspect_ratio();
  bool set_boundary(int left=0, int right=0, int bottom=0, int top=0);

  virtual bool start_placement() = 0;

  bool write_pl_solution(std::string const &NameOfFile);
  bool write_pl_anchor_solution(std::string const &NameOfFile);
  bool write_node_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
  bool write_anchor_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");

  virtual ~placer_t();
};


#endif //HPCC_PLACER_HPP
