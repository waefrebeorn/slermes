/* AUTO-GENERATED oracle harness for tools_approval (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_approval.c"

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
  printf("{\"func\":\"is_yolo_mode_frozen\",\"ret\":0}\n", "is_yolo_mode_frozen", (int)is_yolo_mode_frozen());
  printf("{\"func\":\"is_approval_bypass_active\",\"ret\":0}\n", "is_approval_bypass_active", (int)is_approval_bypass_active());
  printf("{\"func\":\"_is_interactive_cli\",\"ret\":0}\n", "_is_interactive_cli", (int)_is_interactive_cli());
  approval_reset_current_session_key();
  approval_reset_current_observability_context();
  printf("{\"func\":\"approval__is_gateway_approval_context\",\"ret\":0}\n", "approval__is_gateway_approval_context", approval__is_gateway_approval_context());
  approval_disable_session_yolo();
  printf("{\"func\":\"approval_is_current_session_yolo_enabled\",\"ret\":0}\n", "approval_is_current_session_yolo_enabled", approval_is_current_session_yolo_enabled());
  approval_load_permanent();
  approval_unregister_gateway_notify();
  return 0;
}
