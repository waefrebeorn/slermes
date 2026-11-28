/* gate: _resolve_claim_ttl_seconds -> explicit-ttl clamp. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern int kanban_resolve_claim_ttl_seconds(int given, int ttl_seconds);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_resolve_claim_ttl_seconds");
  json_t *in=json_parse(argv[i],NULL);
  json_t *tt=json_obj_get(in,"ttl");
  int given=!(tt==NULL || json_is_null(tt));
  int v= json_is_number(tt) ? (int)tt->num_val : 0;
  int r=kanban_resolve_claim_ttl_seconds(given,v);
  char b[32]; snprintf(b,sizeof b,"%d",r); WCUP_VALSTR(b);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_resolve_claim_ttl_seconds"); if(in) json_free(in);} return 0;}
