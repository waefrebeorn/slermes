/* gate: _coerce_positive_int -> real port. Input "value\tdefault". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern char *tools_tool_output_limits_u_coerce_positive_int(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_coerce_positive_int");
  char*r=tools_tool_output_limits_u_coerce_positive_int(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_coerce_positive_int"); free(r);} return 0;}
