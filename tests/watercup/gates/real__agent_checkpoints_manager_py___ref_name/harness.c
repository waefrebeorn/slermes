/* gate: _ref_name -> real port (str in, str out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_checkpoints_manager_u_ref_name(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_ref_name");
  char*r=agent_checkpoints_manager_u_ref_name(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_ref_name"); free(r);} return 0;}
