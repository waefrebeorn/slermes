/*
 * t_port_tool_result_storage_filename.c — faithful verification harness for
 * port_tools_tool_result_storage.c:_safe_result_filename.
 * Emits JSON lines consumed by tests/sta_oracle_safe_result_filename.py,
 * which recomputes the SAME function from the LIVE tools/tool_result_storage.py
 * and asserts exact equality (filename string).
 */
#include "port_tools_tool_result_storage.c"
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

static void emit(const char *id){
    char *got = cli_tools_tool_result_storage__safe_result_filename(id);
    printf("{\"fn\":\"safe_result_filename\",\"id\":%s,\"out\":%s}\n",
           js(id ? id : ""), js(got ? got : ""));
    free(got);
}

int main(void){
    /* clean id -> unchanged stem + .txt */
    emit("abc123");
    /* None -> tool_result */
    emit(NULL);
    /* unsafe chars replaced with _ */
    emit("weird/id:with*chars");
    /* leading/trailing ._- stripped */
    emit("..bad--name__");
    /* after strip empty -> tool_result (changed=True) -> digest form */
    emit("...___");
    /* long stem > 120 -> digest form */
    char longid[200];
    for (int i=0;i<180;i++) longid[i] = (i%2)?'a':'/';
    longid[180]=0;
    emit(longid);
    /* truncation at 120 then rstrip, but still changed (had /) -> digest */
    char midid[200];
    for (int i=0;i<130;i++) midid[i] = (i<60)?'A':'.';
    midid[130]=0;
    emit(midid);
    /* no change expected: clean short */
    emit("tool_use_xyz");
    return 0;
}
