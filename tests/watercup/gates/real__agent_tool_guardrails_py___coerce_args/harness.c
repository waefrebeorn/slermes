/* gate: _coerce_args -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char * agent_tool_guardrails_u_coerce_args(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_coerce_args");
  char * r=agent_tool_guardrails_u_coerce_args(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_coerce_args"); if (r) free(r);} return 0;}
