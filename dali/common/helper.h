//
// Created by Yihang Yang on 10/13/21.
//

#ifndef DALI_DALI_COMMON_HELPER_H_
#define DALI_DALI_COMMON_HELPER_H_

#include <string>
#include <vector>

namespace dali {

// splits a line into many words
void StrTokenize(std::string const &line, std::vector<std::string> &res);

// finds the first number in a string
int FindFirstNumber(std::string const &str);

// check if an executable can be found or not
bool IsExecutableExisting(std::string const &executable_path);

void ReportMemory();

}

#endif //DALI_DALI_COMMON_HELPER_H_
