/* gate: _otlp_config -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char * agent_monitoring_otlp_exporter_u_otlp_config(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_otlp_config");
  char * r=agent_monitoring_otlp_exporter_u_otlp_config(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_otlp_config"); if (r) free(r);} return 0;}
