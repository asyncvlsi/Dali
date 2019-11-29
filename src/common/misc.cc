//
// Created by Yihang Yang on 2019-07-25.
//

#include "misc.h"

void Assert(bool e, const std::string &error_message) {
  if (!e) {
    std::cout << "FATAL ERROR:" << std::endl;
    std::cout << "    " << error_message << std::endl;
    assert(e);
  }
}

void Warning(bool e, const std::string &warning_message) {
  if (globalVerboseLevel >= LOG_WARNING) {
    if (e) {
      std::cout << "WARNING:" << std::endl;
      std::cout << "    " << warning_message << std::endl;
    }
  }
}

void StrSplit(std::string &line, std::vector<std::string> &res) {
  static std::vector<char> delimiter_list {' ', ':', ';', '\t', '\r', '\n'};

  res.clear();
  std::string empty_str;
  bool is_delimiter, old_is_delimiter = true;
  int current_field = -1;
  for (auto &&c: line) {
    is_delimiter = false;
    for (auto &&delimiter: delimiter_list) {
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

int FindFirstDigit(std::string &str) {
  
}