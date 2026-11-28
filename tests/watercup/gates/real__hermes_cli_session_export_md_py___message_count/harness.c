/* gate: real__hermes_cli_session_export_md_py___message_count -> real _message_count (session JSON -> long). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "json.h"
extern long sexmd_u_message_count(const char *session_json);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("sexmd_u_message_count"); fflush(stdout);
  long r=sexmd_u_message_count(argv[i]); char b[32]; snprintf(b,sizeof b,"%ld",r);
  WCUP_VALSTR(b); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("sexmd_u_message_count"); } return 0; }
