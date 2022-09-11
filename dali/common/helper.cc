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
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "dali/common/logging.h"
#include "dali/common/memory.h"

namespace dali {

void SaveArgs(int32_t argc, char *argv[]) {
  std::string cmd_line_arguments;
  for (int32_t i = 0; i < argc; ++i) {
    cmd_line_arguments += argv[i];
    cmd_line_arguments.push_back(' ');
  }
  BOOST_LOG_TRIVIAL(info) << "Command:\n";
  BOOST_LOG_TRIVIAL(info) << cmd_line_arguments << "\n" << std::endl;
}

std::vector<std::vector<std::string>> ParseArguments(
    int32_t argc,
    char *argv[],
    std::string const &flag_prefix
) {
  std::vector<std::vector<std::string>> options;
  for (int32_t i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg.substr(0, flag_prefix.size())== flag_prefix) {
      // this is a new flag
      options.emplace_back();
      options.back().emplace_back(arg);
    } else if (!options.empty()) { // this is an option for the latest flag
      options.back().emplace_back(arg);
    } else { // option with no flag declared in advance?
      DaliExpects(false, "there is no flag declared before: " << arg);
    }
  }
  return options;
}

double AbsResidual(double x, double y) {
  return std::fabs(x - std::round(x / y) * y);
}

class SegmentTree {
 public:
  int32_t start, end;
  std::vector<int32_t> X;
  SegmentTree *left;
  SegmentTree *right;
  int32_t count;
  long long total;

  SegmentTree(int32_t start0, int32_t end0, std::vector<int32_t> &X0) :
      start(start0),
      end(end0),
      X(X0) {
    left = nullptr;
    right = nullptr;
    count = 0;
    total = 0;
  }

  int32_t GetRangeMid() const {
    return start + (end - start) / 2;
  }

  SegmentTree *GetLeft() {
    if (left == nullptr) {
      left = new SegmentTree(start, GetRangeMid(), X);
    }
    return left;
  }

  SegmentTree *GetRight() {
    if (right == nullptr) {
      right = new SegmentTree(GetRangeMid(), end, X);
    }
    return right;
  }

  long long Update(int32_t i, int32_t j, int32_t val);
};

long long SegmentTree::Update(int32_t i, int32_t j, int32_t val) {
  if (i >= j) return 0;
  if (start == i && end == j) {
    count += val;
  } else {
    GetLeft()->Update(i, std::min(GetRangeMid(), j), val);
    GetRight()->Update(std::max(GetRangeMid(), i), j, val);
  }

  if (count > 0) total = X[end] - X[start];
  else total = GetLeft()->total + GetRight()->total;

  return total;
}

unsigned long long GetCoverArea(std::vector<RectI> &rects) {
  // no rectangles, return 0 immediately
  if (rects.empty()) return 0;

  int32_t event_open = 1, event_close = -1;
  // event is a tuple containing {y_loc, open/close event, lower_x, upper_x}
  std::vector<std::vector<int32_t>> events;
  // each rectangle can create at most 2 events if area is positive, otherwise 0 event
  events.reserve(rects.size() * 2);
  std::unordered_set<int32_t> x_values_set;

  // create an event only when a rectangle has an area larger than 0
  for (auto &rec : rects) {
    if (!rec.CheckValidity()) {
      DaliExpects(false, "Something wrong with this rectangle? " << rec);
    }
    if ((rec.LLX() == rec.URX()) || (rec.LLY() == rec.URY())) continue;
    events.push_back({rec.LLY(), event_open, rec.LLX(), rec.URX()});
    events.push_back({rec.URY(), event_close, rec.LLX(), rec.URX()});
    x_values_set.insert(rec.LLX());
    x_values_set.insert(rec.URX());
  }

  // if there is no events, return 0 earlier
  if (events.empty()) return 0;

  // sort all events based on y location of an event
  std::sort(
      events.begin(),
      events.end(),
      [](std::vector<int32_t> const &event0, std::vector<int32_t> const &event1) {
        return event0[0] < event1[0];
      }
  );

  // build a map to find index from x value
  std::vector<int32_t> x_values;
  int32_t sz = static_cast<int32_t>(x_values_set.size());
  x_values.reserve(sz);
  for (auto &val : x_values_set) {
    x_values.push_back(val);
  }
  std::sort(x_values.begin(), x_values.end());
  std::unordered_map<int32_t, int32_t> x_value_id_map;
  for (int32_t i = 0; i < sz; ++i) {
    x_value_id_map.insert(
        std::unordered_map<int32_t, int32_t>::value_type(x_values[i], i)
    );
  }

  SegmentTree active(0, sz - 1, x_values);
  long long ans = 0;
  long long cur_x_sum = 0;
  int32_t cur_y = events[0][0];
  for (auto &event : events) {
    int32_t y = event[0], type = event[1], x1 = event[2], x2 = event[3];
    ans += cur_x_sum * (y - cur_y);
    cur_x_sum = active.Update(x_value_id_map[x1], x_value_id_map[x2], type);
    cur_y = y;
  }

  return ans;
}

double RoundOrCeiling(double x, double epsilon) {
  if (AbsResidual(x, 1) > epsilon) {
    return std::ceil(x);
  }
  return std::round(x);
}

void StrTokenize(std::string const &line, std::vector<std::string> &res) {
  static std::vector<char> delimiter_list{' ', ':', ';', '\t', '\r', '\n'};

  res.clear();
  std::string empty_str;
  bool is_delimiter, old_is_delimiter = true;
  int32_t current_field = -1;
  for (auto &c : line) {
    is_delimiter = false;
    for (auto &delimiter : delimiter_list) {
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
int32_t FindFirstNumber(std::string const &str) {
  int32_t res = -1;
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
                  "Invalid naming convention: " << str);
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
  int32_t res = std::system(command.c_str());
  return res == 0;
}

void ReportMemory() {
  auto peak_mem = getPeakRSS();
  auto curr_mem = getCurrentRSS();
  BOOST_LOG_TRIVIAL(info)
    << "(peak memory: " << (peak_mem >> 20u) << "MB, "
    << " current memory: " << (curr_mem >> 20u) << "MB)\n";
}

/****
 * This member function comes from a solution I submitted to LeetCode, lol
 *
 * If two intervals overlap with each other, these two intervals will be merged into one
 *
 * This function can merge a list of intervals
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

  int32_t begin = intervals[0].lo;
  int32_t end = intervals[0].hi;
  SegI tmp(0, 0);
  for (size_t i = 1; i < sz; ++i) {
    if (end < intervals[i].lo) {
      tmp.lo = begin;
      tmp.hi = end;
      res.push_back(tmp);
      begin = intervals[i].lo;
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
