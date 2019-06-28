//
// Created by Yihang Yang on 2019-06-27.
//

#ifndef HPCC_CIRCUITBLOCKTYPE_HPP
#define HPCC_CIRCUITBLOCKTYPE_HPP

#include <string>
#include <map>
#include "circuitpin.hpp"

class blocktype_t {
private:
  /****essential data entries****/
  std::string _name;
  int _width, _height;
  std::vector<pin_t> pin_list;
  std::map<std::string, size_t> pinname_num;

  /****cached data entries****/
  int _num;
public:
  blocktype_t();

};


#endif //HPCC_CIRCUITBLOCKTYPE_HPP
