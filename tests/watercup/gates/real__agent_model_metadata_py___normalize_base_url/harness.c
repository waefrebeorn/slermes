/* gate: _normalize_base_url -> real port (string in, string out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_model_metadata_u_normalize_base_url(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_normalize_base_url");
  char*r=agent_model_metadata_u_normalize_base_url(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_normalize_base_url"); free(r);} return 0;}
