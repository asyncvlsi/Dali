/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "helper.h"

#include <cmath>

#include "logging.h"
#include "memory.h"

namespace dali {

double AbsResidual(double x, double y) {
  return std::fabs(x - std::round(x / y) * y);
}

void StrTokenize(std::string const &line, std::vector<std::string> &res) {
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

/****
 * this function assumes that the input string is a concatenation of
 * a pure English char string, and a pure digit string
 * like "metal7", "metal12", etc.
 * it will return the location of the first digit
 * ****/
int FindFirstNumber(std::string const &str) {
  int res = -1;
  size_t sz = str.size();
  for (size_t i = 0; i < sz; ++i) {
    if (str[i] >= '0' && str[i] <= '9') {
      res = i;
      break;
    }
  }
  if (res > 0) {
    for (size_t i = res + 1; i < sz; ++i) {
      DaliExpects(str[i] >= '0' && str[i] <= '9',
                  "Invalid naming convention: " + str);
    }
  }
  return res;
}

/**
 * Check if a binary executable exists or not using BSD's "which" command.
 * We will use the property that "which" returns the number of failed arguments
 *
 * @param executable_path, the full path or name of an external executable.
 * @return true if this executable can be found.
 */
bool IsExecutableExisting(std::string const &executable_path) {
  // system call
  std::string command = "which " + executable_path + " >/dev/null";
  int res = std::system(command.c_str());
  return res == 0;
}

void ReportMemory() {
  auto peak_mem = getPeakRSS();
  auto curr_mem = getCurrentRSS();
  BOOST_LOG_TRIVIAL(info)
    << "(peak memory: " << (peak_mem >> 20u) << " MB, "
    << " current memory: " << (curr_mem >> 20u) << " MB)\n";
}

/****
 * This member function comes from a solution I submitted to LeetCode, lol
 *
 * If two intervals overlap with each other, these two intervals will be merged into one
 *
 * This member function can merge a list of intervals
 * ****/
void MergeIntervals(std::vector<SegI> &intervals) {
  size_t sz = intervals.size();
  if (sz <= 1) return;

  std::sort(
      intervals.begin(),
      intervals.end(),
      [](const SegI &inter1,
         const SegI &inter2) {
        return inter1.lo < inter2.lo;
      }
  );

  std::vector<SegI> res;

  int begin = intervals[0].lo;
  int end = intervals[0].hi;

  SegI tmp(2, 0);
  for (size_t i = 1; i < sz; ++i) {
    if (end < intervals[i].lo) {
      tmp.lo = begin;
      tmp.hi = end;
      res.push_back(tmp);
      begin = intervals[i].hi;
    }
    if (end < intervals[i].hi) {
      end = intervals[i].hi;
    }
  }

  tmp.lo = begin;
  tmp.hi = end;
  res.push_back(tmp);

  intervals = res;
}

}
