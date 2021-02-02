//
// Created by yihang on 1/27/21.
//

#include "dali/daliscm/daliscm.h"

#define PROMPT "(dali-scm) "

int main(int argc, char **argv) {
  int num_of_thread_openmp = 1;
  omp_set_num_threads(num_of_thread_openmp);

  FILE *fp;

  LispInit();
  LispCliInitPlain(PROMPT, dali::Cmds, sizeof(dali::Cmds) / sizeof(dali::Cmds[0]));

  fp = stdin;

  LispCliRun(fp);
  fclose(fp);

  LispCliEnd();

  return 0;
}