/* AUTO-GENERATED oracle harness for agent_delegation_context (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_agent_delegation_context.c"

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
  printf("{\"func\":\"delegated_child_context_enter\",\"ret\":0}\n", "delegated_child_context_enter", (int)delegated_child_context_enter());
  delegated_child_context_exit();
  printf("{\"func\":\"is_delegated_child_context\",\"ret\":0}\n", "is_delegated_child_context", (int)is_delegated_child_context());
  printf("{\"func\":\"is_delegated_child_process_context\",\"ret\":0}\n", "is_delegated_child_process_context", (int)is_delegated_child_process_context());
  printf("{\"func\":\"delegated_child_context_enter\",\"ret\":0}\n", "delegated_child_context_enter", (int)delegated_child_context_enter());
  delegated_child_context_exit();
  printf("{\"func\":\"is_delegated_child_context\",\"ret\":0}\n", "is_delegated_child_context", (int)is_delegated_child_context());
  printf("{\"func\":\"is_delegated_child_process_context\",\"ret\":0}\n", "is_delegated_child_process_context", (int)is_delegated_child_process_context());
  return 0;
}
