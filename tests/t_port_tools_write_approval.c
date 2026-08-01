/* AUTO-GENERATED oracle harness for tools_write_approval (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_tools_write_approval.c"

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
  printf("{\"func\":\"cli_tools_write_approval_is_background\",\"ret\":0}\n", "cli_tools_write_approval_is_background", cli_tools_write_approval_is_background());
  printf("{\"func\":\"cli_tools_write_approval__interactive_approval_available\",\"ret\":0}\n", "cli_tools_write_approval__interactive_approval_available", cli_tools_write_approval__interactive_approval_available());
  return 0;
}
