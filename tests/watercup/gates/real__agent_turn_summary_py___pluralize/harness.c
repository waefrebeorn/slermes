/* gate: _pluralize -> real port. Input "count\tnoun". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_turn_summary_u_pluralize(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_pluralize");
  char*r=agent_turn_summary_u_pluralize(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_pluralize"); free(r);} return 0;}
