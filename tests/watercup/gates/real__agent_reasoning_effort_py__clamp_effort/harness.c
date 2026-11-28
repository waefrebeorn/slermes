/* gate: clamp_effort(effort, supported, overrides) -> str|None. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
#include "reasoning_effort.h"
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("clamp_effort");
  json_t *in=json_parse(argv[i],NULL);
  json_t *ej=json_obj_get(in,"effort");
  json_t *sj=json_obj_get(in,"supported");
  json_t *oj=json_obj_get(in,"overrides");
  const char *eff=NULL; if(json_is_string(ej)&&ej->str_val) eff=ej->str_val;
  const char *sup[32]; size_t ns=0;
  if(json_is_array(sj)){ size_t c=json_len(sj); for(size_t k=0;k<c&&ns<32;k++){ json_t *it=json_get(sj,k);
      if(json_is_string(it)&&it->str_val) sup[ns++]=it->str_val; } }
  re_override_t ov[32]; size_t no=0;
  if(json_is_object(oj)){ size_t c=json_object_size(oj);
      for(size_t k=0;k<c;k++){ const char *kk=json_object_get_key_at(oj,k); json_t *vv=json_object_get_at(oj,k);
          if(kk&&json_is_string(vv)&&vv->str_val&&no<32){ ov[no].from=kk; ov[no].to=vv->str_val; no++; } } }
  const char *r=clamp_effort(eff, sup, ns, ov, no);
  WCUP_VALSTR(r?r:"");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("clamp_effort"); if(in) json_free(in);} return 0;}
