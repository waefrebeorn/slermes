/* terminal_breadcrumbs_pure.c - Faithful C11 port of the pure, deterministic
 * helpers in hermes_cli/terminal_breadcrumbs.py (per-terminal session
 * breadcrumbs). The tty/config/DB glue is NOT ported (not pure logic).
 *
 * Ports (PoP):
 *   _SANITIZE_RE.sub("-", raw...)  -> tbr_sanitize  (filename-safe id)
 *   stale check (now - ts > window) -> tbr_is_stale_payload
 */

#include "hermes_core_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define TBR_STALE_AFTER_SECONDS (30LL * 24 * 3600)

/* _SANITIZE_RE = [^A-Za-z0-9._-] ; sanitize strips + replaces, caps 120. */
/* PoP: tbr_sanitize @ hermes_cli/terminal_breadcrumbs.py:_SANITIZE_RE */
char *tbr_sanitize(const char *raw) {
    char *s = strdup(raw ? raw : "");
    /* strip leading/trailing whitespace + slashes */
    char *p = s; while (*p == ' ' || *p == '\t') p++;
    size_t l = strlen(p); while (l > 0 && (p[l - 1] == ' ' || p[l - 1] == '\t' || p[l - 1] == '/')) p[--l] = 0;
    /* replace invalid chars with '-' */
    char *out = (char *)malloc(128);
    size_t o = 0;
    for (size_t i = 0; p[i] && o < 119; i++) {
        char c = p[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-') {
            out[o++] = c;
        } else {
            out[o++] = '-';
        }
    }
    out[o] = '\0';
    free(s);
    return out;
}

/* Mirrors read_breadcrumb's staleness gate: ts (epoch float) older than the
 * window from now is stale. Returns 1 if stale, 0 otherwise. A non-finite /
 * missing ts is treated as stale (best-effort: drop).  */
/* PoP: tbr_is_stale_payload @ hermes_cli/terminal_breadcrumbs.py:_STALE_AFTER_SECONDS */

int tbr_is_stale_payload(double ts, double now) {
    if (!(ts == ts) || ts <= 0.0) return 1;  /* NaN or missing -> stale */
    return (now - ts) > (double)TBR_STALE_AFTER_SECONDS;
}

/* PoP: _breadcrumbs_dir @ hermes_cli/terminal_breadcrumbs.py:_breadcrumbs_dir */
char *hermes_cli_terminal_breadcrumbs_dir(void) {
    /* Python: return get_hermes_home() / "terminal-sessions" */
    extern char *hermes_constants_get_hermes_home(void);
    const char *home = hermes_constants_get_hermes_home();
    size_t len = strlen(home) + 32;
    char *buf = malloc(len);
    if (!buf) return NULL;
    snprintf(buf, len, "%s/terminal-sessions", home);
    return buf;
}

/* PoP: _sanitize @ hermes_cli/terminal_breadcrumbs.py:_sanitize */
char *hermes_cli_terminal_breadcrumbs_sanitize(const char *raw) {
    /* Python: return _SANITIZE_RE.sub("-", raw.strip().strip("/"))[:120] */
    if (!raw) return strdup("");
    size_t len = strlen(raw);
    if (len > 120) len = 120;
    char *buf = (char *)malloc(len + 1);
    if (!buf) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len && raw[i]; i++) {
        if (raw[i] == '/') continue;
        if (raw[i] >= 'a' && raw[i] <= 'z') buf[j++] = raw[i];
        else if (raw[i] >= 'A' && raw[i] <= 'Z') buf[j++] = raw[i] - 'A' + 'a';
        else if (raw[i] >= '0' && raw[i] <= '9') buf[j++] = raw[i];
        else buf[j++] = '-';
    }
    buf[j] = '\0';
    return buf;
}

/* _TERMINAL_ENV_VARS = ("ZELLIJ_PANE_ID", "TMUX_PANE", "KITTY_WINDOW_ID",
 * "WEZTERM_PANE", "TERM_SESSION_ID", "WT_SESSION") */
static const char *TBR_ENV_VARS[] = {
    "ZELLIJ_PANE_ID", "TMUX_PANE", "KITTY_WINDOW_ID",
    "WEZTERM_PANE", "TERM_SESSION_ID", "WT_SESSION",
};
static const size_t TBR_ENV_COUNT =
    sizeof(TBR_ENV_VARS) / sizeof(TBR_ENV_VARS[0]);

