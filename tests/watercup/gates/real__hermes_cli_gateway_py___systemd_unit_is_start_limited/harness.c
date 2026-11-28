/* gate: _systemd_unit_is_start_limited -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int hermes_cli_gateway_u_systemd_unit_is_start_limited(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_systemd_unit_is_start_limited");
  int r=hermes_cli_gateway_u_systemd_unit_is_start_limited(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_systemd_unit_is_start_limited"); } return 0;}
