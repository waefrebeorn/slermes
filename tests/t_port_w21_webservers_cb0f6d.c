/* JSON-lines harness for the w21_webservers_cb0f6d gate
 * (hermes_cli/web_server_static_assets.py).
 *
 * Every case materializes the files it names under the isolated SLERMES_HOME
 * (fast_gates hands the harness and the oracle the same temp dir), then runs
 * the port. The font result is reported relative to that home so the two sides
 * compare identical text on any drive/case. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "noc_w21_webservers_cb0f6d.h"
#include "hermes_cli/web_server_static_assets.c"

#if defined(_WIN32)
#define MK_DIR(p) mkdir(p)
#else
#define MK_DIR(p) mkdir((p), 0700)
#endif

static char base[2048];

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Create every missing parent directory of `path` (in place, then restored). */
static void mkparents(char *path) {
    char *p;
    for (p = path + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char sep = *p;
            *p = '\0';
            MK_DIR(path);
            *p = sep;
        }
    }
}

static void make_entry(const char *rel) {
    char path[4096];
    size_t n;
    snprintf(path, sizeof(path), "%s/%s", base, rel ? rel : "");
    n = strlen(path);
    if (n && path[n - 1] == '/') {
        path[n - 1] = '\0';
        mkparents(path);
        MK_DIR(path);
        return;
    }
    mkparents(path);
    {
        FILE *f = fopen(path, "wb");
        if (f) fclose(f);
    }
}

static void drop_relative(const char *abs, char *out, size_t outsz) {
    size_t n = strlen(base);
    if (!strncmp(abs, base, n) && (abs[n] == '/' || abs[n] == '\\')) {
        snprintf(out, outsz, "%s", abs + n + 1);
    } else {
        snprintf(out, outsz, "%s", abs);
    }
}

static void prepare(json_t *in) {
    json_t *files = json_obj_get(in, "files");
    size_t i;
    if (!files || files->type != JSON_ARRAY) return;
    for (i = 0; i < files->c.count; i++) {
        json_t *v = files->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val) make_entry(v->str_val);
    }
}

int main(void) {
    char line[65536];
    const char *home = getenv("SLERMES_HOME");
    if (!home || !home[0]) home = getenv("HOME");
    if (!home || !home[0]) home = getenv("TMPDIR");
    if (!home || !home[0]) home = ".";
    snprintf(base, sizeof(base), "%s", home);
    wsa_slashes(base);

    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func, *full_path;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        full_path = json_get_str(in, "full_path", NULL);
        prepare(in);
        if (!strcmp(func, "is_static_asset")) {
            out = json_bool(w21_webservers_cb0f6d_is_static_asset_path(full_path) != 0);
        } else if (!strcmp(func, "font")) {
            char dist[4096], dash[4096], web[4096], rel[4096];
            char *hit;
            snprintf(dist, sizeof(dist), "%s/dist", base);
            snprintf(dash, sizeof(dash), "%s/dash", base);
            snprintf(web, sizeof(web), "%s/webpublic", base);
            hit = w21_webservers_cb0f6d_public_font_file(dist, dash, web, full_path);
            if (hit) {
                drop_relative(hit, rel, sizeof(rel));
                out = json_string(rel);
                free(hit);
            } else {
                out = json_null();
            }
        } else if (!strcmp(func, "vocabulary")) {
            out = w21_webservers_cb0f6d_vocabulary();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
