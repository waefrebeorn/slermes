/* AUTO-GENERATED oracle harness for tools_kanban_tools (gen_oracle.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_kanban_tools.c"

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
  printf("{\"func\":\"profile_has_kanban_toolset\",\"ret\":0}\n", "profile_has_kanban_toolset", (int)profile_has_kanban_toolset());
  printf("{\"func\":\"check_kanban_mode\",\"ret\":0}\n", "check_kanban_mode", (int)check_kanban_mode());
  printf("{\"func\":\"check_kanban_orchestrator_mode\",\"ret\":0}\n", "check_kanban_orchestrator_mode", (int)check_kanban_orchestrator_mode());
  printf("{\"func\":\"goal_judge_available\",\"ret\":0}\n", "goal_judge_available", (int)goal_judge_available());
  return 0;
}
