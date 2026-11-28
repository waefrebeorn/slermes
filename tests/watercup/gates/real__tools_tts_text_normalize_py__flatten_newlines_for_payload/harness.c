/* gate: flatten_newlines_for_payload -> real port. Input is HEX of the UTF-8 text. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_tts_text_normalize_u_flatten_newlines_for_payload(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("flatten_newlines_for_payload");
  char*r=agent_tts_text_normalize_u_flatten_newlines_for_payload(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("flatten_newlines_for_payload"); free(r);} return 0;}
