/* gate: _is_cjk_token_dense_char -> real port. Input is HEX of the UTF-8 text. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int agent_model_metadata_u_is_cjk_token_dense_char(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_is_cjk_token_dense_char");
  int r=agent_model_metadata_u_is_cjk_token_dense_char(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_is_cjk_token_dense_char");} return 0;}
