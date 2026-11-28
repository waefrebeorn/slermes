/* gate: _message_field(msg, name) -> real acf_message_field(io,in); io unused. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
#include "auxiliary_client_flow.h"
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_message_field");
  json_t *in=json_parse(argv[i],NULL);
  json_t *r=acf_message_field(NULL,in);
  if (r && r->type == JSON_NULL) WCUP_VALSTR("");
  else if (json_is_string(r)) WCUP_VALSTR(r->str_val ? r->str_val : "");
  else if (r) { char *js=json_serialize(r); WCUP_VALSTR(js?js:""); free(js); }
  else WCUP_VALSTR("");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_message_field"); if(r) json_free(r); if(in) json_free(in);} return 0;}
