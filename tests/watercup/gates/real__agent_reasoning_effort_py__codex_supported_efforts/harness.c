/* gate: codex_supported_efforts -> table rendered as a JSON array. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
#include "reasoning_effort.h"
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("codex_supported_efforts");
  size_t n=0; const char *const *tb=codex_supported_efforts(argv[i],&n);
  json_t *a=json_array(); for(size_t k=0;k<n;k++) json_append(a,json_string(tb[k]));
  char *js=json_serialize(a); WCUP_VALSTR(js?js:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("codex_supported_efforts"); if(js) free(js); json_free(a);} return 0;}
