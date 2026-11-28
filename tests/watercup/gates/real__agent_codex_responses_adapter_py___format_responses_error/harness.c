/* gate: _format_responses_error -> real port. Input JSON {"error":...,"status":...}. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *agent_codex_responses_u_format_responses_error(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_format_responses_error");
  char*r=agent_codex_responses_u_format_responses_error(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_format_responses_error"); free(r);} return 0;}
