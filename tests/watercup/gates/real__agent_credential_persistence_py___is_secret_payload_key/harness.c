/* gate: _is_secret_payload_key -> bool. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include <stdbool.h>
bool is_secret_payload_key(const char *key);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_is_secret_payload_key");
  WCUP_VALSTR(is_secret_payload_key(argv[i])?"True":"False");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("_is_secret_payload_key"); }
 return 0;}
