/* gate: _bech32_hrp_expand -> real port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char * gateway_authz_mixin_u_bech32_hrp_expand(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_bech32_hrp_expand");
  char * r=gateway_authz_mixin_u_bech32_hrp_expand(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_bech32_hrp_expand"); if (r) free(r);} return 0;}
