/* gate: total_tokens -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern long agent_usage_pricing_u_total_tokens(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("total_tokens");
  long r=agent_usage_pricing_u_total_tokens(argv[i]);
  WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("total_tokens"); } return 0;}
