/* gate: environments_ssh_build_ssh_command — traced copy of
 * src/hermes_cli/environments_ssh.c::cli_tools_environments_ssh__build_ssh_command
 * (hermes_log stubbed; body verbatim). LIVE twin drives the REAL
 * tools/environments/ssh.py::SSHEnvironment._build_ssh_command via an
 * uninitialized instance (the constructor opens a live SSH connection).
 *
 * Input argv: "<user>\t<host>\t<port>\t<key_path>\t<control_socket>".
 * Branch ladder (must match twin.py EXACTLY):
 *   B1: multiplex enabled (compile-time True on POSIX)
 *   B2: port != 22        B3: key_path non-empty
 *   VSTR(space-joined command)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "watercup.h"

static void decode_escapes(char *s) {
    char *o = s;
    for (char *p = s; *p; p++) {
        if (p[0] == '\\' && p[1] == 't') { *o++ = '\t'; p++; }
        else *o++ = *p;
    }
    *o = '\0';
}

/* verbatim port body minus hermes_log */
static int build_ssh_command(char *buf, size_t bufsz,
                             const char *user, const char *host,
                             int port, const char *key_path,
                             const char *control_socket) {
    if (!buf || bufsz == 0) return -1;
    int written = snprintf(buf, bufsz,
        "ssh -o ControlPath=%s -o ControlMaster=auto -o ControlPersist=300 "
        "-o BatchMode=yes -o StrictHostKeyChecking=accept-new -o ConnectTimeout=10",
        control_socket ? control_socket : "~/.ssh/control-%h-%p-%r");
    if (port != 22 && written > 0 && (size_t)written < bufsz) {
        written += snprintf(buf + written, bufsz - written, " -p %d", port);
    }
    if (key_path && key_path[0] && (size_t)written < bufsz) {
        written += snprintf(buf + written, bufsz - written, " -i %s", key_path);
    }
    if ((size_t)written < bufsz) {
        written += snprintf(buf + written, bufsz - written, " %s@%s", user, host);
    }
    return written;
}

static void run_one(const char *input) {
    char buf[1024];
    snprintf(buf, sizeof buf, "%s", input);
    decode_escapes(buf);
    char *f[5] = {"", "", "22", "", ""};
    int fi = 0;
    char *tok = buf;
    for (fi = 0; fi < 5; fi++) {
        f[fi] = tok;
        char *nt = strchr(tok, '\t');
        if (!nt) break;
        *nt = '\0';
        tok = nt + 1;
    }
    const char *user = f[0], *host = f[1], *key = f[3], *csock = f[4];
    int port = atoi(f[2]);

    WCUP_BEGIN("_build_ssh_command");
    WCUP_BRANCH(true);                          /* B1: _SSH_MULTIPLEX on POSIX */
    WCUP_BRANCH(port != 22);                    /* B2 */
    WCUP_BRANCH(key && key[0]);                 /* B3 */

    char out[2048];
    build_ssh_command(out, sizeof out, user, host, port, key, csock);
    WCUP_VALSTR(out);
    char cup[32];
    WCUP_FINALIZE(cup, sizeof cup);
    printf("%s\n", cup);
    WCUP_END("_build_ssh_command");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <fn> <user\\thost\\tport\\tkey\\tsock...>\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) run_one(argv[i]);
    return 0;
}
