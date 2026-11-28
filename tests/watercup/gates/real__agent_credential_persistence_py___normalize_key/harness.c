/* gate: _normalize_key -> credential_normalize_key (buffer form). */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
void credential_normalize_key(const char *key, char *out, size_t out_sz);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_normalize_key"); char buf[256];
  credential_normalize_key(argv[i],buf,sizeof buf); WCUP_VALSTR(buf);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup); WCUP_END("_normalize_key"); }
 return 0;}
