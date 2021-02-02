//
// Created by Yihang Yang on 2021-01-27.
//

#ifndef DALI_DALI_DALISCM_DALISCM_H_
#define DALI_DALI_DALISCM_DALISCM_H_

#include <cstdio>
#include <cstring>

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
int process_setGrid(int argc, char **argv);
int process_globalPlace(int argc, char **argv);
int process_legalize(int argc, char **argv);
int process_wellLegalize(int argc, char **argv);
int process_savePlace(int argc, char **argv);
int process_closeDali(int argc, char **argv);

inline struct LispCliCommand Cmds[] = {
    {nullptr, "Dali - Placement", nullptr},

    {"run_test", "run_test, run a simple test case", dali::process_runtest},
    {"initDali", "initDali [-v verbosity_level(0-5)] - initialize dali", dali::process_initDali},
    {"readLef", "readLef <lefFile> - read LEF file", dali::process_readLef},
    {"readDef", "readDef <defFile> - read DEF file", dali::process_readDef},
    {"readCell", "readCell <cellFile> - read CELL file", dali::process_readCell},
    {"loadConf", "loadConf <confFile> - load Dali placement configuration file", dali::process_loadConf},
    {"setDensity", "setDensity <density> - set placement target density", dali::process_setDensity},
    {"setGrid", "setGrid <grid_value_x, grid_value_y> - set placement grid value", dali::process_setGrid},
    {"globalPlace", "globalPlace - start global placement", dali::process_globalPlace},
    {"legalize", "legalize - start legalization", dali::process_legalize},
    {"wellLegalize", "wellLegalize - start well legalization", dali::process_wellLegalize},
    {"savePlace", "savePlace <outputDefFile> - save DEF file, only needs the base name", dali::process_savePlace},
    {"closeDali", "closeDali - close Dali and free memory", dali::process_closeDali}
};

}

#endif //DALI_DALI_DALISCM_DALISCM_H_
