//
// Created by Yihang Yang on 2019-06-27.
//

#ifndef HPCC_BLOCKTYPE_HPP
#define HPCC_BLOCKTYPE_HPP

#include <string>
#include <map>
#include "circuitpin.hpp"

struct point {
  int x;
  int y;
  explicit point(int x0=0, int y0=0): x(x0), y(y0){}
};

class block_type_t {
private:
  /****essential data entries****/
  std::string _name;
  int _width, _height;

  /****cached data entries****/
  int _num;
public:
  block_type_t();
  block_type_t(std::string &init_name, int init_width, int init_height);
  /****essential data entries****/
  std::vector<point> pin_list;
  std::map<std::string, size_t> pinname_num_map;
  /********/

  void set_name(std::string &typeName);
  std::string name();
  void set_width(int w);
  int width();
  void set_height(int h);
  int height();
  void set_num(size_t Num);
  int num();

  friend std::ostream& operator<<(std::ostream& os, const block_type_t &blockType) {
    os << "block type name: " << blockType._name << "\n";
    os << "width and height: " << blockType._width << " " << blockType._height << "\n";
    os << "assigned primary key: " << blockType._num << "\n";
    os << "pin list:\n";
    for( auto &&it: blockType.pinname_num_map) {
      os << "  " << it.first << " " << it.second << "  " << blockType.pin_list[it.second].x << " " << blockType.pin_list[it.second].y << "\n";
    }
    return os;
  }

  bool add_pin(std::string &pinName, double xOffset, double yOffset);

};


#endif //HPCC_BLOCKTYPE_HPP
