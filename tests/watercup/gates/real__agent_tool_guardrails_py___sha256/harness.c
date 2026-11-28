/* gate: _sha256 -> real port (str in, hex str out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_tool_guardrails_u_sha256(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_sha256");
  char *r=agent_tool_guardrails_u_sha256(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_sha256"); if (r) free(r);} return 0;}
