/*
 * t_port_display_shell.c — faithful verification harness for
 * port_agent_display.c shell-summarization helpers.
 * Emits JSON lines consumed by tests/sta_oracle_display_shell.py which
 * recomputes the SAME functions from LIVE agent/display.py.
 */
#include "port_agent_display.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *js(const char *s){
    static char b[6][8192];
    static int cur = 0;
    char *q = b[cur];
    cur = (cur + 1) % 6;
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

static void emit_words(const char *fn, const char *in){
    int n=0; char **w = cli_agent_display__split_shell_words(in, &n);
    printf("{\"fn\":%s,\"in\":%s,\"out\":[", js(fn), js(in));
    for(int i=0;i<n;i++){ if(i)printf(","); printf("%s", js(w[i])); }
    printf("]}\n");
    for(int i=0;i<n;i++) free(w[i]); free(w);
}
static void emit_str(const char *fn, const char *in){
    char *r = NULL;
    if (strcmp(fn,"strip_pipe")==0) r = cli_agent_display__strip_shell_pipe_tail(in);
    else if (strcmp(fn,"clean_seg")==0) r = cli_agent_display__clean_shell_segment(in);
    printf("{\"fn\":%s,\"in\":%s,\"out\":%s}\n", js(fn), js(in), js(r?r:""));
    free(r);
}
static void emit_compound(const char *in){
    int n=0; char **w = cli_agent_display__split_shell_compound(in, &n);
    printf("{\"fn\":\"compound\",\"in\":%s,\"out\":[", js(in));
    for(int i=0;i<n;i++){ if(i)printf(","); printf("%s", js(w[i])); }
    printf("]}\n");
    for(int i=0;i<n;i++) free(w[i]); free(w);
}
static void emit_bool(const char *fn, const char *in){
    int r = cli_agent_display__is_shell_boundary_echo(in);
    printf("{\"fn\":%s,\"in\":%s,\"out\":%d}\n", js(fn), js(in), r?1:0);
}

int main(void){
    emit_words("split_words", "foo bar 'baz qux'");
    emit_words("split_words", "a   b\tc");
    emit_words("split_words", "git commit -m \"hello world\"");
    emit_words("split_words", "x='a b' y");
    emit_str("strip_pipe", "cat foo | head -n 5");
    emit_str("strip_pipe", "grep x file | sort");
    emit_str("strip_pipe", "ls -la");
    emit_str("clean_seg", "cat foo > /tmp/x 2>&1");
    emit_str("clean_seg", "prog 2>&1 | tee log");
    emit_str("clean_seg", "echo hi < input");
    emit_compound("cd src && make && echo done");
    emit_compound("ls; cat a | head; echo x");
    emit_compound("a || b && c");
    emit_bool("boundary_echo", "echo --signal-int");
    emit_bool("boundary_echo", "echo _exit=1");
    emit_bool("boundary_echo", "echo $?");
    emit_bool("boundary_echo", "echo PIPESTATUS");
    emit_bool("boundary_echo", "echo hello");
    emit_bool("boundary_echo", "ls -la");
    return 0;
}
