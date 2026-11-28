/* gate: format_profile_label(name, display_name) -> real C (malloc'd string). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern char *profiles_format_profile_label(const char *name, const char *display_name);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("format_profile_label");
  json_t *in=json_parse(argv[i],NULL);
  const char *nm=json_get_str(in,"name","");
  json_t *dn=json_obj_get(in,"display_name");
  const char *dnv=(json_is_string(dn)&&dn->str_val)?dn->str_val:"";
  char *r=profiles_format_profile_label(nm,json_is_null(dn)?NULL:dnv);
  WCUP_VALSTR(r?r:""); char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("format_profile_label"); if(r) free(r); if(in) json_free(in);} return 0;}
