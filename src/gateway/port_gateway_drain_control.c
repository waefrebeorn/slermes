/* Slermes C port — gateway/drain_control.py (marker contract, pure FS helpers) */

#include <stdbool.h>
#include "hermes_gateway_core.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <regex.h>
#include <sys/stat.h>

static char *read_file_text(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    char tmp[1024];
    size_t rd;
    while ((rd = fread(tmp, 1, sizeof(tmp), f)) > 0) {
        if (len + rd + 1 > cap) { cap = (len + rd + 1) * 2; buf = realloc(buf, cap); }
        memcpy(buf + len, tmp, rd); len += rd;
    }
    buf[len] = '\0';
    fclose(f);
    return buf;
}

static bool write_file_atomic(const char *path, const char *content)
{
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    FILE *f = fopen(tmp, "wb");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return rename(tmp, path) == 0;
}

/* Mirror current_instantiation_epoch(): boot_id + pid1 start time. */
static void current_instantiation_epoch(char *out, size_t outsz)
{
    out[0] = '\0';
    char *boot = read_file_text("/proc/sys/kernel/random/boot_id");
    char *pid1 = read_file_text("/proc/1/stat");
    char bid[64] = "", p1[64] = "";
    if (boot) { size_t i = 0; while (boot[i] && boot[i] != '\n' && i < sizeof(bid)-1) bid[i] = boot[i], i++; bid[i]='\0'; }
    if (pid1) {
        char *rparen = strrchr(pid1, ')');
        if (rparen) {
            /* fields after comm: state ppid ... starttime is the 20th */
            char *rest = rparen + 1;
            int n = 0; char *tok = strtok(rest, " ");
            while (tok) {
                n++;
                if (n == 20) { strncpy(p1, tok, sizeof(p1)-1); p1[sizeof(p1)-1]='\0'; break; }
                tok = strtok(NULL, " ");
            }
        }
    }
    if (boot) free(boot);
    if (pid1) free(pid1);
    if (!bid[0] && !p1[0]) return;  /* no /proc -> empty epoch */
    snprintf(out, outsz, "%s:%s", bid, p1);
}

static const char *drain_request_path(char *out, size_t outsz, const char *home)
{
    const char *h = home ? home : getenv("HERMES_HOME");
    if (!h) h = ".";
    snprintf(out, outsz, "%s/.drain_request.json", h);
    return out;
}

/* PoP: gateway_drain_control_write_drain_request @ gateway/drain_control.py:write_drain_request */
/* Returns a malloc'd JSON payload string (caller frees). */
char *gateway_drain_control_write_drain_request(const char *principal, bool suppress_notification, const char *home)
{
    char epoch[128]; current_instantiation_epoch(epoch, sizeof(epoch));
    char path[1024]; drain_request_path(path, sizeof(path), home);
    char ts[64]; time_t now = time(NULL);
    struct tm tm; gmtime_r(&now, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &tm);
    char payload[512];
    snprintf(payload, sizeof(payload),
        "{\"action\": \"drain\", \"requested_at\": \"%s\", \"principal\": \"%s\", "
        "\"epoch\": \"%s\", \"suppress_notification\": %s}",
        ts, principal ? principal : "drain-control", epoch, suppress_notification ? "true" : "false");
    write_file_atomic(path, payload);
    return strdup(payload);
}

/* PoP: gateway_drain_control_clear_drain_request @ gateway/drain_control.py:clear_drain_request */
bool gateway_drain_control_clear_drain_request(const char *home)
{
    char path[1024]; drain_request_path(path, sizeof(path), home);
    if (unlink(path) == 0) return true;
    return false;  /* missing or error -> idempotent False */
}

/* Internal: marker epoch stale? (lenient) */
static bool marker_epoch_is_stale(const char *body, const char *current_epoch)
{
    if (!current_epoch || !current_epoch[0]) return false;
    /* find "epoch" : "..." */
    char pat[32]; snprintf(pat, sizeof(pat), "\"epoch\"[ \t]*:[ \t]*\"");
    regex_t re; if (regcomp(&re, pat, REG_EXTENDED) != 0) return false;
    regmatch_t m; bool stale = false;
    if (regexec(&re, body, 1, &m, 0) == 0) {
        char *s = (char *)body + m.rm_eo - 1;
        if (*s == '"') {
            char mepoch[128]; size_t o = 0;
            while (s[1] && s[1] != '"' && o < sizeof(mepoch) - 1) mepoch[o++] = s[1], s++;
            mepoch[o] = '\0';
            if (mepoch[0] && strcmp(mepoch, current_epoch) != 0) stale = true;
        }
    }
    regfree(&re);
    return stale;
}

/* PoP: gateway_drain_control_read_drain_request @ gateway/drain_control.py:read_drain_request */
/* Returns malloc'd JSON body (possibly "{}") or NULL if absent. Caller frees. */
char *gateway_drain_control_read_drain_request(const char *home)
{
    char path[1024]; drain_request_path(path, sizeof(path), home);
    char *raw = read_file_text(path);
    if (!raw) return NULL;
    /* try parse as JSON object; on failure return "{}" */
    if (strstr(raw, "{") != NULL) {
        /* minimal: ensure it's an object; return as-is if starts with '{' */
        if (raw[0] == '{') return raw;
    }
    free(raw);
    return strdup("{}");
}
