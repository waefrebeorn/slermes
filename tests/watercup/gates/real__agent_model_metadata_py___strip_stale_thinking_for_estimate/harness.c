/* gate: _strip_stale_thinking_for_estimate -> projection "role[sortedkeys]" joined by |. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_model_metadata_u_strip_stale_thinking_for_estimate(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_strip_stale_thinking_for_estimate");
  char*r=agent_model_metadata_u_strip_stale_thinking_for_estimate(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_strip_stale_thinking_for_estimate"); free(r);} return 0;}
