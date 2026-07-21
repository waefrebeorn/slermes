/* Slermes C11 port of gateway/restart_loop_guard.py — implementation.
 * PoP: exact port. Semantic source of truth = gateway/restart_loop_guard.py. */
#include "restart_loop_guard.h"
#include "hermes_gateway_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Resolve hermes home: explicit arg, else HERMES_HOME env, else ~/.hermes. */
static void resolve_home(char *out, size_t outsz, const char *hermes_home) {
    if (hermes_home && hermes_home[0]) {
        snprintf(out, outsz, "%s", hermes_home);
        return;
    }
    const char *env = getenv("HERMES_HOME");
    if (env && env[0]) {
        snprintf(out, outsz, "%s", env);
        return;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(out, outsz, "%s/.hermes", home);
        return;
    }
    snprintf(out, outsz, "%s", ".hermes");
}

/* PoP: restart_loop_state_path @ gateway/restart_loop_guard.py:_state_path */
char *restart_loop_state_path(char *buf, size_t bufsz, const char *hermes_home) {
    char home[PATH_MAX];
    resolve_home(home, sizeof(home), hermes_home);
    if (snprintf(buf, bufsz, "%s/gateway/restart_loop.json", home) >= (int)bufsz)
        return NULL;
    return buf;
}

/* PoP: restart_loop_load_boots @ gateway/restart_loop_guard.py:_load_boots */
int restart_loop_load_boots(double *out, int cap, const char *hermes_home) {
    char path[PATH_MAX];
    if (!restart_loop_state_path(path, sizeof(path), hermes_home)) return -1;
    FILE *f = fopen(path, "rb");
    if (!f) return 0; /* missing file -> empty list (best-effort) */
    /* read whole file */
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 0) { fclose(f); return 0; }
    char *raw = malloc((size_t)n + 1);
    if (!raw) { fclose(f); return -1; }
    size_t rd = fread(raw, 1, (size_t)n, f);
    raw[rd] = '\0';
    fclose(f);

    /* Minimal JSON object parse: find "boots" : [ ... ] */
    int count = 0;
    const char *b = strstr(raw, "\"boots\"");
    if (b) {
        const char *arr = strchr(b, '[');
        if (arr) {
            const char *p = arr + 1;
            while (*p && *p != ']') {
                /* skip whitespace and commas */
                while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\t') p++;
                if (*p == ']') break;
                char *endp = NULL;
                double v = strtod(p, &endp);
                if (endp == p) { p++; continue; } /* not a number, skip */
                if (count < cap) out[count++] = v;
                p = endp;
            }
        }
    }
    free(raw);
    return count;
}

/* PoP: restart_loop_save_boots @ gateway/restart_loop_guard.py:_save_boots */
bool restart_loop_save_boots(const double *boots, int n, const char *hermes_home) {
    char path[PATH_MAX];
    if (!restart_loop_state_path(path, sizeof(path), hermes_home)) return false;
    /* mkdir -p <home>/gateway (create parent home too, best effort) */
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        /* create the home dir itself if needed */
        mkdir(dir, 0700);
        char *slash2 = strrchr(dir, '/');
        if (slash2) { *slash2 = '\0'; mkdir(dir, 0700); }
        /* restore and create gateway dir */
        snprintf(dir, sizeof(dir), "%s", path);
        slash = strrchr(dir, '/');
        if (slash) { *slash = '\0'; mkdir(dir, 0700); }
    }
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "{\"boots\":[");
    for (int i = 0; i < n; i++) {
        if (i) fprintf(f, ",");
        fprintf(f, "%g", boots[i]);
    }
    fprintf(f, "]}");
    if (fclose(f) != 0) return false;
    return true;
}

/* PoP: restart_loop_record_boot @ gateway/restart_loop_guard.py:record_restart_interrupted_boot */
int restart_loop_record_boot(int window_seconds, double now, const char *hermes_home) {
    double boots[1024];
    int cap = (int)(sizeof(boots) / sizeof(boots[0]));
    int n = restart_loop_load_boots(boots, cap, hermes_home);
    if (n < 0) n = 0;
    double cutoff = now - (window_seconds > 1 ? window_seconds : 1);
    int w = 0;
    for (int i = 0; i < n; i++)
        if (boots[i] >= cutoff) boots[w++] = boots[i];
    if (w < cap) boots[w++] = now;
    if (!restart_loop_save_boots(boots, w, hermes_home)) return -1;
    return w;
}

/* PoP: restart_loop_is_tripped @ gateway/restart_loop_guard.py:is_restart_loop_tripped */
bool restart_loop_is_tripped(int max_restarts, int window_seconds, double now,
                             const char *hermes_home) {
    if (max_restarts <= 0) return false;
    double boots[1024];
    int n = restart_loop_load_boots(boots, (int)(sizeof(boots) / sizeof(boots[0])), hermes_home);
    if (n < 0) return false; /* fail open */
    double cutoff = now - (window_seconds > 1 ? window_seconds : 1);
    int recent = 0;
    for (int i = 0; i < n; i++)
        if (boots[i] >= cutoff) recent++;
    return recent >= max_restarts;
}

/* PoP: restart_loop_clear @ gateway/restart_loop_guard.py:clear */
void restart_loop_clear(const char *hermes_home) {
    char path[PATH_MAX];
    if (!restart_loop_state_path(path, sizeof(path), hermes_home)) return;
    remove(path);
}

/* PoP: restart_loop_check_and_record @ gateway/restart_loop_guard.py:check_and_record */
bool restart_loop_check_and_record(int max_restarts, int window_seconds, double now,
                                   const char *hermes_home) {
    int n = restart_loop_record_boot(window_seconds, now, hermes_home);
    if (n < 0) return false; /* fail open */
    if (max_restarts <= 0) return false;
    return n >= max_restarts;
}
