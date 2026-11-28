/* gate: _extract_response_text -> real port (JSON payload in, text out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_x_search_tool_u_extract_response_text(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_extract_response_text");
  char*r=agent_x_search_tool_u_extract_response_text(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_extract_response_text"); free(r);} return 0;}
