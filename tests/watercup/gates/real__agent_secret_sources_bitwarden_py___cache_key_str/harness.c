/* gate: _cache_key_str -> real port (JSON in, str out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_secret_sources_bitwarden_u_cache_key_str(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_cache_key_str");
  char *r=agent_secret_sources_bitwarden_u_cache_key_str(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_cache_key_str"); if (r) free(r);} return 0;}
