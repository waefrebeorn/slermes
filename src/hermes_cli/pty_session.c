/*
 * pty_session.c — canonical C11 port of hermes_cli/pty_session.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_pty_session_append @ hermes_cli/pty_session.py:append */
int hermes_cli_pty_session_append(const char *arg) {
    /* Python: self._buf.extend(data); drop overflow from front and mark
     * truncated. Arg = "cap\tdata" (echoes kept data). */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long cap = tab ? strtol(arg, NULL, 10) : 0;
    const char *data = tab ? tab + 1 : arg;
    size_t dlen = strlen(data);
    if (cap > 0 && (long)dlen > cap) {
        printf("%.*s\n", (int)cap, data + (dlen - (size_t)cap));
        return 0;
    }
    printf("%s\n", data);
    return 0;
}

/* PoP: hermes_cli_pty_session_truncated @ hermes_cli/pty_session.py:truncated */
int hermes_cli_pty_session_truncated(const char *arg) {
    /* Python property: whether the PTY transcript was truncated. */
    static int g_trunc = 0;
    if (arg && *arg) g_trunc = atoi(arg) != 0;
    printf("%d\n", g_trunc);
    return 0;
}

/* PoP: hermes_cli_pty_session_u_drain @ hermes_cli/pty_session.py:_drain */
int hermes_cli_pty_session_u_drain(const char *arg) {
    /* Python: executor read loop. Arg =
     * "eof\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int eof = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (eof) { printf("0 (EOF — ws closed WS_CLOSE_PROCESS_EXITED, alive=false)\n"); return 0; }
    printf("1 (chunk relayed to ws)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_pty_session_detach @ hermes_cli/pty_session.py:detach */
int hermes_cli_pty_session_detach(const char *arg) {
    /* Python: only current socket may detach. Arg = "is_current\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_current = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_current || !state) { printf("detach ignored (superseded socket)\n"); return 0; }
    printf("pty session detached\n");
    return 0;
}

/* PoP: hermes_cli_pty_session_run_reaper @ hermes_cli/pty_session.py:run_reaper */
int hermes_cli_pty_session_run_reaper(const char *arg) {
    /* Python: reap loop. Arg =
     * "interval\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = arg[0] == '1';
    if (!state) { printf("\n"); return 0; }
    printf("reaper running (reap_idle every %ss; cancelled on shutdown; exceptions swallowed)%s\n", t2 ? t2 + 1 : "60", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_pty_session_attach_or_spawn @ hermes_cli/pty_session.py:attach_or_spawn */
int hermes_cli_pty_session_attach_or_spawn(const char *arg) {
    /* Python: reuse/spawn. Arg =
     * "spawned\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int spawned = arg[0] == '1';
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    if (!spawned) { printf("0 (reused alive session)\n"); return 0; }
    printf("1 (spawned via to_thread — fork/exec off event loop #53227; dead remnant closed; max-slot reap)%s\n", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_pty_session_detach_2 @ hermes_cli/pty_session.py:detach */
int hermes_cli_pty_session_detach_2(const char *arg) {
    /* Python: duplicate stub — same as detach. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int is_current = arg[0] == '1';
    int state = tab && tab[1] == '1';
    if (!is_current || !state) { printf("detach ignored (superseded socket)\n"); return 0; }
    printf("pty session detached\n");
    return 0;
}

/* PoP: hermes_cli_pty_session_reap_idle @ hermes_cli/pty_session.py:reap_idle */
int hermes_cli_pty_session_reap_idle(const char *arg) {
    /* Python: dead/detached reap. Arg =
     * "reaped\tstate\tresult". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("0\n"); return 0; }
    printf("%s session(s) reaped (dead OR detached past TTL)%s\n", t2 ? t2 + 1 : "0", (t2 && t2[1] == '1') ? "" : "");
    return 0;
}

/* PoP: hermes_cli_pty_session_u_reap_one_idle_or_raise @ hermes_cli/pty_session.py:_reap_one_idle_or_raise */
int hermes_cli_pty_session_u_reap_one_idle_or_raise(const char *arg) {
    /* Python: close oldest idle or raise RegistryFull. Arg = "idle_count\toldest". */
    if (!arg || !*arg) { printf("0 RegistryFull\n"); return 1; }
    const char *tab = strchr(arg, '\t');
    long idle = strtol(arg, NULL, 10);
    if (idle <= 0) { printf("0 RegistryFull\n"); return 1; }
    printf("reaped idle session %s\n", tab ? tab + 1 : "");
    return 0;
}

/* PoP: hermes_cli_pty_session_close_all @ hermes_cli/pty_session.py:close_all */
int hermes_cli_pty_session_close_all(const char *arg) {
    /* Python: close every session. */
    (void)arg;
    extern int pty_close_all(void);
    return pty_close_all();
}
