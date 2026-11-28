/* gate: models_dev supports_vision -> real port (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern int agent_models_dev_u_supports_vision(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("supports_vision");
  int r=agent_models_dev_u_supports_vision(argv[i]); WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("supports_vision");} return 0;}
