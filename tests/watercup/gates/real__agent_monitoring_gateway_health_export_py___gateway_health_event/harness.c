/* gate: _gateway_health_event -> real port (JSON in, int out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int agent_monitoring_gateway_health_export_u_gateway_health_event(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_gateway_health_event");
  int r=agent_monitoring_gateway_health_export_u_gateway_health_event(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_gateway_health_event");} return 0;}
