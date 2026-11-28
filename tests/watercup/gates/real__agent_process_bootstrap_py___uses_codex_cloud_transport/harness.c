/* gate: _uses_codex_cloud_transport -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int hermes_agent_process_bootstrap_uses_codex_cloud_transport(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_uses_codex_cloud_transport");
  int r=hermes_agent_process_bootstrap_uses_codex_cloud_transport(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_uses_codex_cloud_transport"); } return 0;}
