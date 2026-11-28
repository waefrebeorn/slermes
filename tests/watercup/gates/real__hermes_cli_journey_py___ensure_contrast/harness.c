/* gate: ensure_contrast -> real port. Input "fg;bg;min". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *hermes_cli_journey_u_ensure_contrast(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_ensure_contrast");
  char*r=hermes_cli_journey_u_ensure_contrast(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_ensure_contrast"); free(r);} return 0;}
