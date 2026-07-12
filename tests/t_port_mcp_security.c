/* Oracle harness: hermes_cli/mcp_security.py vs LIVE Python. */
#include <stdio.h>
#include "cli/port_mcp_security.h"

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
    const char *cmds[] = {"", "  ", "npx", "/usr/bin/python3", "C:\\\\Windows\\\\python.exe",
                          "uvx run --foo", "bash -c 'echo hi'", "node server.js"};
    for (int i = 0; i < 8; i++) {
        char out[128]; hermes_cli_mcp_security_command_basename(cmds[i], out, sizeof(out));
        printf("{\"t\":\"basename\",\"cmd\":%s,\"out\":%s}\n", jstr(cmds[i]), jstr(out));
    }
    const char *args[] = {"", "echo hello", "a b c"};
    for (int i = 0; i < 3; i++) {
        char out[128]; hermes_cli_mcp_security_inline_script(args[i], out, sizeof(out));
        printf("{\"t\":\"inline\",\"args\":%s,\"out\":%s}\n", jstr(args[i]), jstr(out));
    }
    const char *entries[] = {
        "python\x1f-x\x1fSECRET=1",
        "node\x1fserver.js\x1fA=1\x1fB=2",
        "\x1f",
    };
    for (int i = 0; i < 3; i++) {
        char out[256]; hermes_cli_mcp_security_entry_text(entries[i], out, sizeof(out));
        printf("{\"t\":\"entry\",\"e\":%s,\"out\":%s}\n", jstr(entries[i]), jstr(out));
    }
    return 0;
}
