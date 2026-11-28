/* gate: _strip_reference_wrappers -> real port (string in, string out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *agent_context_references_u_strip_reference_wrappers(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_strip_reference_wrappers");
  char*r=agent_context_references_u_strip_reference_wrappers(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_strip_reference_wrappers"); free(r);} return 0;}
