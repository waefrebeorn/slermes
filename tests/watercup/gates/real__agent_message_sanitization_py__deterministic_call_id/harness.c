/* gate: deterministic_call_id -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_message_sanitization_u_deterministic_call_id(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("deterministic_call_id");
  char *r=agent_message_sanitization_u_deterministic_call_id(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("deterministic_call_id"); if (r) free(r);} return 0;}
