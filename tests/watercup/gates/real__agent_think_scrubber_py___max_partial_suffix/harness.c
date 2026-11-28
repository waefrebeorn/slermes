/* gate: _max_partial_suffix(buf, tags) -> int. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
long ths_tag_max_partial(const char *buf, const char *const *tags, int n, int max_tag_len);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_max_partial_suffix");
  json_t *in=json_parse(argv[i],NULL);
  const char *buf=json_get_str(in,"buf","");
  json_t *tj=json_obj_get(in,"tags");
  const char *tags[16]; int n=0;
  if(json_is_array(tj)){ size_t c=json_len(tj); for(size_t k=0;k<c&&n<16;k++){ json_t *it=json_get(tj,k);
      if(json_is_string(it)&&it->str_val) tags[n++]=it->str_val; } }
  char js[32]; snprintf(js,sizeof js,"%ld",ths_tag_max_partial(buf,tags,n,23)); WCUP_VALSTR(js);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_max_partial_suffix"); if(in) json_free(in);} return 0;}
