/* gate: _strip_orphan_close_tags(text) -> str. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
int ths_strip_orphan(const char *text, char *out, size_t cap);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_strip_orphan_close_tags"); char buf[4096];
  ths_strip_orphan(argv[i],buf,sizeof buf); WCUP_VALSTR(buf);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_strip_orphan_close_tags"); } return 0;}
