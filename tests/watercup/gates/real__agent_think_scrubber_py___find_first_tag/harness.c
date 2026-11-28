/* gate: _find_first_tag(buf, tags) -> (idx, len). */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
int ths_tag_find_first(const char *buf, const char *const *tags, int n, int *out_len);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_find_first_tag");
  json_t *in=json_parse(argv[i],NULL);
  const char *buf=json_get_str(in,"buf","");
  json_t *tj=json_obj_get(in,"tags");
  const char *tags[16]; int n=0;
  if(json_is_array(tj)){ size_t c=json_len(tj); for(size_t k=0;k<c&&n<16;k++){ json_t *it=json_get(tj,k);
      if(json_is_string(it)&&it->str_val) tags[n++]=it->str_val; } }
  int tl=0; int idx=ths_tag_find_first(buf,tags,n,&tl);
  char js[64]; snprintf(js,sizeof js,"[%d,%d]",idx,tl); WCUP_VALSTR(js);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_find_first_tag"); if(in) json_free(in);} return 0;}
