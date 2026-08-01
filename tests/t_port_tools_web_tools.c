/* AUTO-GENERATED oracle harness for tools_web_tools (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_web_tools.c"

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
  web_ensure_web_plugins_loaded();
  printf("{\"func\":\"web_ddgs_package_importable\",\"ret\":0}\n", "web_ddgs_package_importable", (int)web_ddgs_package_importable());
  printf("{\"func\":\"web_get_extract_char_limit\",\"ret\":0}\n", "web_get_extract_char_limit", web_get_extract_char_limit());
  printf("{\"func\":\"web_check_web_api_key\",\"ret\":0}\n", "web_check_web_api_key", (int)web_check_web_api_key());
  return 0;
}
