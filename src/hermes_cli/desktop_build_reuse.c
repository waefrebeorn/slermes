/*
 * desktop_build_reuse.c - C home for upstream Hermes hermes_cli/desktop_build_reuse.py.
 *
 * PoP: w18_desktopbui_e30f9a_stamp_drift @ hermes_cli/desktop_build_reuse.py:_stamp_drift
 * PoP: w18_desktopbui_e30f9a_packaged_desktop_reusable @ hermes_cli/desktop_build_reuse.py:packaged_desktop_reusable
 *
 * The two seam points are the blocked ones in the Python source: the installed
 * node binary plus the desktop-reuse subprocess result (stamp drift), and the
 * filesystem stamps / build-needed / drift result (reuse decision). All string
 * data is copied out of json nodes before the nodes are freed.
 */
#include "noc_w18_desktopbui_e30f9a.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct json_t *w18_desktopbui_e30f9a_stamp_drift(const char *binary,
                                                 const char *raw_stdout, int ok) {
    /* installed is None or installed.binary is None -> None. An empty string is
     * a present binary (Python only tests `is None`). */
    if (!binary) return json_null();
    if (!ok) return json_null();
    if (!raw_stdout) raw_stdout = "";

    const char *a = raw_stdout;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t len = (size_t)(b - a);
    if (!len) return json_null();

    /* result.stdout.strip().splitlines()[-1] ; splitlines handles \n, \r, \r\n
     * and drops a trailing empty segment. Keep only the last segment. */
    char last[16384];
    last[0] = '\0';
    int have = 0;
    size_t i = 0;
    while (i < len) {
        size_t j = i;
        while (j < len && a[j] != '\n' && a[j] != '\r') j++;
        size_t seglen = j - i;
        if (seglen > sizeof(last) - 1) seglen = sizeof(last) - 1;
        memcpy(last, a + i, seglen);
        last[seglen] = '\0';
        have = 1;
        if (j >= len) break;
        if (a[j] == '\r' && j + 1 < len && a[j + 1] == '\n') j += 2;
        else j += 1;
        if (j >= len) break;  /* trailing line break: no further empty line */
        i = j;
    }
    if (!have) return json_null();

    json_t *parsed = json_parse(last, NULL);
    if (!parsed) return json_null();
    if (parsed->type == JSON_ARRAY) return parsed;
    json_free(parsed);
    return json_null();
}

/* Structural equality mirroring Python == for JSON containers/scalars. */
static int db_json_equal(const json_t *a, const json_t *b) {
    if (a == b) return 1;
    if (!a || !b || a->type != b->type) return 0;
    switch (a->type) {
        case JSON_NULL:  return 1;
        case JSON_BOOL:  return a->bool_val == b->bool_val;
        case JSON_NUMBER: return a->num_val == b->num_val;
        case JSON_STRING: return !!a->str_val && !!b->str_val && !strcmp(a->str_val, b->str_val);
        case JSON_ARRAY:
            if (json_len(a) != json_len(b)) return 0;
            for (size_t i = 0; i < json_len(a); ++i)
                if (!db_json_equal(json_get(a, i), json_get(b, i))) return 0;
            return 1;
        case JSON_OBJECT:
            if (json_object_size(a) != json_object_size(b)) return 0;
            for (size_t i = 0; i < json_object_size(a); ++i) {
                const char *k = json_object_get_key_at(a, i);
                if (!k) return 0;
                const json_t *vb = json_obj_get(b, k);
                if (!vb || !db_json_equal(json_object_get_at(a, i), vb)) return 0;
            }
            return 1;
    }
    return 0;
}

struct json_t *w18_desktopbui_e30f9a_packaged_desktop_reusable(
    int resources_present, const struct json_t *baked, const struct json_t *prepared,
    int build_needed, const struct json_t *drift) {
    if (!resources_present) return json_null();
    if (!baked || baked->type != JSON_OBJECT) return json_null();
    if (!prepared) return json_null();

    const json_t *cv = json_obj_get(baked, "commit");
    if (!cv || cv->type != JSON_STRING || !cv->str_val || !cv->str_val[0]) return json_null();
    if (!db_json_equal(baked, prepared)) return json_null();
    if (build_needed) return json_null();

    /* `commit if _stamp_drift(...) == [] else None`: only an empty list matches. */
    int drift_empty = (drift && drift->type == JSON_ARRAY && json_len(drift) == 0);
    if (!drift_empty) return json_null();

    char *commit = (char *)malloc(strlen(cv->str_val) + 1);
    if (!commit) return json_null();
    strcpy(commit, cv->str_val);
    json_t *res = json_string(commit);
    free(commit);
    return res;
}
