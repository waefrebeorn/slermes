/* gate: _scope_hash -> real port (str in, hex str out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *gateway_status_u_scope_hash(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_scope_hash");
  char *r=gateway_status_u_scope_hash(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_scope_hash"); if (r) free(r);} return 0;}
