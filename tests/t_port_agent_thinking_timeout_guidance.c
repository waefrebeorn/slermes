/* Oracle harness: agent/thinking_timeout_guidance.py vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_thinking_timeout_guidance.h"

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
    struct { const char *reason; const char *model; const char *err; } c[] = {
        {"timeout", "openai/o3-mini", "broken pipe"},
        {"timeout", "openai/o3-mini", "http 429 rate limit"},
        {"timeout", "gpt-4o", "broken pipe"},
        {"context_overflow", "openai/o3-mini", "broken pipe"},
        {"timeout", "anthropic/claude-opus-4-6", "errno 32 connection reset"},
        {"timeout", "deepseek/deepseek-r1", "server disconnected peer closed"},
        {"timeout", "qwen/qwen3-235b-thinking", "remote protocol error"},
        {"timeout", "x-ai/grok-4-fast-non-reasoning", "broken pipe"},
    };
    int n = sizeof(c)/sizeof(c[0]);
    for (int i = 0; i < n; i++) {
        int r = agent_thinking_timeout_is_thinking_timeout(c[i].reason, c[i].model, c[i].err) ? 1 : 0;
        printf("{\"reason\":%s,\"model\":%s,\"err\":%s,\"out\":%d}\n",
               jstr(c[i].reason), jstr(c[i].model), jstr(c[i].err), r);
    }
    return 0;
}
