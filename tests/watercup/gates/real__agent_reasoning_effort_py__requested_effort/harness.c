/* gate: requested_effort(reasoning_config) -> str|None. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
#include "reasoning_effort.h"
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("requested_effort");
  json_t *cfg=json_parse(argv[i],NULL);
  char *r=requested_effort(cfg); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("requested_effort"); if(r) free(r); if(cfg) json_free(cfg);} return 0;}
