/* gate: hermes_cli_session_export_py___messages -> real _messages (session JSON -> filtered JSON array string). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern char *sexp_u_messages(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("sexp_u_messages"); fflush(stdout);
  char*r=sexp_u_messages(argv[i]); WCUP_VALSTR(r?r:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("sexp_u_messages"); free(r);} return 0;}
