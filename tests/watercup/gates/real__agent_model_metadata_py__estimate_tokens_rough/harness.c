/* gate: estimate_tokens_rough -> real port. Input is HEX of the UTF-8 text. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern long agent_model_metadata_u_estimate_tokens_rough(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("estimate_tokens_rough");
  long r=agent_model_metadata_u_estimate_tokens_rough(argv[i]); WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("estimate_tokens_rough");} return 0;}
