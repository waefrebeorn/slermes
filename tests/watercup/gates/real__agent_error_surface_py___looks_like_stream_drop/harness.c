/* gate: _looks_like_stream_drop -> real port (str in, int out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
extern int agent_error_surface_u_looks_like_stream_drop(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_looks_like_stream_drop");
  int r=agent_error_surface_u_looks_like_stream_drop(argv[i]); WCUP_VALSTR(r ? "1" : "0");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_looks_like_stream_drop");} return 0;}
