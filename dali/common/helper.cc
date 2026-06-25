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

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dali/common/logging.h"
#include "dali/common/memory.h"

namespace dali {

void SaveArgs(int argc, char* argv[]) {
  std::string cmd_line_arguments;
  for (int i = 0; i < argc; ++i) {
    cmd_line_arguments += argv[i];
    cmd_line_arguments.push_back(' ');
  }
  BOOST_LOG_TRIVIAL(info) << "Command:\n";
  BOOST_LOG_TRIVIAL(info) << cmd_line_arguments << "\n" << std::endl;
}

std::vector<std::vector<std::string>> ParseArguments(
    int argc, char* argv[], std::string const& flag_prefix) {
  std::vector<std::vector<std::string>> options;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);
    if (arg.substr(0, flag_prefix.size()) == flag_prefix) {
      // this is a new flag
      options.emplace_back();
      options.back().emplace_back(arg);
    } else if (!options.empty()) {  // this is an option for the latest flag
      options.back().emplace_back(arg);
    } else {  // option with no flag declared in advance?
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
  SegmentTree(int start, int end, std::vector<int> const& x_locations)
      : start_(start), end_(end), x_locations_(x_locations) {}

  long long Update(int i, int j, int val);
  long long Total() const { return total_; }

 private:
  int GetRangeMid() const { return start_ + (end_ - start_) / 2; }

  SegmentTree& GetLeft() {
    if (left_ == nullptr) {
      left_ =
          std::make_unique<SegmentTree>(start_, GetRangeMid(), x_locations_);
    }
    return *left_;
  }

  SegmentTree& GetRight() {
    if (right_ == nullptr) {
      right_ = std::make_unique<SegmentTree>(GetRangeMid(), end_, x_locations_);
    }
    return *right_;
  }

  int start_;
  int end_;
  std::vector<int> const& x_locations_;
  std::unique_ptr<SegmentTree> left_;
  std::unique_ptr<SegmentTree> right_;
  int count_ = 0;
  long long total_ = 0;
};

long long SegmentTree::Update(int i, int j, int val) {
  if (i >= j) return 0;
  if (start_ == i && end_ == j) {
    count_ += val;
  } else {
    GetLeft().Update(i, std::min(GetRangeMid(), j), val);
    GetRight().Update(std::max(GetRangeMid(), i), j, val);
  }

  if (count_ > 0)
    total_ = x_locations_[end_] - x_locations_[start_];
  else
    total_ = GetLeft().Total() + GetRight().Total();

  return total_;
}

unsigned long long GetCoverArea(std::vector<RectI> const& rects) {
  // no rectangles, return 0 immediately
  if (rects.empty()) return 0;

  constexpr int event_open = 1;
  constexpr int event_close = -1;
  // event is a tuple containing {y_loc, open/close event, lower_x, upper_x}
  std::vector<std::vector<int>> events;
  // each rectangle can create at most 2 events if area is positive, otherwise 0
  // event
  events.reserve(rects.size() * 2);
  std::unordered_set<int> x_values_set;

  // create an event only when a rectangle has an area larger than 0
  for (auto const& rec : rects) {
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
  std::sort(events.begin(), events.end(),
            [](std::vector<int> const& event0, std::vector<int> const& event1) {
              return event0[0] < event1[0];
            });

  // build a map to find index from x value
  std::vector<int> x_values;
  int sz = static_cast<int>(x_values_set.size());
  x_values.reserve(sz);
  for (auto const& val : x_values_set) {
    x_values.push_back(val);
  }
  std::sort(x_values.begin(), x_values.end());
  std::unordered_map<int, int> x_value_id_map;
  for (int i = 0; i < sz; ++i) {
    x_value_id_map.insert(
        std::unordered_map<int, int>::value_type(x_values[i], i));
  }

  SegmentTree active(0, sz - 1, x_values);
  long long ans = 0;
  long long cur_x_sum = 0;
  int cur_y = events[0][0];
  for (auto const& event : events) {
    int y = event[0], type = event[1], x1 = event[2], x2 = event[3];
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

void StrTokenize(std::string const& line, std::vector<std::string>& res) {
  static const std::vector<char> delimiter_list{' ',  ':',  ';',
                                                '\t', '\r', '\n'};

  res.clear();
  std::string empty_str;
  bool is_delimiter, old_is_delimiter = true;
  int current_field = -1;
  for (auto const& c : line) {
    is_delimiter = false;
    for (auto const& delimiter : delimiter_list) {
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

int FindFirstNumber(std::string const& str) {
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
                  "Invalid naming convention: " << str);
    }
  }
  return res;
}

bool IsExecutableExisting(std::string const& executable_path) {
  // system call
  std::string command = "which " + executable_path + " >/dev/null";
  int res = std::system(command.c_str());
  return res == 0;
}

void ReportMemory() {
  auto peak_mem = getPeakRSS();
  auto curr_mem = getCurrentRSS();
  BOOST_LOG_TRIVIAL(info) << "(peak memory: " << (peak_mem >> 20u) << "MB, "
                          << " current memory: " << (curr_mem >> 20u)
                          << "MB)\n";
}

void MergeIntervals(std::vector<SegI>& intervals) {
  size_t sz = intervals.size();
  if (sz <= 1) return;

  std::sort(intervals.begin(), intervals.end(),
            [](const SegI& inter1, const SegI& inter2) {
              return inter1.lo < inter2.lo;
            });

  std::vector<SegI> res;

  int begin = intervals[0].lo;
  int end = intervals[0].hi;
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

}  // namespace dali
