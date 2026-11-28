/* gate: _gateway_health_config -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char * agent_monitoring_gateway_health_export_u_gateway_health_config(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_gateway_health_config");
  char * r=agent_monitoring_gateway_health_export_u_gateway_health_config(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_gateway_health_config"); if (r) free(r);} return 0;}
