/* Oracle harness: cron/lifecycle_guard.contains_gateway_lifecycle_command vs LIVE Python. */
#include <stdio.h>
#include "cron/port_lifecycle_guard.h"

static const char *jstr(const char *s){
    static char b[8][1024]; static int bi=0; int idx=bi; char *q=b[idx]; bi=(bi+1)&7;
    *q++='"';
    for(const char *p=s;p&&*p&&(q-b[idx])<1000;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else *q++=c;
    }
    *q++='"';*q='\0';return b[idx];
}
int main(void) {
    const char *cases[] = {
        "hermes gateway restart",
        "hermes gateway stop now",
        "please just start the sibling profile gateway",
        "some prose about Kong API gateway autoscaling and restart behavior",
        "launchctl kickstart ai.hermes.gateway",
        "launchctl unload ai.hermes.gateway",
        "systemctl restart hermes-gateway",
        "systemctl --user stop hermes-gateway",
        "pkill -f hermes-gateway",
        "pkill -9 hermes gateway",
        "kill $(pgrep hermes) gateway",
        "hermes gateway start",          /* benign: start excluded */
        "just normal text",
        "",
        NULL,
    };
    for (int i = 0; cases[i]; i++) {
        int r = cron_lifecycle_contains_gateway_lifecycle_command(cases[i]) ? 1 : 0;
        printf("{\"text\":%s,\"out\":%d}\n", jstr(cases[i]), r);
    }
    return 0;
}
