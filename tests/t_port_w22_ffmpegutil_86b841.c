/* JSON-lines harness for the w22_ffmpegutil_86b841 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The winget branch is a real directory walk, so a case may carry "seed": a
 * list of paths relative to LOCALAPPDATA that the harness creates (directories
 * plus empty files) before resolving. The oracle seeds the identical tree from
 * the same key, so both sides scan the same bytes. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "noc_w22_ffmpegutil_86b841.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static void make_tree(const char *relative) {
    char buf[4096];
    size_t i, n, cut;
    n = strlen(relative);
    if (n == 0 || n >= sizeof(buf)) return;
    memcpy(buf, relative, n + 1);
    /* the final component is a file: create its parent directories only */
    cut = 0;
    for (i = 0; i < n; i++) {
        if (buf[i] == '/' || buf[i] == '\\') cut = i;
    }
    if (cut == 0) return;
    buf[cut] = '\0';
    for (i = 1; buf[i]; i++) {
        if (buf[i] == '/' || buf[i] == '\\') {
            char keep = buf[i];
            buf[i] = '\0';
            mkdir(buf);
            buf[i] = keep;
        }
    }
    mkdir(buf);
}

static void seed_case(const char *local_appdata, const json_t *seed) {
    size_t i;
    if (!local_appdata || !local_appdata[0]) return;
    if (!seed || seed->type != JSON_ARRAY) return;
    for (i = 0; i < seed->c.count; i++) {
        json_t *item = seed->c.items[i];
        char full[4096];
        const char *rel;
        int n;
        FILE *fp;
        if (!item || item->type != JSON_STRING || !item->str_val) continue;
        rel = item->str_val;
        if (rel[0] == '/') continue;              /* refuse an absolute seed */
        n = snprintf(full, sizeof(full), "%s/%s", local_appdata, rel);
        if (n <= 0 || n >= (int)sizeof(full)) continue;
        make_tree(full);
        fp = fopen(full, "ab");
        if (fp) fclose(fp);
    }
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "expand")) {
            const char *value = json_get_str(in, "value", NULL);
            char *got = w22_ffmpegutil_86b841_expand_path(value);
            out = got ? json_string(got) : json_null();
            free(got);
        } else if (!strcmp(func, "resolve")) {
            const char *local_raw = json_get_str(in, "local_appdata", NULL);
            char *local_abs = local_raw ? w22_ffmpegutil_86b841_expand_path(local_raw) : NULL;
            char *got;
            seed_case(local_abs, json_obj_get(in, "seed"));
            got = w22_ffmpegutil_86b841_resolve(json_get_str(in, "ffmpeg_path", NULL),
                                                json_get_str(in, "shared", NULL),
                                                local_abs);
            out = got ? json_string(got) : json_null();
            free(got);
            free(local_abs);
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
