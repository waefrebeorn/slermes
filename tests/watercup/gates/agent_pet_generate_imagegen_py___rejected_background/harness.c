/* gate: agent_pet_generate_imagegen_py___rejected_background -> real _rejected_background (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int agent_pet_generate_imagegen_u_rejected_background(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("agent_pet_generate_imagegen_u_rejected_background"); fflush(stdout);
  int r=agent_pet_generate_imagegen_u_rejected_background(argv[i]); WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("agent_pet_generate_imagegen_u_rejected_background");} return 0;}
