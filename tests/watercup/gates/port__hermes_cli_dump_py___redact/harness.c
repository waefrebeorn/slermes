/* gate: dump._redact -> red_mask_secret(v, 4, 4) (python defaults). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *red_mask_secret(const char *secret, long head, long tail);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_redact");
  char *r=red_mask_secret(argv[i],4,4); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_redact"); if(r) free(r);} return 0;}
