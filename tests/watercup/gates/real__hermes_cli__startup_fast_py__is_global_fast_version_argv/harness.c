/* gate: is_global_fast_version_argv -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int hermes_cli__startup_fast_u_is_global_fast_version_argv(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("is_global_fast_version_argv");
  int r=hermes_cli__startup_fast_u_is_global_fast_version_argv(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("is_global_fast_version_argv"); } return 0;}
