/* Oracle harness: tools/url_safety.py:_allows_private_ip_resolution vs LIVE Python. */
#include <stdio.h>
#include "tools/port_url_safety_helpers.h"

static const char *jstr(const char *s){
    static char b[4][512]; static int bi=0; int idx=bi; char *q=b[idx]; bi=(bi+1)&3; *q++='"';
    for(const char *p=s;p&&*p&&(q-b[idx])<500;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else *q++=c;
    }
    *q++='"';*q='\0';return b[idx];
}
int main(void) {
    struct { const char *scheme; const char *host; } c[] = {
        {"https", "multimedia.nt.qq.com.cn"},
        {"https", "MULTIMEDIA.NT.QQ.COM.CN"},
        {"http", "multimedia.nt.qq.com.cn"},
        {"https", "evil.example.com"},
        {"https", ""},
        {"", "multimedia.nt.qq.com.cn"},
        {NULL, "multimedia.nt.qq.com.cn"},
    };
    int n = sizeof(c)/sizeof(c[0]);
    for (int i = 0; i < n; i++) {
        int r = tools_url_safety_allows_private_ip_resolution(c[i].host, c[i].scheme) ? 1 : 0;
        printf("{\"scheme\":%s,\"host\":%s,\"out\":%d}\n",
               c[i].scheme ? jstr(c[i].scheme) : "null", c[i].host ? jstr(c[i].host) : "null", r);
    }
    return 0;
}
