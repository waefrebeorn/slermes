/* gate: _coerce_insecure(ssl_verify) -> bool. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include <stdbool.h>
bool agent_ssl_verify_coerce_insecure(const char *ssl_verify);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_coerce_insecure");
  WCUP_VALSTR(agent_ssl_verify_coerce_insecure(argv[i])?"True":"False");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("_coerce_insecure"); }
 return 0;}
