/* gate: detect_reaction -> real port. Input is HEX of the UTF-8 text. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_reactions_u_detect_reaction(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("detect_reaction");
  char*r=agent_reactions_u_detect_reaction(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("detect_reaction"); free(r);} return 0;}
