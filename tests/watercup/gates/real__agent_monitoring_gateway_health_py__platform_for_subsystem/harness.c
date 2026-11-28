/* gate: platform_for_subsystem -> real port (string in, string out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_monitoring_gateway_health_u_platform_for_subsystem(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("platform_for_subsystem");
  char*r=agent_monitoring_gateway_health_u_platform_for_subsystem(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("platform_for_subsystem"); free(r);} return 0;}
