//
// Created by Yihang Yang on 2019-06-15.
//

#ifndef HPCC_BLOCKAL_HPP
#define HPCC_BLOCKAL_HPP

#include "circuit/circuitblock.hpp"

class block_al_t:block_t {
private:
  double _dllx, _dlly; // lower left corner of type double

public:
  block_al_t();
  block_al_t(std::string &blockName, int w, int h, int lx = 0, int ly = 0, bool movable = true);
  void retrieve_info_from_database(const block_t &node_info);
  void write_info_to_database(block_t &node_info);

  void set_dllx(double lower_left_x);
  double dllx() const;
  void set_dlly(double lower_left_y);
  double dlly() const;
  void set_durx(double upper_right_x);
  double durx() const;
  void set_dury(double upper_right_y);
  double dury() const;
  void set_center_dx(double center_x);
  double dx() const;
  void set_center_dy(double center_y);
  double dy() const;


  friend std::ostream& operator<<(std::ostream& os, const block_al_t &block) {
    os << "block name: " << block._name << "\n";
    os << "width and height: " << block._w << " " << block._h << "\n";
    os << "lower left corner: " << block._llx << " " << block._lly << "\n";
    os << "movability: " << block._movable << "\n";
    os << "orientation: " << block._orientation << "\n";
    os << "assigned primary key: " << block._num << "\n";
    return os;
  }
};


#endif //HPCC_BLOCKAL_HPP
