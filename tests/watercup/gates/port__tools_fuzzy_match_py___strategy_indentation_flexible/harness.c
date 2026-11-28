/* gate: _strategy_indentation_flexible(content, pattern) -> spans as [[s,e],...]. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "watercup.h"
#include "hermes_json.h"
extern void fuzzy_match_strategy_indentation_flexible(const char *content, const char *pattern,
                                                     long **matches, size_t *nmatch);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_strategy_indentation_flexible");
  json_t *in=json_parse(argv[i],NULL);
  const char *c=json_get_str(in,"content",""), *p=json_get_str(in,"pattern","");
  long *m=NULL; size_t n=0;
  fuzzy_match_strategy_indentation_flexible(c,p,&m,&n);
  json_t *arr=json_array();
  for(size_t k=0;k<n;k++){ json_t *pr=json_array();
      json_append(pr,json_number((double)m[2*k])); json_append(pr,json_number((double)m[2*k+1]));
      json_append(arr,pr); }
  char *js=json_serialize(arr); WCUP_VALSTR(js?js:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_strategy_indentation_flexible");
  if(js) free(js); json_free(arr); if(m) free(m); if(in) json_free(in);} return 0;}
