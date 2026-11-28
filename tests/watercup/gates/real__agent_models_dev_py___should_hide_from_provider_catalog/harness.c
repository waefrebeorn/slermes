/* gate: _should_hide_from_provider_catalog -> real port (JSON in, int out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int agent_models_dev_u_should_hide_from_provider_catalog(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_should_hide_from_provider_catalog");
  int r=agent_models_dev_u_should_hide_from_provider_catalog(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_should_hide_from_provider_catalog");} return 0;}
