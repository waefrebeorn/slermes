/*
 * t_port_agent_oneshot.c — faithful verification harness for
 * port_agent_oneshot.c:_strip_code_fence.
 * Emits JSON lines consumed by tests/sta_oracle_oneshot.py which recomputes
 * the SAME function from the LIVE agent/oneshot.py.
 */
#include "port_agent_oneshot.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *js(const char *s){
    static char b[4][8192];
    static int cur = 0;
    char *q = b[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    for(const char*p=s;*p&&(q-base)<7900;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else if(c=='\n'){*q++='\\';*q++='n';}
        else if(c=='\t'){*q++='\\';*q++='t';}
        else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}
        else *q++=c;
    }
    *q++='"'; *q=0; return base;
}

static void emit(const char *in){
    char *r = cli_agent_oneshot__strip_code_fence(in);
    printf("{\"fn\":\"strip_code_fence\",\"in\":%s,\"out\":%s}\n", js(in), js(r?r:""));
    free(r);
}

int main(void){
    emit("```\nhello\n```");
    emit("```python\nprint(1)\n```");
    emit("no fence here");
    emit("```");
    emit("```\nonly one line\nstill one line");
    emit("```\n  indented body  \n```");
    emit("```\ntrim\n```\n");   /* trailing newline after fence */
    emit("text with ``` inside but not fenced");
    emit("");
    return 0;
}
