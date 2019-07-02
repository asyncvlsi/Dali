//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_BLOCK_HPP
#define HPCC_BLOCK_HPP


#include <vector>
#include <string>
#include <iostream>
#include "blocktype.hpp"

/* a block can be a gate, can also be a large module, it includes information like
 * the name of a gate/module, its width and height, its lower left corner (llx, lly),
 * the movability, orientation. */

enum orient_t{N, S, W, E, FN, FS, FW, FE};

class block_t {
protected:
  /* essential data entries */
  block_type_t *_type;
  std::string _name; // name
  int _llx, _lly; // lower left corner
  bool _movable; // movable
  enum orient_t _orient; // currently not used

  /* the following entries are derived data */
  size_t _num;
  /* block_num is the index of this block in the vector block_list, this data must be updated after push a new block into block_list */

public:
  block_t(block_type_t *type, std::string name, int llx, int lly, bool movable="true", orient_t orient=N);

  void set_name(std::string blockName);
  std::string name() const;
  int width() const;
  int height() const;
  void set_llx(int lower_left_x);
  int llx() const;
  void set_lly(int lower_left_y);
  int lly() const;
  void set_urx(int upper_right_x);
  int urx() const;
  void set_ury(int upper_right_y);
  int ury() const;
  void set_center_x(double center_x);
  double x() const;
  void set_center_y(double center_y);
  double y() const;
  void set_movable(bool movable);
  bool is_movable() const;
  int area() const;
  void set_orientation(orient_t &orient);
  orient_t orientation() const;
  std::string orient_str() const;
  void set_num(size_t &number);
  size_t num() const;

  friend std::ostream& operator<<(std::ostream& os, const block_t &block) {
    os << "block name: " << block._name << "\n";
    os << "width and height: " << block.width() << " " << block.height() << "\n";
    os << "lower left corner: " << block._llx << " " << block._lly << "\n";
    os << "movability: " << block._movable << "\n";
    os << "orientation: " << block.orient_str() << "\n";
    os << "assigned primary key: " << block._num << "\n";
    return os;
  }

  std::string type_name();
  std::string place_status();
  std::string lower_left_corner();
};


#endif //HPCC_BLOCK_HPP
