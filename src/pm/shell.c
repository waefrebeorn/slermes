/*
 * shell.c — C port of upstream Hermes pm/shell.py (gate w5_shell_7b6f0b).
 *
 * PoP: w5_shell_7b6f0b_windows_bash_candidates @ pm/shell.py:windows_bash_candidates
 *
 * windows_bash_candidates is pure data: the env mapping and the on_path value
 * come from the gate, and the two ntpath helpers are reproduced below so the
 * ordering / dedup / stub filter match Python exactly. _staged_bash, _bash_starts
 * and bash() are filesystem/process probes and are intentionally not ported.
 */
#include "noc_w5_shell_7b6f0b.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PROGRAM_FILES "C:\\Program Files"

typedef struct { char b[8192]; size_t n; } sbuf_t;

static void sb_init(sbuf_t *s) { s->b[0] = '\0'; s->n = 0; }
static void sb_add(sbuf_t *s, const char *x) {
    size_t l = strlen(x);
    if (s->n + l >= sizeof(s->b)) l = sizeof(s->b) - 1 - s->n;
    memcpy(s->b + s->n, x, l);
    s->n += l;
    s->b[s->n] = '\0';
}
static void sb_ch(sbuf_t *s, char c) {
    if (s->n + 1 < sizeof(s->b)) { s->b[s->n++] = c; s->b[s->n] = '\0'; }
}

static char *dupstr(const char *s) {
    size_t n = strlen(s);
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n + 1);
    return o;
}

/* ntpath.join for the relative segments used here: a single backslash
 * separator, no doubled separator when the left side already ends in one. */
static char *nt_join(const char *const *parts, size_t nparts) {
    sbuf_t s;
    sb_init(&s);
    for (size_t i = 0; i < nparts; i++) {
        const char *b = parts[i] ? parts[i] : "";
        if (!*b) continue;
        if (s.n == 0) { sb_add(&s, b); continue; }
        char last = s.b[s.n - 1];
        if (last == '\\' || last == '/') sb_add(&s, b);
        else { sb_ch(&s, '\\'); sb_add(&s, b); }
    }
    return dupstr(s.b);
}

static char *nt_join2(const char *a, const char *b) {
    const char *p[2] = { a, b };
    return nt_join(p, 2);
}
static char *nt_join3(const char *a, const char *b, const char *c) {
    const char *p[3] = { a, b, c };
    return nt_join(p, 3);
}

/* ntpath.normpath(path).lower() — enough of the Windows algorithm to decide
 * whether on_path sits under one of the two bash stub directories. */
static void nt_norm_lower(const char *path, char *out, size_t outsz) {
    char tmp[8192];
    size_t n = 0;
    for (const char *q = path; q && *q && n < sizeof(tmp) - 1; q++)
        tmp[n++] = (*q == '/') ? '\\' : *q;
    tmp[n] = '\0';

    sbuf_t pref;
    sb_init(&pref);
    size_t start = 0;
    if (n >= 2 && tmp[0] == '\\' && tmp[1] == '\\') { sb_add(&pref, "\\\\"); start = 2; }
    else if (n >= 1 && tmp[0] == '\\') { sb_ch(&pref, '\\'); start = 1; }
    else if (n >= 2 && isalpha((unsigned char)tmp[0]) && tmp[1] == ':') {
        sb_ch(&pref, tmp[0]); sb_ch(&pref, ':'); start = 2;
    }

    char *comps[128];
    size_t nc = 0;
    size_t i = start;
    while (i < n) {
        while (i < n && tmp[i] == '\\') i++;
        if (i >= n) break;
        size_t j = i;
        while (j < n && tmp[j] != '\\') j++;
        tmp[j] = '\0';
        size_t len = j - i;
        if (len == 1 && tmp[i] == '.') { /* drop */ }
        else if (len == 2 && tmp[i] == '.' && tmp[i + 1] == '.') { if (nc) nc--; }
        else if (nc < 128) comps[nc++] = tmp + i;
        i = j + 1;
    }

    sbuf_t res;
    sb_init(&res);
    sb_add(&res, pref.b);
    for (size_t k = 0; k < nc; k++) {
        if (res.n && res.b[res.n - 1] != '\\') sb_ch(&res, '\\');
        sb_add(&res, comps[k]);
    }
    if (res.n == 0) sb_add(&res, ".");
    for (size_t k = 0; k < res.n; k++)
        if (res.b[k] >= 'A' && res.b[k] <= 'Z') res.b[k] = (char)(res.b[k] + 32);
    snprintf(out, outsz, "%s", res.b);
}

static const char *env_get(const w5_shell_7b6f0b_env_t *env, size_t n, const char *key) {
    for (size_t i = 0; i < n; i++)
        if (env[i].key && !strcmp(env[i].key, key)) return env[i].value ? env[i].value : "";
    return NULL;
}

static int has_str(char **arr, size_t n, const char *s) {
    for (size_t i = 0; i < n; i++) if (!strcmp(arr[i], s)) return 1;
    return 0;
}
static void push(char ***arr, size_t *n, size_t *cap, char *s) {
    if (!s) return;
    if (has_str(*arr, *n, s)) { free(s); return; }
    if (*n + 1 >= *cap) {
        size_t nc = *cap * 2;
        char **na = (char **)realloc(*arr, nc * sizeof(char *));
        if (!na) { free(s); return; }
        *arr = na; *cap = nc;
    }
    (*arr)[(*n)++] = s;
}

/* PoP: w5_shell_7b6f0b_windows_bash_candidates @ pm/shell.py:windows_bash_candidates */
char **w5_shell_7b6f0b_windows_bash_candidates(
    const char *on_path, const w5_shell_7b6f0b_env_t *env, size_t env_count,
    size_t *out_n)
{
    size_t cap = 32, n = 0;
    char **res = (char **)malloc(cap * sizeof(char *));
    if (!res) { if (out_n) *out_n = 0; return NULL; }

    const char *pf = env_get(env, env_count, "ProgramFiles");
    if (!pf) pf = DEFAULT_PROGRAM_FILES;
    const char *override = env_get(env, env_count, "HERMES_GIT_BASH_PATH");
    if (override && *override) push(&res, &n, &cap, dupstr(override));

    char *roots[4];
    size_t nr = 0;
    roots[nr++] = nt_join2(pf, "Git");
    const char *pf86 = env_get(env, env_count, "ProgramFiles(x86)");
    if (pf86 && *pf86) roots[nr++] = nt_join2(pf86, "Git");
    const char *la = env_get(env, env_count, "LOCALAPPDATA");
    if (la && *la) {
        roots[nr++] = nt_join3(la, "hermes", "git");
        roots[nr++] = nt_join3(la, "Programs", "Git");
    }
    for (size_t i = 0; i < nr; i++) {
        push(&res, &n, &cap, nt_join3(roots[i], "bin", "bash.exe"));
        {
            const char *u[4] = { roots[i], "usr", "bin", "bash.exe" };
            push(&res, &n, &cap, nt_join(u, 4));
        }
    }
    for (size_t i = 0; i < nr; i++) free(roots[i]);

    if (on_path && *on_path) {
        char norm[8192];
        nt_norm_lower(on_path, norm, sizeof(norm));
        if (!strstr(norm, "system32") && !strstr(norm, "windowsapps"))
            push(&res, &n, &cap, dupstr(on_path));
    }
    if (out_n) *out_n = n;
    return res;
}

void w5_shell_7b6f0b_strings_free(char **arr, size_t n) {
    if (!arr) return;
    for (size_t i = 0; i < n; i++) free(arr[i]);
    free(arr);
}
