/* Oracle harness: agent/retry_utils.py vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_retry_utils.h"

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
    struct { int st; const char *base; const char *model; const char *text; } c[] = {
        {429, "https://api.z.ai/api/coding/paas/v4", "glm-5.2", "code 1305 the service may be temporarily overloaded"},
        {429, "https://api.z.ai/api/coding/paas/v4", "glm-5.2", "quota exceeded"},
        {429, "https://api.openai.com/v1", "glm-5.2", "code 1305 temporarily overloaded"},
        {429, "https://api.z.ai/api/coding/paas/v4", "gpt-4o", "code 1305 temporarily overloaded"},
        {200, "https://api.z.ai/api/coding/paas/v4", "glm-5.2", "code 1305 temporarily overloaded"},
        {429, "https://api.z.ai/api/coding/paas/v4", "glm-5.2", "temporarily overloaded please retry"},
        {500, "https://api.z.ai/api/coding/paas/v4", "glm-5.2", "temporarily overloaded"},
    };
    int n = sizeof(c)/sizeof(c[0]);
    for (int i = 0; i < n; i++) {
        retry_utils_err_t e; e.status_code = c[i].st;
        snprintf(e.text, sizeof(e.text), "%s", c[i].text);
        int r = agent_retry_utils_is_zai_coding_overload_error(c[i].base, c[i].model, &e) ? 1 : 0;
        printf("{\"status\":%d,\"base\":%s,\"model\":%s,\"text\":%s,\"out\":%d}\n",
               c[i].st, jstr(c[i].base), jstr(c[i].model), jstr(c[i].text), r);
    }
    return 0;
}