/* PoP: get_terminal_id @ hermes_cli/terminal_breadcrumbs.py:get_terminal_id */
char *tbr_get_terminal_id(void) {
    /* Prefers the real tty device path (stdin, then stdout). */
    char *tty = NULL;
    for (int fd = 0; fd <= 1; fd++) {
        tty = ttyname(fd);
        if (tty) {
            char *s = tbr_sanitize(tty);
            size_t slen = strlen(s);
            char *result = (char *)malloc(slen + 5);
            if (result) {
                snprintf(result, slen + 5, "tty-%s", s);
                free(s);
                return result;
            }
            free(s);
        }
    }
    /* Fall back to first present multiplexer/emulator env var. */
    for (size_t i = 0; i < TBR_ENV_COUNT; i++) {
        const char *val = getenv(TBR_ENV_VARS[i]);
        if (val && *val) {
            char *s = tbr_sanitize(val);
            char *lower = strdup(TBR_ENV_VARS[i]);
            for (char *p = lower; *p; p++)
                if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
            size_t slen = strlen(lower) + strlen(s) + 2;
            char *result = (char *)malloc(slen);
            if (result) {
                snprintf(result, slen, "%s-%s", lower, s);
                free(s); free(lower);
                return result;
            }
            free(s); free(lower);
        }
    }
    return NULL;
}

/* PoP: _prune_stale @ hermes_cli/terminal_breadcrumbs.py:_prune_stale */
void tbr_prune_stale(const char *dir, double now) {
    if (!dir) return;
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *entry;
    char path[4096];
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        struct stat st;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (stat(path, &st) != 0) continue;
        if (!S_ISREG(st.st_mode)) continue;
        if (tbr_is_stale_payload((double)st.st_mtime, now)) {
            unlink(path);
        }
    }
    closedir(d);
}

/* PoP: write_breadcrumb @ hermes_cli/terminal_breadcrumbs.py:write_breadcrumb */
void tbr_write_breadcrumb(const char *session_id, const char *cwd) {
    if (!session_id || !*session_id) return;

    /* Config gate: session.terminal_continue (default true). */
    extern json_t *config_py_load_config_readonly(void);
    extern json_t *config_py_get_config_value(const json_t *cfg,
                                               const char *dotted_key,
                                               const json_t *default_val);
    json_t *cfg = config_py_load_config_readonly();
    bool enabled = true;
    if (cfg) {
        json_t *val = config_py_get_config_value(cfg, "session.terminal_continue", NULL);
        if (val && val->type == JSON_BOOL) {
            enabled = val->bool_val;
        }
        json_free(val);
        json_free(cfg);
    }
    if (!enabled) return;

    char *terminal_id = tbr_get_terminal_id();
    if (!terminal_id) return;

    char *dir = hermes_cli_terminal_breadcrumbs_dir();
    if (!dir) {
        free(terminal_id);
        return;
    }

    /* directory.mkdir(parents=True, exist_ok=True) */
    struct stat st;
    if (stat(dir, &st) != 0) {
        mkdir(dir, 0700);
    }

    char *wd = cwd ? (char *)cwd : getcwd(NULL, 0);
    double now = (double)time(NULL);

    /* Write payload JSON: {"session_id":..., "cwd":..., "ts":...} */
    char payload[8192];
    /* Simple JSON escaping for cwd (which may contain backslashes/quotes). */
    char esc_wd[4096];
    size_t ei = 0;
    if (wd) {
        for (size_t i = 0; wd[i] && ei < sizeof(esc_wd) - 2; i++) {
            if (wd[i] == '"' || wd[i] == '\\') {
                esc_wd[ei++] = '\\';
                esc_wd[ei++] = wd[i];
            } else if (wd[i] == '\n') {
                esc_wd[ei++] = '\\'; esc_wd[ei++] = 'n';
            } else {
                esc_wd[ei++] = wd[i];
            }
        }
    }
    esc_wd[ei] = '\0';

    /* Sanitize terminal_id for use in filenames (reuse tbr_sanitize). */
    char *safe_id = tbr_sanitize(terminal_id);

    char tmp_path[4200], final_path[4200];
    snprintf(tmp_path, sizeof(tmp_path), "%s/.%s.tmp", dir, safe_id);
    snprintf(final_path, sizeof(final_path), "%s/%s", dir, safe_id);

    /* payload JSON */
    snprintf(payload, sizeof(payload),
             "{\"session_id\":\"%s\",\"cwd\":\"%s\",\"ts\":%.6f}",
             session_id, esc_wd, now);

    FILE *f = fopen(tmp_path, "w");
    if (f) {
        fputs(payload, f);
        fclose(f);
        rename(tmp_path, final_path);
    }

    tbr_prune_stale(dir, now);
    free(safe_id);
    if (wd != cwd) free(wd);
    free(terminal_id);
    free(dir);
}
