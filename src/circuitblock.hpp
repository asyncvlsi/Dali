//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_CIRCUITBLOCK_HPP
#define HPCC_CIRCUITBLOCK_HPP


#include <vector>
#include <string>
#include <iostream>

/* a block can be a gate, can also be a large module, it includes information like
 * the name of a gate/module, its width and height, its lower left corner (llx, lly),
 * the movability, orientation. */

class block_t {
private:
  /* essential data entries */
  std::string _name; // name
  int _w, _h; // width and height
  int _llx, _lly; // lower left corner
  bool _movable; // movable
  std::string _orientation; // currently not used

  /* the following entries are derived data */
  size_t _num;
  /* block_num is the index of this block in the vector block_list, this data must be updated after push a new block into block_list */

public:
  block_t();
  block_t(std::string &blockName, int w, int h, int llx, int lly, bool movable);

  void set_name(std::string &blockName);
  std::string name();
  void set_width(int width);
  int width();
  void set_height(int height);
  int height();
  void set_llx(int lower_left_x);
  int llx();
  void set_lly(int lower_left_y);
  int lly();
  void set_movable(bool movable);
  bool is_movable();
  int area();
  void set_orientation(std::string orientation);
  std::string orientation();
  void set_num(size_t num);
  size_t num();

  friend std::ostream& operator<<(std::ostream& os, const block_t &block) {
    os << "block name: " << block._name << "\n";
    os << "width and height: " << block._w << " " << block._h << "\n";
    os << "lower left corner: " << block._llx << " " << block._lly << "\n";
    os << "movability: " << block._movable << "\n";
    os << "orientation: " << block._orientation << "\n";
    os << "assigned primary key: " << block._num << "\n";

    return os;
  }
};


#endif //HPCC_CIRCUITBLOCK_HPP
