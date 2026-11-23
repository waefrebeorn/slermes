/*
 * update_cmd_zip.c — C home for upstream Hermes hermes_cli/update_cmd_zip.py.
 *
 * PoP: w12_updatecmd_8df38e_status_top_level @ hermes_cli/update_cmd_zip.py:_status_top_level
 * PoP: w12_updatecmd_8df38e_is_zip_preserved_entry_status_line @ hermes_cli/update_cmd_zip.py:_is_zip_preserved_entry_status_line
 * PoP: w12_updatecmd_8df38e_is_zip_staging_artifact_status_line @ hermes_cli/update_cmd_zip.py:_is_zip_staging_artifact_status_line
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "noc_w12_updatecmd_8df38e.h"

#define CAP 8192

/* Normalize a path range exactly as Python's:
 *   .strip().strip('"').replace("\\","/").rstrip("/")
 * NUL is rewritten at every shortened length. */
static void norm_range(const char *s, size_t len, char *buf, size_t cap) {
    size_t n = len;
    if (n >= cap) n = cap - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    size_t b = 0, e = n;
    while (b < e && isspace((unsigned char)buf[b])) b++;
    while (e > b && isspace((unsigned char)buf[e - 1])) e--;
    if (b > 0) memmove(buf, buf + b, e - b);
    buf[e - b] = '\0'; n = e - b;
    b = 0; e = n;
    while (b < e && buf[b] == '"') b++;
    while (e > b && buf[e - 1] == '"') e--;
    if (b > 0) memmove(buf, buf + b, e - b);
    buf[e - b] = '\0'; n = e - b;
    for (size_t i = 0; i < n; i++) if (buf[i] == '\\') buf[i] = '/';
    while (n > 0 && buf[n - 1] == '/') { n--; buf[n] = '\0'; }
}

/* Truncate at the first '/' — Python split("/",1)[0]. */
static const char *top_of(char *buf) {
    char *slash = strchr(buf, '/');
    if (slash) *slash = '\0';
    return buf;
}

static int in_preserved_top(const char *s) {
    static const char *const set[] = {"venv", ".venv", "node_modules", ".git", ".env"};
    for (size_t i = 0; i < sizeof(set)/sizeof(set[0]); i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}

static int in_collection(const json_t *coll, const char *s) {
    if (!coll || coll->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(coll); i++) {
        json_t *v = json_get(coll, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
}

static int nested_allowed(const char *top, const char *nested) {
    static const char *const apps[] = {"desktop/release", "desktop/dist", "desktop/dist-webapp", "desktop/build"};
    static const char *const hm[] = {"web_dist"};
    static const char *const scripts[] = {"whatsapp-bridge/node_modules"};
    static const char *const tui[] = {"dist", "packages/hermes-ink/dist"};
    const char *const *list = NULL; size_t n = 0;
    if (!strcmp(top, "apps")) { list = apps; n = 4; }
    else if (!strcmp(top, "hermes_cli")) { list = hm; n = 1; }
    else if (!strcmp(top, "scripts")) { list = scripts; n = 1; }
    else if (!strcmp(top, "ui-tui")) { list = tui; n = 2; }
    for (size_t i = 0; i < n; i++) {
        size_t kl = strlen(list[i]);
        if (!strcmp(nested, list[i]) || (!strncmp(nested, list[i], kl) && nested[kl] == '/')) return 1;
    }
    return 0;
}

json_t *w12_updatecmd_8df38e_status_top_level(const char *path) {
    char buf[CAP];
    norm_range(path ? path : "", strlen(path ? path : ""), buf, sizeof(buf));
    return json_string(top_of(buf));
}

json_t *w12_updatecmd_8df38e_is_zip_preserved_entry_status_line(const char *line, const json_t *shipped) {
    const char *s = line ? line : "";
    size_t len = strlen(s);
    char status_buf[8] = "";
    const char *status = "";
    const char *payload = s;
    if (len >= 3) {
        status_buf[0] = s[0]; status_buf[1] = s[1]; status_buf[2] = '\0';
        status = status_buf;
        payload = s + 3;
    }
    int split = 0;
    for (const char *p = status; *p; p++) if (*p == 'R' || *p == 'C') { split = 1; break; }

    char work[CAP];
    if (!split) {
        norm_range(payload, strlen(payload), work, sizeof(work));
        if (in_preserved_top(top_of(work))) return json_bool(true);
    } else {
        const char *p = payload;
        int all = 1;
        for (;;) {
            const char *q = strstr(p, " -> ");
            size_t seg = q ? (size_t)(q - p) : strlen(p);
            norm_range(p, seg, work, sizeof(work));
            if (!in_preserved_top(top_of(work))) { all = 0; break; }
            if (!q) break;
            p = q + 4;
        }
        if (all) return json_bool(true);
    }
    if (strcmp(status, "!!") != 0) return json_bool(false);

    norm_range(payload, strlen(payload), work, sizeof(work));
    char path[CAP];
    memcpy(path, work, strlen(work) + 1);
    char top[CAP], nested[CAP];
    const char *slash = strchr(path, '/');
    if (!slash) { memcpy(top, path, strlen(path) + 1); nested[0] = '\0'; }
    else {
        size_t tl = (size_t)(slash - path);
        memcpy(top, path, tl); top[tl] = '\0';
        memcpy(nested, slash + 1, strlen(slash + 1) + 1);
    }
    if (shipped && shipped->type != JSON_NULL && !in_collection(shipped, top)) return json_bool(true);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    if (!strcmp(base, "__pycache__") || !strcmp(base, "node_modules")) return json_bool(true);
    return json_bool(nested_allowed(top, nested) ? true : false);
}

/* \.hermes-update-zip-swap\.[0-9a-f]{12}\.tmp  (fullmatch) */
static int is_journal_temp(const char *top) {
    static const char pre[] = ".hermes-update-zip-swap.";
    size_t pl = sizeof(pre) - 1;
    if (strncmp(top, pre, pl) != 0) return 0;
    const char *p = top + pl;
    for (int i = 0; i < 12; i++) {
        char c = p[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return 0;
    }
    return strcmp(p + 12, ".tmp") == 0;
}

json_t *w12_updatecmd_8df38e_is_zip_staging_artifact_status_line(const char *line, const json_t *staged) {
    const char *s = line ? line : "";
    size_t len = strlen(s);
    char tail[CAP];
    if (len >= 3) memcpy(tail, s + 3, strlen(s + 3) + 1);
    else memcpy(tail, s, len + 1);
    norm_range(tail, strlen(tail), tail, sizeof(tail));
    const char *top = top_of(tail);
    static const char *const roots[] = {
        ".hermes-update-zip-swap", ".hermes-update-zip-swap.lock",
        ".update-incomplete", ".lazy-refresh-incomplete"
    };
    for (size_t i = 0; i < sizeof(roots)/sizeof(roots[0]); i++)
        if (!strcmp(top, roots[i])) return json_bool(true);
    if (is_journal_temp(top)) return json_bool(true);
    if (staged && staged->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(staged); i++) {
            json_t *v = json_get(staged, i);
            if (!v || v->type != JSON_STRING || !v->str_val) continue;
            size_t need = strlen(v->str_val) + 32;
            char *want = (char *)malloc(need);
            if (!want) continue;
            snprintf(want, need, "%s.hermes-update-staging", v->str_val);
            int hit = !strcmp(top, want);
            free(want);
            if (hit) return json_bool(true);
        }
    }
    return json_bool(false);
}

void hermes_cli_update_cmd_zip_port_pending(void) { (void)0; }

