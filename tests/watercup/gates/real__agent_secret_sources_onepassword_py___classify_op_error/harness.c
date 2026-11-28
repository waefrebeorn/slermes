/* gate: _classify_op_error -> real port (str in, ErrorKind.value out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_secret_sources_onepassword_u_classify_op_error(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_classify_op_error");
  char *r=agent_secret_sources_onepassword_u_classify_op_error(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_classify_op_error"); if (r) free(r);} return 0;}
