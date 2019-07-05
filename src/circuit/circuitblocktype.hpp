//
// Created by Yihang Yang on 2019-06-27.
//

#ifndef HPCC_BLOCKTYPE_HPP
#define HPCC_BLOCKTYPE_HPP

#include <string>
#include <map>
#include "circuitpin.hpp"

struct point {
  double x;
  double y;
  explicit point(double x0=0, double y0=0): x(x0), y(y0){}
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
  block_type_t(std::string &name, int width, int height);
  /****essential data entries****/
  std::vector<point> pin_list;
  std::map<std::string, int> pinname_num_map;
  /********/

  void set_name(std::string &typeName);
  std::string name();
  void set_width(int w);
  int width();
  void set_height(int h);
  int height();
  int num();

  friend std::ostream& operator<<(std::ostream& os, const block_type_t &blockType) {
    os << "block type name: " << blockType._name << "\n";
    os << "width and height: " << blockType._width << " " << blockType._height << "\n";
    os << "assigned primary key: " << blockType._num << "\n";
    return os;
  }

  bool add_pin(std::string &pinName, int xoff, int yoff);

};


#endif //HPCC_BLOCKTYPE_HPP
