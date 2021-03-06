//
// Created by Yihang Yang on 2019-06-15.
//

#ifndef DALI_BLOCKAL_HPP
#define DALI_BLOCKAL_HPP

#include <cmath>
#include "circuit/block.h"
#include "placer/detailedPlacer/MDPlacer/bin.h"

class block_al_t:public block_t {
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
  void x_increment(double delta_x);
  void y_increment(double delta_y);

  std::vector< bin_index > bin; // bins this cell is in
  bool is_overlap(const block_al_t &rhs) const;
  double overlap_area(const  block_al_t &rhs) const;

  double vx, vy;
  void modif_vx();
  void modif_vy();
  void add_gravity_vx(double gravity_x);
  void add_gravity_vy(double gravity_y);
  void update_loc(int time_step);


  friend std::ostream& operator<<(std::ostream& os, const block_al_t &block) {
    os << "block Name: " << block._name << "\n";
    os << "Width and Height: " << block._w << " " << block._h << "\n";
    os << "lower left corner: " << block._dllx << " " << block._dlly << "\n";
    os << "movability: " << block._movable << "\n";
    os << "orientation: " << block._orientation << "\n";
    os << "assigned primary key: " << block._num << "\n";
    return os;
  }
};


#endif //DALI_BLOCKAL_HPP
