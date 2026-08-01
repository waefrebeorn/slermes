/* Oracle harness: agent/agent_runtime_helpers.intent_ack_continuation_mode vs LIVE Python. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ported in src/agent/port_agent_intent_ack.c (no standalone header) */
void agent_intent_ack_continuation_mode(const char *mode, const char *api_mode,
                                        const char *model, char *out, size_t outsz);

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
    struct { const char *mode; const char *api_mode; const char *model; } c[] = {
        {"auto", "codex_responses", "gpt-4o"},
        {"auto", "openai", "gpt-4o"},
        {"true", "openai", "gpt-4o"},
        {"false", "codex_responses", "gpt-4o"},
        {"always", "openai", "gpt-4o"},
        {"never", "codex_responses", "gpt-4o"},
        {"on", "openai", "gpt-4o"},
        {"off", "codex_responses", "gpt-4o"},
        {"codex;gpt", "openai", "gpt-4o"},
        {"codex;claude", "openai", "gpt-4o"},
        {"claude;opus", "openai", "claude-opus-4"},
        {"", "openai", "gpt-4o"},
        {NULL, "openai", "gpt-4o"},
    };
    int n = sizeof(c)/sizeof(c[0]);
    for (int i = 0; i < n; i++) {
        char out[32];
        agent_intent_ack_continuation_mode(c[i].mode, c[i].api_mode, c[i].model, out, sizeof(out));
        printf("{\"mode\":%s,\"api_mode\":%s,\"model\":%s,\"out\":%s}\n",
               c[i].mode ? jstr(c[i].mode) : "null", jstr(c[i].api_mode), jstr(c[i].model), jstr(out));
    }
    return 0;
}
