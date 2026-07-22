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
#ifndef DALI_COMMON_HELPER_H_
#define DALI_COMMON_HELPER_H_

#include <fstream>
#include <string>
#include <vector>

#include "logging.h"
#include "misc.h"

namespace dali {

/** Log the command line used to start the current process. */
void SaveArgs(int argc, char* argv[]);

/**
 * Group command-line arguments into flag-led option lists.
 *
 * Each argument beginning with flag_prefix starts a new group. Later arguments
 * are appended to that group until the next prefixed flag is found.
 */
std::vector<std::vector<std::string>> ParseArguments(
    int argc, char* argv[], std::string const& flag_prefix);

/** Return the residual distance |x - round(x / y) * y|. */
double AbsResidual(double x, double y);

/** Return the union area covered by a list of integer rectangles. */
unsigned long long GetCoverArea(std::vector<RectI> const& rects);

/**
 * Round x when it is within epsilon of an integer; otherwise return ceil(x).
 */
double RoundOrCeiling(double x, double epsilon = 1e-5);

/** Split a line on whitespace and common punctuation delimiters. */
void StrTokenize(std::string const& line, std::vector<std::string>& res);

/** Return the index of the first digit in a string, or -1 if none exists. */
int FindFirstNumber(std::string const& str);

/** Return true when the executable can be found by the shell. */
bool IsExecutableExisting(std::string const& executable_path);

/** Log peak and current resident memory usage in megabytes. */
void ReportMemory();

/** Sort and merge overlapping integer intervals in place. */
void MergeIntervals(std::vector<SegI>& intervals);


/** Log a visual separator line. */
inline void PrintHorizontalLine() {
  LOG(info) << "------------------------------------------------------------\n";
}

}  // namespace dali

#endif  // DALI_COMMON_HELPER_H_
