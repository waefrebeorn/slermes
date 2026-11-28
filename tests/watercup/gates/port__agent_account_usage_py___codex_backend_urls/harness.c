/* gate: _codex_backend_urls -> real port (base_url in, JSON array of 3 out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_account_usage_u_codex_backend_urls(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_codex_backend_urls");
  char *r=agent_account_usage_u_codex_backend_urls(argv[i]); WCUP_VALSTR(r?r:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_codex_backend_urls"); if(r) free(r);} return 0;}
