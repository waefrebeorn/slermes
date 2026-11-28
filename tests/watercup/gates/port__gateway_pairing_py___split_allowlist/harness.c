/* gate: _split_allowlist(raw) -> list[str]. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
char *pairing_split_allowlist_json(const char *raw);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_split_allowlist");
  char *js=pairing_split_allowlist_json(argv[i]); WCUP_VALSTR(js?js:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_split_allowlist"); if(js) free(js);} return 0;}
