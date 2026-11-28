/* gate: _normalize(platform, chat_id) -> "platform:chat". */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
void dead_target_normalize_key(const char *platform, const char *chat_id, char *out, size_t cap);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_normalize");
  json_t *in=json_parse(argv[i],NULL);
  const char *p=json_get_str(in,"platform",""), *c=json_get_str(in,"chat_id","");
  char buf[600]; dead_target_normalize_key(p,c,buf,sizeof buf); WCUP_VALSTR(buf);
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_normalize"); if(in) json_free(in);} return 0;}
