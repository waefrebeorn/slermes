/* AUTO-GENERATED oracle harness for tools_browser_camofox (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_browser_camofox.c"

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
  printf("{\"func\":\"check_camofox_available\",\"ret\":0}\n", "check_camofox_available", (int)check_camofox_available());
  printf("{\"func\":\"_managed_persistence_enabled\",\"ret\":0}\n", "_managed_persistence_enabled", (int)_managed_persistence_enabled());
  printf("{\"func\":\"_adopt_existing_tab_enabled\",\"ret\":0}\n", "_adopt_existing_tab_enabled", (int)_adopt_existing_tab_enabled());
  printf("{\"func\":\"_loopback_rewrite_enabled\",\"ret\":0}\n", "_loopback_rewrite_enabled", (int)_loopback_rewrite_enabled());
  return 0;
}
