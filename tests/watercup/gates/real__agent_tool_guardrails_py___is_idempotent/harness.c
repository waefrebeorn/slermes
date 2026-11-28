/* gate: _is_idempotent -> real port (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int agent_tool_guardrails_u_is_idempotent(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_is_idempotent");
  int r=agent_tool_guardrails_u_is_idempotent(argv[i]); WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_is_idempotent");} return 0;}
