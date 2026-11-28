/* gate: _prepare_child_command(argv, environ) -> argv. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
void stderr_ts_prepare_child_command(char **argv, int argc, const char *xpc_service_name,
                                     char **out, int *outc);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_prepare_child_command");
  json_t *in=json_parse(argv[i],NULL);
  json_t *aj=json_obj_get(in,"argv");
  char *av[32]; int n=0;
  if(json_is_array(aj)){ size_t c=json_len(aj); for(size_t k=0;k<c&&n<32;k++){ json_t *it=json_get(aj,k);
      av[n++]=(char*)((json_is_string(it)&&it->str_val)?it->str_val:""); } }
  const char *xpc=json_get_str(in,"xpc","");
  char *out[64]; int oc=0;
  stderr_ts_prepare_child_command(av,n,xpc,out,&oc);
  json_t *res=json_array();
  for(int k=0;k<oc;k++) json_append(res,json_string(out[k]));
  char *js=json_serialize(res); WCUP_VALSTR(js?js:"[]");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_prepare_child_command"); if(js) free(js); json_free(res); if(in) json_free(in);} return 0;}
