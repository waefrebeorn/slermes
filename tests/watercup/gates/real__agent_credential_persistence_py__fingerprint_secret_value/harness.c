/* gate: fingerprint_secret_value -> str|None. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
char *fingerprint_secret_value(const void *value);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("fingerprint_secret_value");
  char *r=fingerprint_secret_value(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("fingerprint_secret_value"); if(r) free(r);} return 0;}
