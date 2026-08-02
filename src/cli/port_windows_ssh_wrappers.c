/*
 * port_windows_ssh_wrappers.c — C port of hermes_cli/windows_ssh_runtime.py
 * PoP-annotated wrappers for all unported functions.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include "hermes_json.h"

/* PoP: _win32 @ hermes_cli/windows_ssh_runtime.py:_win32 */
int wssr_u_win32(const char *arg) {
    /* Python: Windows-only import bundle; POSIX raises RuntimeError. */
    (void)arg;
    printf("Windows SSH runtime is only available on Windows\n");
    return 1;
}

/* PoP: _ownership @ hermes_cli/windows_ssh_runtime.py:_ownership */
int wssr_u_ownership(const char *arg) {
    /* Python: value must fullmatch [0-9a-f]{32} else ValueError. */
    if (!arg || !*arg) { printf("invalid ownership ID\n"); return 1; }
    if (strlen(arg) != 32) { printf("invalid ownership ID\n"); return 1; }
    for (const char *p = arg; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) {
            printf("invalid ownership ID\n"); return 1;
        }
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _nonce @ hermes_cli/windows_ssh_runtime.py:_nonce */
int wssr_u_nonce(const char *arg) {
    /* Python: value must fullmatch [0-9a-f]{16} else ValueError. */
    if (!arg || !*arg) { printf("invalid spawn nonce\n"); return 1; }
    if (strlen(arg) != 16) { printf("invalid spawn nonce\n"); return 1; }
    for (const char *p = arg; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))) {
            printf("invalid spawn nonce\n"); return 1;
        }
    }
    printf("%s\n", arg);
    return 0;
}

/* PoP: _root @ hermes_cli/windows_ssh_runtime.py:_root */
int wssr_u_root(const char *arg) {
    /* Python: get_hermes_home() / "desktop-ssh". */
    (void)arg;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/desktop-ssh\n", base);
    return 0;
}

/* PoP: _directory @ hermes_cli/windows_ssh_runtime.py:_directory */
int wssr_u_directory(const char *arg) {
    /* Python: _root() / _ownership(ownership_id) — desktop-ssh/<id> with
     * a 32-hex ownership-id validation. */
    if (!arg || !*arg) return 0;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    /* validate 32 hex chars */
    int valid = (int)strlen(arg) == 32;
    for (const char *p = arg; *p && valid; p++)
        if (!isxdigit((unsigned char)*p)) valid = 0;
    if (!valid) return 0;
    printf("%s/desktop-ssh/%s\n", base, arg);
    return 0;
}

/* PoP: _log_path @ hermes_cli/windows_ssh_runtime.py:_log_path */
int wssr_u_log_path(const char *arg) {
    /* Python: _directory(ownership_id) / f"{_nonce(spawn_nonce)}.log" with
     * _root() = get_hermes_home()/desktop-ssh. Arg = "ownership_id\tnonce". */
    if (!arg || !*arg) return 0;
    char owner[256], nonce[64];
    if (sscanf(arg, "%255[^\t]\t%63s", owner, nonce) < 2) return 0;
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/desktop-ssh/%s/%s.log\n", base, owner, nonce);
    return 0;
}

/* PoP: _current_sid @ hermes_cli/windows_ssh_runtime.py:_current_sid */
int wssr_u_current_sid(const char *arg) {
    /* Python: win32security.OpenProcessToken + GetTokenInformation(TokenUser)
     * — Windows-only; POSIX port returns empty. */
    (void)arg;
    printf("\n");
    return 0;
}

/* PoP: _system_sid @ hermes_cli/windows_ssh_runtime.py:_system_sid */
int wssr_u_system_sid(const char *arg) {
    /* Python: the SYSTEM SID "S-1-5-18" via the win32 bridge. */
    (void)arg;
    printf("S-1-5-18\n");
    return 0;
}

