/* gate: format_cost_label -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char * agent_usage_pricing_u_format_cost_label(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("format_cost_label");
  char *r=agent_usage_pricing_u_format_cost_label(argv[i]);
  WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("format_cost_label"); free(r);} return 0;}
