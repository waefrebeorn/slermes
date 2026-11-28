/* gate: _is_untrusted_tool -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int agent_tool_dispatch_helpers_u_is_untrusted_tool(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_is_untrusted_tool");
  int r=agent_tool_dispatch_helpers_u_is_untrusted_tool(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_is_untrusted_tool"); } return 0;}
