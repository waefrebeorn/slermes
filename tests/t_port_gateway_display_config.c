/*
 * t_port_gateway_display_config.c — harness emitting JSON lines for the oracle.
 * Declares normalise_display_value directly (lives in src/gateway/helpers.c,
 * linked separately) to avoid the hermes_json/libjson header clash.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *normalise_display_value(const char *setting, const char *value);

static const char *js(const char *s){
    static char b[4][2048];
    static int cur = 0;
    char *q = b[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    for(const char*p=s;*p&&(q-base)<1900;p++){
        unsigned char c=*p;
        if(c=='"'||c=='\\'){*q++='\\';*q++=c;}
        else if(c=='\n'){*q++='\\';*q++='n';}
        else if(c=='\t'){*q++='\\';*q++='t';}
        else if(c<' '){*q++='\\';*q++='u';*q++='0';*q++='0';*q++=(c>15?'1':'0');*q++=(c%16<10?c%16+'0':c%16-10+'a');}
        else *q++=c;
    }
    *q++='"'; *q=0; return base;
}
static void emit(const char *setting, const char *value) {
    char *out = normalise_display_value(setting, value);
    printf("{\"setting\":%s,\"value\":%s,\"out\":%s}\n",
           js(setting), js(value), js(out ? out : ""));
    free(out);
}
int main(void) {
    emit("tool_progress", "False");
    emit("tool_progress", "True");
    emit("tool_progress", "ALL");
    emit("show_reasoning", "yes");
    emit("show_reasoning", "off");
    emit("streaming", "1");
    emit("interim_assistant_messages", "no");
    emit("long_running_notifications", "ON");
    emit("busy_ack_detail", "FALSE");
    emit("tool_progress_grouping", "separate");
    emit("tool_progress_grouping", "weird");
    emit("reasoning_style", "blockquote");
    emit("reasoning_style", "xyz");
    emit("tool_preview_length", "120");
    emit("tool_preview_length", "abc");
    emit("some_other", "KEEPme");
    return 0;
}
