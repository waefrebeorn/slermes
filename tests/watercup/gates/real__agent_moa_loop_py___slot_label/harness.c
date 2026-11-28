/* gate: real__agent_moa_loop_py___slot_label -> real _slot_label (JSON slot -> string). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *moa_u_slot_label(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("moa_u_slot_label"); fflush(stdout);
  char*r=moa_u_slot_label(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("moa_u_slot_label"); free(r);} return 0;}
