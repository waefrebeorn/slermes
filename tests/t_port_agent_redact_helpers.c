/* Oracle harness: agent/redact.py helper subset. One JSON object per line. */
#include <stdio.h>
#include <stdlib.h>
#include "agent/port_agent_redact_helpers.h"

static const char *js(const char *s) {
    static char bufs[8][4096];
    static int bi = 0;
    char *b = bufs[bi]; bi = (bi + 1) % 8;
    char *q = b; *q++ = '"';
    for (const char *p = s; p && *p && q - b < 4000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"'; *q = '\0';
    return b;
}

int main(void) {
    const char *toks[] = {"", "ghp_abcdefghij12345678", "sk-ant-abcdefghij", "randomstring", "AKIAIOSFODNN7EXAMPLE", NULL};
    for (int i = 0; toks[i]; i++) {
        char *m = agent_redact_mask_token_nonreusable(toks[i]);
        printf("{\"t\":\"mask\",\"in\":%s,\"out\":%s}\n", js(toks[i]), js(m));
        free(m);
    }
    const char *cmds[] = {"env", "printenv PATH", "ls -la && export FOO=1", "set | grep x",
                          "git status", "echo hi", "declare -x BAR", "cat file", NULL};
    for (int i = 0; cmds[i]; i++)
        printf("{\"t\":\"envdump\",\"in\":%s,\"out\":%d}\n", js(cmds[i]),
               agent_redact_is_env_dump_command(cmds[i])?1:0);
    return 0;
}
