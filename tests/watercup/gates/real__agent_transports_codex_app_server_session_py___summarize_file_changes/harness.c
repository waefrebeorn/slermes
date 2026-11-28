/* gate: _summarize_file_changes -> real port (JSON list in, summary out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_codex_app_server_session_u_summarize_file_changes(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_summarize_file_changes");
  char*r=agent_codex_app_server_session_u_summarize_file_changes(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_summarize_file_changes"); free(r);} return 0;}
