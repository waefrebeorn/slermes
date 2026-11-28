/* gate: format_focus_toggle_message(enabled, mode) -> real C port. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *fv_format_focus_toggle_message(int enabled, const char *mode, int mode_is_bool, int mode_bool_val);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("format_focus_toggle_message");
  json_t *in=json_parse(argv[i],NULL);
  json_t *en=json_obj_get(in,"enabled");
  int enabled=json_is_bool(en)?(en->bool_val?1:0):(json_is_number(en)?(en->num_val!=0):0);
  json_t *md=json_obj_get(in,"mode");
  const char *ms=""; int isb=0, bv=0;
  if(json_is_bool(md)){isb=1;bv=md->bool_val?1:0;}
  else if(json_is_string(md)) ms=(md->str_val?md->str_val:"");
  else if(json_is_number(md)){ static char nb[32]; snprintf(nb,sizeof nb,"%g",md->num_val); ms=nb; }
  char *r=fv_format_focus_toggle_message(enabled,ms,isb,bv); WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("format_focus_toggle_message"); if(r) free(r); if(in) json_free(in);} return 0;}
