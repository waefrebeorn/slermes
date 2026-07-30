/* Oracle harness: agent/reasoning_timeouts.py vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_reasoning_timeouts.h"

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
        "nvidia/nemotron-3-ultra-550b-a55b", "openai/o3-mini", "deepseek/deepseek-r1",
        "qwen/qwen3-235b-a22b-thinking", "x-ai/grok-4-fast-reasoning", "anthropic/claude-opus-4-6",
        "gpt-4o", "olmo-1", "openai/o1-preview", "anthropic/claude-sonnet-4.5",
        "grok-4-fast-non-reasoning", "qwen3-235b-instruct", "", NULL,
    };
    for (int i = 0; cases[i]; i++) {
        double f = agent_reasoning_timeouts_get_floor(cases[i]);
        if (f < 0) printf("{\"in\":%s,\"out\":null}\n", jstr(cases[i]));
        else printf("{\"in\":%s,\"out\":%g}\n", jstr(cases[i]), f);
    }
    printf("{\"in\":null,\"out\":null}\n");
    return 0;
}
