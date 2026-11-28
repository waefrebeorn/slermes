/* gate: mcp_server_enabled(cfg) -> bool. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
int w8_mcptoolcom_f3c09b_mcp_server_enabled(const struct json_t *cfg);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("mcp_server_enabled");
  json_t *cfg=json_parse(argv[i],NULL);
  WCUP_VALSTR(w8_mcptoolcom_f3c09b_mcp_server_enabled(cfg)?"True":"False");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("mcp_server_enabled"); if(cfg) json_free(cfg);} return 0;}
