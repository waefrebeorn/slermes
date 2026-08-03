/* AUTO-GENERATED oracle harness for tools_mcp_oauth_manager (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_mcp_oauth_manager.c"

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
  printf("{\"func\":\"cli_tools_mcp_oauth_manager__make_hermes_provider_class\",\"ret\":0}\n", "cli_tools_mcp_oauth_manager__make_hermes_provider_class", cli_tools_mcp_oauth_manager__make_hermes_provider_class());
  cli_tools_mcp_oauth_manager_reset_manager_for_tests();
  return 0;
}