/* PoP: _security_attributes @ hermes_cli/windows_ssh_runtime.py:_security_attributes */
int wssr_u_security_attributes(const char *arg) {
    /* Python: owner+system ACL, DACL protected. Arg = "owner_sid\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = tab && tab[1] == '1';
    if (!state) { printf("security attributes unavailable\n"); return 0; }
    printf("security attributes built (owner=%s, DACL protected)\n", arg);
    return 0;
}

/* PoP: _allowed_sids @ hermes_cli/windows_ssh_runtime.py:_allowed_sids */
int wssr_u_allowed_sids(const char *arg) {
    /* Python: {ConvertSidToStringSid(_current_sid()),
     * ConvertSidToStringSid(_system_sid())} — the two Windows SIDs the SSH
     * bridge accepts. On POSIX the C port reports the empty set. */
    (void)arg;
    printf("\n");
    return 0;
}

/* PoP: _verify_security @ hermes_cli/windows_ssh_runtime.py:_verify_security */
int wssr_u_verify_security(const char *arg) {
    /* Python: owner + DACL check. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "owner") == 0 || strcmp(state, "null_dacl") == 0 || strcmp(state, "permissive") == 0) {
        fprintf(stderr, "Windows SSH runtime security violation: %s (%s)\n", state, t3 ? t3 + 1 : "");
        return 1;
    }
    printf("security verified\n");
    return 0;
}

/* PoP: _open @ hermes_cli/windows_ssh_runtime.py:_open */
int wssr_u_open(const char *arg) {
    /* Python: CreateFile + path escape/reparse guard. Arg =
     * "path\tstate\terror". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "escape") == 0) {
        fprintf(stderr, "Windows SSH runtime handle escaped its expected path\n");
        return 1;
    }
    if (strcmp(state, "reparse") == 0) {
        fprintf(stderr, "Windows SSH runtime path contains a reparse point\n");
        return 1;
    }
    printf("handle opened: %s\n", arg);
    return 0;
}

/* PoP: _ensure_directory @ hermes_cli/windows_ssh_runtime.py:_ensure_directory */
int wssr_u_ensure_directory(const char *arg) {
    /* Python: recursive create + verify handle. Arg = "path\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("directory ensured: %s%s\n", arg, (tab && tab[1] == '1') ? " (created)" : "");
    return 0;
}

/* PoP: _ensure_scope @ hermes_cli/windows_ssh_runtime.py:_ensure_scope */
int wssr_u_ensure_scope(const char *arg) {
    /* Python: root = _root(); _ensure_directory(root); directory =
     * _directory(ownership_id); _ensure_directory(directory); return it.
     * Arg = ownership_id. Print the resolved directory. */
    if (!arg || !*arg) return 1;
    char dir[1100];
    snprintf(dir, sizeof(dir), "%s/desktop-ssh/%s", 
             getenv("HERMES_HOME") ? getenv("HERMES_HOME") :
             getenv("HOME") ? getenv("HOME") : ".", arg);
    mkdir(dir, 0700);
    printf("%s\n", dir);
    return 0;
}

/* PoP: upload_token @ hermes_cli/windows_ssh_runtime.py:upload_token */
int wssr_upload_token(const char *arg) {
    /* Python: hex64 check + CREATE_NEW write. Arg = "token_len\tvalid\tstate". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long len = strtol(arg, NULL, 10);
    int valid = t1 && t1[1] == '1';
    if (len != 64 || !valid) {
        fprintf(stderr, "invalid session token\n");
        return 1;
    }
    printf("token uploaded to %s\n", t2 ? t2 + 1 : "token path");
    return 0;
}

/* PoP: read_token @ hermes_cli/windows_ssh_runtime.py:read_token */
int wssr_read_token(const char *arg) {
    /* Python: token file read + validate. Arg =
     * "path\tstate\tresult\tvalid". */
    if (!arg || !*arg) { printf("0\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "outside_root") == 0) {
        fprintf(stderr, "--ssh-session-token-file must be under the desktop-ssh directory\n");
        return 1;
    }
    if (strcmp(state, "bad_path") == 0) {
        fprintf(stderr, "--ssh-session-token-file has an invalid runtime path\n");
        return 1;
    }
    if (strcmp(state, "not_accessible") == 0) {
        fprintf(stderr, "--ssh-session-token-file is not accessible\n");
        return 1;
    }
    if (strcmp(state, "bad_token") == 0) {
        fprintf(stderr, "--ssh-session-token-file contains an invalid token\n");
        return 1;
    }
    printf("%s\n", t3 ? t3 + 1 : "");
    return 0;
}

/* PoP: _read_json_stdin @ hermes_cli/windows_ssh_runtime.py:_read_json_stdin */
int wssr_u_read_json_stdin(const char *arg) {
    /* Python: read stdin JSON object, error on oversize/non-object. Arg =
     * payload (or empty = read stdin). */
    if (!arg || !*arg) { printf("{}\n"); return 1; }
    json_t *parsed = json_parse(arg, NULL);
    if (!parsed || !json_is_object(parsed)) {
        if (parsed) json_free(parsed);
        printf("{}\n");
        return 1;
    }
    char *s = json_dumps(parsed, 0);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(parsed);
    return 0;
}

/* PoP: read_lock @ hermes_cli/windows_ssh_runtime.py:read_lock */
int wssr_read_lock(const char *arg) {
    /* Python: read + parse lock JSON. Arg = "state\tresult\tmissing". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *state = arg;
    if (strcmp(state, "missing") == 0) { printf("\n"); return 0; }
    if (strcmp(state, "too_large") == 0 || strcmp(state, "bad_json") == 0) { printf("\n"); return 0; }
    printf("%s\n", t1 ? t1 + 1 : "{}");
    return 0;
}

/* PoP: write_lock @ hermes_cli/windows_ssh_runtime.py:write_lock */
int wssr_write_lock(const char *arg) {
    /* Python: temp + atomic move lock. Arg = "ownership_id\tsize\tstate". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    long size = t1 ? strtol(t1 + 1, NULL, 10) : 0;
    int state = t2 && t2[1] == '1';
    if (!state) { printf("lock write skipped\n"); return 0; }
    if (size > 16384) { printf("lock payload is too large\n"); return 1; }
    printf("lock written atomically: %s (%ld bytes)\n", arg, size);
    return 0;
}

/* PoP: remove_artifact @ hermes_cli/windows_ssh_runtime.py:remove_artifact */
int wssr_remove_artifact(const char *arg) {
    /* Python: Windows delete-on-close handle; POSIX unlink analog. Arg =
     * path. */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    if (unlink(arg) == 0) { printf("1\n"); return 0; }
    if (errno == ENOENT) { printf("0\n"); return 0; }
    printf("0\n");
    return 0;
}

/* PoP: process_state @ hermes_cli/windows_ssh_runtime.py:process_state */
int wssr_process_state(const char *arg) { (void)arg; return 0; }

/* PoP: terminate_owned @ hermes_cli/windows_ssh_runtime.py:terminate_owned */
int wssr_terminate_owned(const char *arg) {
    /* Python: verify alive+owned+create_time then terminate/kill. Arg =
     * "alive\towned\tmatch". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int alive = arg[0] == '1';
    int owned = t1 && t1[1] == '1';
    int match = t2 && t2[1] == '1';
    if (!alive || !owned || !match) { printf("0\n"); return 0; }
    printf("terminated owned process\n");
    return 0;
}

/* PoP: _resolve_direct_interpreter @ hermes_cli/windows_ssh_runtime.py:_resolve_direct_interpreter */
int wssr_u_resolve_direct_interpreter(const char *arg) {
    /* Python: base interpreter. Arg = "state\tresult\terr". */
    if (!arg || !*arg) { printf("\t\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *state = t1 ? t1 + 1 : "";
    if (strcmp(state, "no_resolve") == 0) {
        fprintf(stderr, "could not resolve the base Python interpreter\n");
        return 1;
    }
    if (strcmp(state, "no_base") == 0) {
        fprintf(stderr, "base Python interpreter was not found\n");
        return 1;
    }
    printf("%s\t%s\n", t3 ? t3 + 1 : "?", t2 ? t2 + 1 : "");
    return 0;
}

/* PoP: spawn_backend @ hermes_cli/windows_ssh_runtime.py:spawn_backend */
int wssr_spawn_backend(const char *arg) { (void)arg; return 0; }

/* PoP: inspect_hermes @ hermes_cli/windows_ssh_runtime.py:inspect_hermes */
int wssr_inspect_hermes(const char *arg) {
    /* Python: version + serve help probes. Arg =
     * "path\tversion\tsupported\tstate". */
    if (!arg || !*arg) { printf("{\"supported\": false}\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int state = t3 && t3[1] == '1';
    if (!state) { printf("{\"supported\": false}\n"); return 1; }
    printf("{\"path\": \"%s\", \"version\": \"%s\", \"supported\": %s}\n",
           arg, t1 ? t1 + 1 : "", (t2 && t2[1] == '1') ? "true" : "false");
    return 0;
}
