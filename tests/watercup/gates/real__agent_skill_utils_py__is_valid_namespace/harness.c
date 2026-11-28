/* gate: is_valid_namespace -> real port (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int agent_skill_utils_u_is_valid_namespace(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("is_valid_namespace");
  int r=agent_skill_utils_u_is_valid_namespace(argv[i]); WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("is_valid_namespace");} return 0;}
