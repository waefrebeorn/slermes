/* gate: hermes_cli_gateway_py___service_scope_label -> real service_scope_label -> "system"/"user". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *cgw_u_service_scope_label(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("cgw_u_service_scope_label"); fflush(stdout);
  char*r=cgw_u_service_scope_label(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("cgw_u_service_scope_label"); free(r);} return 0;}
