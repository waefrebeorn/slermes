/* gate: _credential_secret_fingerprint(payload) -> str|None. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
char *credential_secret_fingerprint(const json_t *payload);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_credential_secret_fingerprint");
  json_t *p=json_parse(argv[i],NULL); char *r=credential_secret_fingerprint(p);
  WCUP_VALSTR(r?r:""); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_credential_secret_fingerprint"); if(r) free(r); if(p) json_free(p);} return 0;}
