//
// Created by Yihang Yang on 10/13/21.
//

#include "helper.h"

#include "dali/common/logging.h"
#include "dali/common/memory.h"

namespace dali {

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

}
