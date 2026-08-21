/*
 * t_port_bot_mode_probe.c — faithful parity harness for
 * src/tools/bot_mode_probe.c (Python tools/bot_mode_probe.py).
 *
 * Reads a JSON fixture from argv[1] describing a filesystem layout;
 * materializes it under HERMES_HOME, then calls the C probe functions and
 * emits one JSON line per op (same shape as sta_oracle_bot_mode_probe.py).
 * run_oracle.sh diffs the two.
 *
 * Fixture shape:
 *   { "home": "rel/root",
 *     "files": { "rel/path": "text content", ... },
 *     "ops": [ {"op":"section","home":"rel/root"},
 *              {"op":"fingerprint","home":"rel/root"},
 *              {"op":"epoch_line","home":"rel/root"},
 *              {"op":"stale","stored":"...","home":"rel/root"},
 *              {"op":"upgrade","stored":"...","home":"rel/root"} ] }
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "bot_mode_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

static void write_all(const char *path, const char *content) {
    char dir[8192];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = (char *)strrchr(dir, '/');
    if (slash) { *slash = '\0'; }
    if (dir[0] != '\0') {
        /* mkdir -p */
        char *p = dir;
        for (char *q = dir + 1; *q; q++) {
            if (*q == '/') { *q = '\0'; mkdir(dir, 0755); *q = '/'; }
        }
        mkdir(dir, 0755);
    }
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(content, 1, strlen(content), f);
    fclose(f);
}

static char *g_home = NULL;

static void materialize(const json_t *files, const char *home_root) {
    if (!files || files->type != JSON_OBJECT) return;
    size_t n = json_object_size(files);
    for (size_t i = 0; i < n; i++) {
        const char *key = json_object_get_key_at(files, i);
        json_t *val = json_object_get_at(files, i);
        if (!key || !val || val->type != JSON_STRING) continue;
        char full[16384];
        snprintf(full, sizeof(full), "%s/%s", home_root, key);
        write_all(full, val->str_val);
    }
}

static char *resolve_home(const char *rel) {
    static char buf[16384];
    snprintf(buf, sizeof(buf), "%s/%s", g_home, rel ? rel : ".");
    buf[sizeof(buf) - 1] = '\0';
    return buf;
}

static void emit_str(const char *op, const char *out) {
    printf("{\"op\": \"%s\", \"out\": \"%s\"}\n", op, out ? out : "");
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    const char *home_env = getenv("HERMES_HOME") ? getenv("HERMES_HOME")
                          : (getenv("HOME") ? getenv("HOME") : ".");
    g_home = (char *)home_env;

    char *raw = read_all(argv[1]);
    if (!raw) { fprintf(stderr, "cannot read fixture\n"); return 1; }
    /* substitute @SBX@ placeholder (same as python twin) */
    /* simple single-pass replace of the literal token */
    {
        char *out = malloc(strlen(raw) + 1);
        char *o = out;
        for (char *p = raw; *p; p++) {
            if (p[0] == '@' && p[1] == 'S' && p[2] == 'B' && p[3] == 'X' && p[4] == '@') {
                const char *s = g_home;
                while (*s) *o++ = *s++;
                p += 4;
            } else {
                *o++ = *p;
            }
        }
        *o = '\0';
        free(raw);
        raw = out;
    }

    json_t *root = json_parse(raw, NULL);
    if (!root) { fprintf(stderr, "bad fixture json\n"); free(raw); return 1; }

    const char *home_rel = json_get_str(root, "home", ".");
    char home_root[16384];
    snprintf(home_root, sizeof(home_root), "%s/%s", g_home, home_rel);
    home_root[sizeof(home_root) - 1] = '\0';

    json_t *files = json_object_get(root, "files");
    materialize(files, home_root);

    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_object_get_at(ops, i);
            const char *op = json_get_str(c, "op", "");
            const char *hrel = json_get_str(c, "home", home_rel);
            char h[16384];
            snprintf(h, sizeof(h), "%s/%s", g_home, hrel);
            if (strcmp(op, "section") == 0) {
                const char *s = get_bot_mode_protocol_section(h, 0);
                emit_str("section", s ? s : "");
            } else if (strcmp(op, "fingerprint") == 0) {
                char *fp = capability_fingerprint(h);
                emit_str("fingerprint", fp ? fp : "");
                free(fp);
            } else if (strcmp(op, "epoch_line") == 0) {
                char *el = epoch_line(h);
                emit_str("epoch_line", el ? el : "");
                free(el);
            } else if (strcmp(op, "stale") == 0) {
                const char *stored = json_get_str(c, "stored", "");
                int r = stored_prompt_capability_stale(stored, h);
                printf("{\"op\": \"stale\", \"out\": %s}\n", r ? "true" : "false");
            } else if (strcmp(op, "upgrade") == 0) {
                const char *stored = json_get_str(c, "stored", "");
                int r = stored_bot_chat_prompt_needs_upgrade(stored, h);
                printf("{\"op\": \"upgrade\", \"out\": %s}\n", r ? "true" : "false");
            }
        }
    }

    json_free(root);
    free(raw);
    return 0;
}
