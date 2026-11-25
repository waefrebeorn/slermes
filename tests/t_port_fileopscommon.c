#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_fileopscommon.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *two(long long a, long long b) {
    json_t *o = json_object();
    json_set(o, "offset", json_number((double)a));
    json_set(o, "limit", json_number((double)b));
    return o;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;

        if (!strcmp(func, "conflict")) {
            out = json_number((double)fileopscommon_count_conflict_blocks(
                json_get_str(in, "text", "")));
        } else if (!strcmp(func, "fence")) {
            char *s = fileopscommon_strip_terminal_fence_leaks(json_get_str(in, "text", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "fname")) {
            char *s = fileopscommon_filename_match_key(json_get_str(in, "name", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "eol")) {
            const char *s = fileopscommon_detect_line_ending(json_get_str(in, "sample", ""));
            out = s ? json_string(s) : json_null();
        } else if (!strcmp(func, "normeol")) {
            char *s = fileopscommon_normalize_line_endings(
                json_get_str(in, "text", ""), json_get_str(in, "target", ""));
            out = json_string(s ? s : ""); free(s);
        } else if (!strcmp(func, "bom")) {
            char *s = NULL;
            int had = fileopscommon_strip_bom(json_get_str(in, "text", ""), &s);
            out = json_object();
            json_set(out, "text", json_string(s ? s : ""));
            json_set(out, "had_bom", json_bool(had ? true : false));
            free(s);
        } else if (!strcmp(func, "hasbom")) {
            out = json_bool(fileopscommon_has_bom(json_get_str(in, "text", "")) ? true : false);
        } else if (!strcmp(func, "coerce")) {
            int ok = 0;
            long long def = (long long)json_get_num(in, "default", 1.0);
            long long v = fileopscommon_coerce_int(json_get_str(in, "value", ""), def, &ok);
            out = json_object();
            json_set(out, "ok", json_bool(ok ? true : false));
            json_set(out, "value", json_number((double)v));
        } else if (!strcmp(func, "readpage")) {
            long long o, l;
            long long mx = (long long)json_get_num(in, "max_lines", 2000.0);
            fileopscommon_normalize_read_pagination(
                json_get_str(in, "offset", ""), json_get_str(in, "limit", ""), mx, &o, &l);
            out = two(o, l);
        } else if (!strcmp(func, "searchpage")) {
            long long o, l;
            fileopscommon_normalize_search_pagination(
                json_get_str(in, "offset", ""), json_get_str(in, "limit", ""), &o, &l);
            out = two(o, l);
        } else if (!strcmp(func, "lint")) {
            out = fileopscommon_lint_result_to_dict(
                json_get_bool(in, "success", true) ? 1 : 0,
                json_get_bool(in, "skipped", false) ? 1 : 0,
                json_get_str(in, "output", ""), json_get_str(in, "message", ""));
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
