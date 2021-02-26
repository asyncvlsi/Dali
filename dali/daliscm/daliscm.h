//
// Created by Yihang Yang on 2021-01-27.
//

#ifndef DALI_DALI_DALISCM_DALISCM_H_
#define DALI_DALI_DALISCM_DALISCM_H_

#include <cstdio>
#include <cstring>

#include <unordered_map>

#include <lisp.h>
#include <lispCli.h>

#include "dali/circuit.h"
#include "dali/placer.h"

namespace dali {

int process_runtest(int argc, char **argv);
int process_initDali(int argc, char **argv);
int process_readLef(int argc, char **argv);
int process_readDef(int argc, char **argv);
int process_readCell(int argc, char **argv);
int process_loadConf(int argc, char **argv);
int process_setDensity(int argc, char **argv);
int process_globalPlace(int argc, char **argv);
int process_legalize(int argc, char **argv);
int process_wellLegalize(int argc, char **argv);
int process_savePlace(int argc, char **argv);
int process_closeDali(int argc, char **argv);

inline char run_test_str[] = "run_test";
inline char initDali_str[] = "initDali";
inline char readLEF_str[] = "readLEF";
inline char readDEF_str[] = "readDEF";
inline char readCell_str[] = "readCell";
inline char loadConf_str[] = "loadConf";
inline char setDensity_str[] = "setDensity";
inline char globalPlace_str[] = "globalPlace";
inline char legalize_str[] = "legalize";
inline char wellLegalize_str[] = "wellLegalize";
inline char savePlace_str[] = "savePlace";
inline char closeDali_str[] = "closeDali";

inline struct LispCliCommand Cmds[] = {
    {nullptr, "Dali - Placement", nullptr},

    {run_test_str, "run_test, run a simple test case", dali::process_runtest},
    {initDali_str, "initDali [-v verbosity_level(0-5)] - initialize dali", dali::process_initDali},
    {readLEF_str, "readLEF <lefFile> [-g grid_value_x, grid_value_y] - read LEF file", dali::process_readLef},
    {readDEF_str, "readDEF <defFile> - read DEF file", dali::process_readDef},
    {readCell_str, "readCell <cellFile> - read CELL file", dali::process_readCell},
    {loadConf_str, "loadConf <confFile> - load Dali placement configuration file", dali::process_loadConf},
    {setDensity_str, "setDensity <density> - set placement target density", dali::process_setDensity},
    {globalPlace_str, "globalPlace - start global placement", dali::process_globalPlace},
    {legalize_str, "legalize - start legalization", dali::process_legalize},
    {wellLegalize_str, "wellLegalize - start well legalization", dali::process_wellLegalize},
    {savePlace_str, "savePlace <outputDefFile> - save DEF file, only needs the base name", dali::process_savePlace},
    {closeDali_str, "closeDali - close Dali and free memory", dali::process_closeDali}
};

}

#endif //DALI_DALI_DALISCM_DALISCM_H_
