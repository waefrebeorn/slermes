/* gate: _positive_pid -> real port (str/JSON in, str out; "" == None). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *hermes_cli_gateway_u_positive_pid(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_positive_pid");
  char *r=hermes_cli_gateway_u_positive_pid(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_positive_pid"); if (r) free(r);} return 0;}
