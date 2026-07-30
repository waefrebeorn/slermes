/* Oracle harness: gateway/cgroup_cleanup._own_cgroup_path vs LIVE Python rule. */
#include <stdio.h>
#include "gateway/port_gateway_cgroup_cleanup.h"

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
        "0::/system.slice/foo.service\n",
        "11:cpu,cpuacct:/user.slice\n0::/user.slice/user-1000.slice/session.scope\n",
        "0::/docker/abc123\n0::/another\n",
        "no cgroup here\njust text\n",
        "",
        NULL,
    };
    for (int i = 0; cases[i]; i++) {
        char out[1024];
        int found = gateway_cgroup_cleanup_own_cgroup_path(cases[i], out, sizeof(out));
        if (!found) printf("{\"in\":%s,\"out\":null}\n", jstr(cases[i]));
        else printf("{\"in\":%s,\"out\":%s}\n", jstr(cases[i]), jstr(out));
    }
    return 0;
}
