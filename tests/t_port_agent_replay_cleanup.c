/* Oracle harness: agent/replay_cleanup.py vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_replay_cleanup.h"

static const char *jstr(const char *s){
    static char b[4][1024]; static int bi=0; int idx=bi; char *q=b[idx]; bi=(bi+1)&3; *q++='"';
    for(const char *p=s;p&&*p&&(q-b[idx])<1000;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else if(c<0x20){*q++='\\';*q++='u';*q++='0';*q++='0';*q++="0123456789abcdef"[c>>4];*q++="0123456789abcdef"[c&0xf];}
        else *q++=c;
    }
    *q++='"';*q='\0';return b[idx];
}
int main(void) {
    const char *cases[] = {
        "[command interrupted]", "exit_code 130", "exit_code: -1, interrupt received",
        "exit_code 137", "some normal output", "exit_code 0 success", "", NULL,
    };
    for (int i = 0; cases[i]; i++) {
        int r = agent_replay_cleanup_is_interrupted_tool_result(cases[i]) ? 1 : 0;
        printf("{\"in\":%s,\"out\":%d}\n", jstr(cases[i]), r);
    }
    printf("{\"in\":null,\"out\":0}\n");
    return 0;
}
