/* gate: is_borrowed_credential_source(source, provider_id) -> bool. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
#include <stdbool.h>
bool is_borrowed_credential_source(const char *source, const char *provider_id);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("is_borrowed_credential_source");
  json_t *in=json_parse(argv[i],NULL);
  const char *s=json_get_str(in,"source",""), *p=json_get_str(in,"provider","");
  WCUP_VALSTR(is_borrowed_credential_source(s,p)?"True":"False");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("is_borrowed_credential_source"); if(in) json_free(in);} return 0;}
