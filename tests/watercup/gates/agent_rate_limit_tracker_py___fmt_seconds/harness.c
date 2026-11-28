/* gate: agent_rate_limit_tracker_py___fmt_seconds -> real fmt_seconds -> string. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_rate_limit_tracker_u_fmt_seconds(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("agent_rate_limit_tracker_u_fmt_seconds"); fflush(stdout);
  char*r=agent_rate_limit_tracker_u_fmt_seconds(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("agent_rate_limit_tracker_u_fmt_seconds"); free(r);} return 0;}
