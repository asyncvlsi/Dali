//
// Created by Yihang Yang on 2019-06-27.
//

#ifndef HPCC_BLOCKTYPE_HPP
#define HPCC_BLOCKTYPE_HPP

#include <string>
#include <map>
#include "circuitpin.hpp"

class block_type_t {
private:
  /****essential data entries****/
  std::string _name;
  int _width, _height;
  std::vector<pin_t> pin_list;
  std::map<std::string, size_t> pinname_num;

  /****cached data entries****/
  int _num;
public:
  block_type_t(std::string &name, int width, int height);

  std::string name();
  int num();
  int width();
  int height();

};


#endif //HPCC_BLOCKTYPE_HPP
