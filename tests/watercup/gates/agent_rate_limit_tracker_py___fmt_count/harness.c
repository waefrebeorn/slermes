/* gate: agent_rate_limit_tracker_py___fmt_count -> real fmt_count -> string. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_rate_limit_tracker_u_fmt_count(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("agent_rate_limit_tracker_u_fmt_count"); fflush(stdout);
  char*r=agent_rate_limit_tracker_u_fmt_count(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("agent_rate_limit_tracker_u_fmt_count"); free(r);} return 0;}
