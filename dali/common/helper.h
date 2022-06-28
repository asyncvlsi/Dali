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

void SaveArgs(int argc, char *argv[]);

std::vector<std::vector<std::string>> ParseArguments(
    int argc,
    char *argv[],
    std::string const &flag_prefix
);

// custom residual function, return: |x - round(x/y) * y|
double AbsResidual(double x, double y);

unsigned long long GetCoverArea(std::vector<RectI> &rects);

// perform round() when x is close to an integer within an epsilon, other perform ceiling()
double RoundOrCeiling(double x, double epsilon = 1e-5);

// splits a line into many words
void StrTokenize(std::string const &line, std::vector<std::string> &res);

// finds the first number in a string
int FindFirstNumber(std::string const &str);

// check if an executable can be found or not
bool IsExecutableExisting(std::string const &executable_path);

void ReportMemory();

void MergeIntervals(std::vector<SegI> &intervals);

/****
 * Create a square with vertices at (lx,ly), (ux,ly), (ux,uy), and (lx,uy).
 * Specify x as the x-coordinates of the vertices and y as the y-coordinates.
 * MATLAB command `patch` automatically connects the last (x,y) coordinate with
 * the first (x,y) coordinate.
 *
 * @param ost: output file stream
 * @param rect: the given rectangle
 * @param has_rgb: use rgb or not
 * @param r: red channel 0-1
 * @param g: green channel 0-1
 * @param b: blue channel 0-1
 *
 * (0,0,0) black, (1,1,1) white
 */
template<class T>
void SaveMatlabPatchRect(
    std::ofstream &ost,
    T lx, T ly, T ux, T uy,
    bool has_rgb = false,
    double r = 0.0, double g = 0.0, double b = 0.0
) {
  ost << lx
      << "\t" << ux
      << "\t" << ux
      << "\t" << lx
      << "\t" << ly
      << "\t" << ly
      << "\t" << uy
      << "\t" << uy;
  if (has_rgb) {
    ost << "\t" << r
        << "\t" << g
        << "\t" << b;
  }
  ost << "\n";
}

template<class T>
void SaveMatlabPatchRegion(
    std::ofstream &ost,
    Rect<T> &n_rect, Rect<T> &p_rect
) {
  ost << n_rect.LLX() << "\t"
      << n_rect.URX() << "\t"
      << n_rect.URX() << "\t"
      << n_rect.LLX() << "\t"
      << n_rect.LLY() << "\t"
      << n_rect.LLY() << "\t"
      << n_rect.URY() << "\t"
      << n_rect.URY() << "\t"
      << p_rect.LLX() << "\t"
      << p_rect.URX() << "\t"
      << p_rect.URX() << "\t"
      << p_rect.LLX() << "\t"
      << p_rect.LLY() << "\t"
      << p_rect.LLY() << "\t"
      << p_rect.URY() << "\t"
      << p_rect.URY() << "\n";
}

inline void PrintHorizontalLine() {
  BOOST_LOG_TRIVIAL(info)
    << "-------------------------------------------------------------\n";
}

}

#endif //DALI_COMMON_HELPER_H_
