/*
 * t_port_xai_expires.c — faithful verification harness for
 * port_tools_xai_http.c:_coerce_expires_after.
 * Emits JSON lines consumed by tests/sta_oracle_xai_expires.py which
 * recomputes the SAME function from LIVE tools/xai_http.py.
 */
#include "port_tools_xai_http.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *js2(const char *s){
    static char b[4096]; char *q=b; *q++='"';
    for(const char*p=s;*p&&(q-b)<3900;p++){unsigned char c=*p; if(c=='"'||c=='\\'){*q++='\\';*q++=c;} else *q++=c;}
    *q++='"'; *q=0; return b;
}

static void emit(const char *v){
    char *r = cli_tools_xai_http__coerce_expires_after(v);
    static char inb[4096];
    char *q = inb; *q++='"';
    if (v) for(const char*p=v;*p&&(q-inb)<3900;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else *q++=c;
    }
    *q++='"'; *q=0;
    printf("{\"fn\":\"coerce_expires\",\"in\":%s,\"out\":%s}\n", inb, js2(r?r:""));
    free(r);
}

int main(void){
    emit(NULL);
    emit("");
    emit("default");
    emit("none");
    emit("null");
    emit("never");
    emit("permanent");
    emit("forever");
    emit("0");
    emit("3600");
    emit("99999999");   /* > max -> clamped to 2592000 */
    emit("-5");         /* <=0 -> null */
    emit("garbage");    /* -> SAFE 172800 */
    emit("  172800  "); /* whitespace + valid int */
    return 0;
}
