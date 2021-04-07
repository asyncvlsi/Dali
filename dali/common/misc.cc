//
// Created by Yihang Yang on 4/6/21.
//

#include "misc.h"

#include "logging.h"

namespace dali {

void StrSplit(std::string &line, std::vector<std::string> &res) {
  static std::vector<char> delimiter_list{' ', ':', ';', '\t', '\r', '\n'};

  res.clear();
  std::string empty_str;
  bool is_delimiter, old_is_delimiter = true;
  int current_field = -1;
  for (auto &c: line) {
    is_delimiter = false;
    for (auto &delimiter: delimiter_list) {
      if (c == delimiter) {
        is_delimiter = true;
        break;
      }
    }
    if (is_delimiter) {
      old_is_delimiter = is_delimiter;
      continue;
    } else {
      if (old_is_delimiter) {
        current_field++;
        res.push_back(empty_str);
      }
      res[current_field] += c;
      old_is_delimiter = is_delimiter;
    }
  }
}

int FindFirstNumber(std::string &str) {
  /****
   * this function assumes that the input string is a concatenation of
   * a pure English char string, and a pure digit string
   * like "metal7", "metal12", etc.
   * it will return the location of the first digit
   * ****/

  int res = -1;
  int sz = str.size();
  for (int i = 0; i < sz; ++i) {
    if (str[i] >= '0' && str[i] <= '9') {
      res = i;
      break;
    }
  }
  if (res > 0) {
    for (int i = res + 1; i < sz; ++i) {
      DaliExpects(str[i] >= '0' && str[i] <= '9', "Invalid naming convention: " + str);
    }
  }
  return res;
}

}
