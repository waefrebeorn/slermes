/* gate: fingerprint_secret_value -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_credential_persistence_u_fingerprint_secret_value(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("fingerprint_secret_value");
  char *r=agent_credential_persistence_u_fingerprint_secret_value(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("fingerprint_secret_value"); if (r) free(r);} return 0;}
