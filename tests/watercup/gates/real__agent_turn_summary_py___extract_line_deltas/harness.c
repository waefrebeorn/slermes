/* gate: _extract_line_deltas -> real port. Input "tool\tresult". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *agent_turn_summary_u_extract_line_deltas(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_extract_line_deltas");
  char*r=agent_turn_summary_u_extract_line_deltas(argv[i]); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_extract_line_deltas"); free(r);} return 0;}
