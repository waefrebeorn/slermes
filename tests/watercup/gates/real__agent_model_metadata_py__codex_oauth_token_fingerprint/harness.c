/* gate: _codex_oauth_token_fingerprint -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char * agent_model_metadata_u_codex_oauth_token_fingerprint(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_codex_oauth_token_fingerprint");
  char * r=agent_model_metadata_u_codex_oauth_token_fingerprint(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_codex_oauth_token_fingerprint"); if (r) free(r);} return 0;}
