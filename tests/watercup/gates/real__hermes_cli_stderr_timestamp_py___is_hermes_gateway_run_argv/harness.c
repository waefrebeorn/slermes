/* gate: _is_hermes_gateway_run_argv(argv) -> bool. */
#include <stdio.h>
#include <stdlib.h>
#include "watercup.h"
#include "hermes_json.h"
int stderr_ts_is_gateway_run_argv(char **argv, int argc);
int main(int argc,char**argv){ if(argc<3)return 2;
 for(int i=2;i<argc;i++){ WCUP_BEGIN("_is_hermes_gateway_run_argv");
  json_t *in=json_parse(argv[i],NULL);
  char *av[32]; int n=0;
  if(json_is_array(in)){ size_t c=json_len(in); for(size_t k=0;k<c&&n<32;k++){ json_t *it=json_get(in,k);
      av[n++]=(char*)((json_is_string(it)&&it->str_val)?it->str_val:""); } }
  WCUP_VALSTR(stderr_ts_is_gateway_run_argv(av,n)?"True":"False");
  char cup[32]; WCUP_FINALIZE(cup,sizeof cup); printf("%s\n",cup);
  WCUP_END("_is_hermes_gateway_run_argv"); if(in) json_free(in);} return 0;}
