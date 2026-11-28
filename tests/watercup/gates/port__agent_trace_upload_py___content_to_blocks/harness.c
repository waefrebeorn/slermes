/* gate: _content_to_blocks -> real port (JSON {content,redact} in, JSON array out). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *agent_trace_upload_u_content_to_blocks(const char *arg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_content_to_blocks");
  char *r=agent_trace_upload_u_content_to_blocks(argv[i]); WCUP_VALSTR(r?r:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_content_to_blocks"); if(r) free(r);} return 0;}
