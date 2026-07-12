/* Oracle harness: agent/display.py pure helpers vs LIVE Python. */
#include <stdio.h>
#include "agent/port_agent_display_helpers.h"

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
    /* _oneline cases */
    const char *ones[] = {"hello   world", "a\nb\tc", "  spaced  out  ", "nochange", "", NULL};
    for (int i = 0; ones[i]; i++) {
        char out[512]; agent_display_oneline(ones[i], out, sizeof(out));
        printf("{\"t\":\"oneline\",\"in\":%s,\"out\":%s}\n", jstr(ones[i]), jstr(out));
    }
    /* _truncate_preview cases: text, max_len */
    struct { const char *t; int m; } tr[] = {
        {"abcdefghij", 0}, {"abcdefghij", 5}, {"abcdefghij", 3}, {"abc", 10}, {"abcdefghij", 4},
    };
    int tn = sizeof(tr)/sizeof(tr[0]);
    for (int i = 0; i < tn; i++) {
        char out[512]; agent_display_truncate_preview(tr[i].t, tr[i].m, out, sizeof(out));
        printf("{\"t\":\"trunc\",\"in\":%s,\"m\":%d,\"out\":%s}\n", jstr(tr[i].t), tr[i].m, jstr(out));
    }
    /* _shell_basename */
    const char *sh[] = {"/usr/bin/python3", "python3", "a/b/c/", "", NULL};
    for (int i = 0; sh[i]; i++) {
        char out[512]; agent_display_shell_basename(sh[i], out, sizeof(out));
        printf("{\"t\":\"base\",\"in\":%s,\"out\":%s}\n", jstr(sh[i]), jstr(out));
    }
    return 0;
}
