/* gate: _parse_single_entry -> real port. Input JSON {"event":..., "raw":...}.
 * Cup is the projection event|command|matcher|timeout|fail_closed (or "None"). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_shell_hooks_u_parse_single_entry(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_parse_single_entry");
  char*r=agent_shell_hooks_u_parse_single_entry(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_parse_single_entry"); free(r);} return 0;}
