/* gate: real__..._safe_int -> real port (int(float(value)) or default). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_rate_limit_tracker_u_safe_int(const char *arg);
int main(int argc, char **argv) { if (argc < 3) return 2;
  for (int i = 2; i < argc; i++) {
    WCUP_BEGIN("_safe_int");
    char *r = agent_rate_limit_tracker_u_safe_int(argv[i]);
    WCUP_VALSTR(r ? r : "");
    char cup[32]; WCUP_FINALIZE(cup, sizeof cup); printf("%s\n", cup);
    WCUP_END("_safe_int"); free(r);
  } return 0; }
