/* gate: _has_top_level_alternation -> real port (pattern in, int out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int agent_redact_pattern_shape_u_has_top_level_alternation(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_has_top_level_alternation");
  int r=agent_redact_pattern_shape_u_has_top_level_alternation(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_has_top_level_alternation");} return 0;}
