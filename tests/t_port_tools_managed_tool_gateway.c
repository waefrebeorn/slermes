/* AUTO-GENERATED oracle harness for tools_managed_tool_gateway (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_managed_tool_gateway.c"

static const char *js(const char *s){
  static char b[4][4096]; static int t=0; t=(t+1)&3; char *q=b[t]; *q++='"';
  if(s) for(const char *p=s;*p&&(q-b[t])<4000;p++){unsigned char c=*p;
    if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
    else if(c=='\n'){*q++='\\';*q++='n';}
    else if(c=='\t'){*q++='\\';*q++='t';}
    else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}
    else *q++=c;}
  *q++='"'; *q=0; return b[t];
}
int main(void){
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("{\"func\":\"cli_tools_managed_tool_gateway_auth_json_path\",\"ret\":%s}\n", js(cli_tools_managed_tool_gateway_auth_json_path()));
  printf("{\"func\":\"cli_tools_managed_tool_gateway_get_tool_gateway_scheme\",\"ret\":%s}\n", js(cli_tools_managed_tool_gateway_get_tool_gateway_scheme()));
  return 0;
}
