/* gate: real__tools_binary_extensions_py__has_binary_extension -> real port (int 1/0). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int tools_binary_extensions_has_binary_extension(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("has_binary_extension");
  int r=tools_binary_extensions_has_binary_extension(argv[i]); WCUP_VALINT(r);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("has_binary_extension");} return 0;}
