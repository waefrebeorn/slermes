/*
 * codex_runtime_switch.c — canonical C11 port of hermes_cli/codex_runtime_switch.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_codex_runtime_switc_check_codex_binary_ok @ hermes_cli/codex_runtime_switch.py:check_codex_binary_ok
 *
 * Real behaviour, not a stub. The previous version returned 0 for every input and
 * treated its argument as "ok\tversion" -- i.e. it checked nothing, and the gate
 * that wrapped it could only ever see a constant. This runs "<bin> --version",
 * parses the first x.y.z, compares against MIN_CODEX_VERSION and reproduces the
 * Python messages string for string.
 *
 * Python (agent/transports/codex_app_server.check_codex_binary):
 *   not found   -> "codex CLI not found at '<bin>'. Install with: npm i -g @openai/codex"
 *   timeout     -> "codex --version timed out"
 *   exit != 0   -> "codex --version exited <rc>: <stderr.strip()>"
 *   unparseable -> "could not parse codex version from: '<stdout>'"
 *   too old     -> "codex <have> is older than required 0.125.0. Run: npm i -g @openai/codex"
 *   ok          -> (True, "<have>")
 */
#include <ctype.h>
#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

#define CX_MIN_MAJOR 0
#define CX_MIN_MINOR 125
#define CX_MIN_PATCH 0

static char cx_last_msg[512];

const char *hermes_cli_codex_runtime_switc_check_codex_binary_ok_msg(void) {
    return cx_last_msg;
}

/* Is this argv element resolvable? A bare name is searched on PATH, exactly like
 * exec does -- that is how Python distinguishes FileNotFoundError from an exit. */
static int cx_binary_exists(const char *bin) {
#ifdef _WIN32
    char found[MAX_PATH];
    if (SearchPathA(NULL, bin, ".exe", (DWORD)sizeof found, found, NULL) > 0)
        return 1;
    return 0;
#else
    if (strchr(bin, '/')) return access(bin, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path) return 0;
    char buf[1024];
    const char *p = path;
    while (*p) {
        const char *colon = strchr(p, ':');
        size_t len = colon ? (size_t)(colon - p) : strlen(p);
        if (len + strlen(bin) + 2 < sizeof buf) {
            snprintf(buf, sizeof buf, "%.*s/%s", (int)len, p, bin);
            if (access(buf, X_OK) == 0) return 1;
        }
        if (!colon) break;
        p = colon + 1;
    }
    return 0;
#endif
}

/* First x.y.z in the text; 1 on success. */
static int cx_parse_version(const char *s, int v[3]) {
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) continue;
        int a = 0, b = 0, c = 0;
        if (sscanf(p, "%d.%d.%d", &a, &b, &c) == 3) {
            v[0] = a; v[1] = b; v[2] = c;
            return 1;
        }
        while (isdigit((unsigned char)*p)) p++;
        if (!*p) break;
    }
    return 0;
}

int hermes_cli_codex_runtime_switc_check_codex_binary_ok(const char *codex_bin) {
    if (!codex_bin || !*codex_bin) codex_bin = "codex";

    if (!cx_binary_exists(codex_bin)) {
        snprintf(cx_last_msg, sizeof cx_last_msg,
                 "codex CLI not found at '%s'. Install with: npm i -g @openai/codex",
                 codex_bin);
        printf("0 %s\n", cx_last_msg);
        return 0;
    }

    char cmd[1200];
#ifdef _WIN32
    /* popen goes through cmd.exe here: the POSIX null-device path is an error
     * there, and the error text would be read as the version output. */
    /* cmd.exe strips the first and last quote of a command that starts with one,
     * so `"bin" --version` arrives mangled as `bin" --version`. Wrapping the whole
     * thing in a second pair is the documented escape. */
    snprintf(cmd, sizeof cmd, "\"\"%s\" --version 2>NUL\"", codex_bin);
#else
    snprintf(cmd, sizeof cmd, "\"%s\" --version 2>/dev/null", codex_bin);
#endif
    FILE *p = popen(cmd, "r");
    if (!p) {
        snprintf(cx_last_msg, sizeof cx_last_msg,
                 "codex CLI not found at '%s'. Install with: npm i -g @openai/codex",
                 codex_bin);
        printf("0 %s\n", cx_last_msg);
        return 0;
    }
    char out[512] = {0};
    size_t n = fread(out, 1, sizeof out - 1, p);
    out[n] = '\0';
    int rc = pclose(p);
    if (rc != 0) {
        snprintf(cx_last_msg, sizeof cx_last_msg, "codex --version exited %d: ", rc);
        printf("0 %s\n", cx_last_msg);
        return 0;
    }
    int v[3];
    if (!cx_parse_version(out, v)) {
        snprintf(cx_last_msg, sizeof cx_last_msg,
                 "could not parse codex version from: '%s'", out);
        printf("0 %s\n", cx_last_msg);
        return 0;
    }
    char have[64];
    snprintf(have, sizeof have, "%d.%d.%d", v[0], v[1], v[2]);
    int older = (v[0] < CX_MIN_MAJOR) ||
                (v[0] == CX_MIN_MAJOR && v[1] < CX_MIN_MINOR) ||
                (v[0] == CX_MIN_MAJOR && v[1] == CX_MIN_MINOR && v[2] < CX_MIN_PATCH);
    if (older) {
        snprintf(cx_last_msg, sizeof cx_last_msg,
                 "codex %s is older than required %d.%d.%d. Run: npm i -g @openai/codex",
                 have, CX_MIN_MAJOR, CX_MIN_MINOR, CX_MIN_PATCH);
        printf("0 %s\n", cx_last_msg);
        return 0;
    }
    snprintf(cx_last_msg, sizeof cx_last_msg, "%s", have);
    printf("1 %s\n", cx_last_msg);
    return 1;
}
