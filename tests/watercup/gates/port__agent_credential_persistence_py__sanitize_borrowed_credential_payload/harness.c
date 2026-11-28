/* gate: sanitize_borrowed_credential_payload(payload, provider) -> dict (sorted keys). */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
char *sanitize_borrowed_credential_payload(const json_t *payload, const char *provider_id);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("sanitize_borrowed_credential_payload");
  json_t *in=json_parse(argv[i],NULL);
  const char *p=json_get_str(in,"provider",NULL);
  char *r=sanitize_borrowed_credential_payload(in,p); WCUP_VALSTR(r?r:"{}");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("sanitize_borrowed_credential_payload"); if(r) free(r); if(in) json_free(in);} return 0;}
