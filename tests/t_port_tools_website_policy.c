/* AUTO-GENERATED oracle harness for tools_website_policy (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_website_policy.c"

static const char *js(const char *s){
  static char b[4][4096]; static int t=0; char *q=b[t]; t=(t+1)&3; *q++='"';
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
  printf("{\"func\":\"cli_tools_website_policy__get_default_config_path\",\"ret\":%s}\n", "cli_tools_website_policy__get_default_config_path", js(cli_tools_website_policy__get_default_config_path()));
  cli_tools_website_policy__invalidate_cache();
  return 0;
}
