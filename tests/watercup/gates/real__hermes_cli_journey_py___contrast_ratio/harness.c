/* gate: contrast_ratio -> real port. Input "r,g,b;r,g,b". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *hermes_cli_journey_u_contrast_ratio(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_contrast_ratio");
  char*r=hermes_cli_journey_u_contrast_ratio(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_contrast_ratio"); free(r);} return 0;}
