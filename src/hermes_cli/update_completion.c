/*
 * update_completion.c — C port of upstream Hermes hermes_cli/update_completion.py.
 *
 * PoP: w16_updatecomp_c7d826_exit_status @ hermes_cli/update_completion.py:_exit_status
 * PoP: w16_updatecomp_c7d826_owed_user_action @ hermes_cli/update_completion.py:_owed_user_action
 * PoP: w16_updatecomp_c7d826_write_bootstrap_result @ hermes_cli/update_completion.py:_write_bootstrap_result
 * PoP: w16_updatecomp_c7d826_report_desktop_build_owed @ hermes_cli/update_completion.py:_report_desktop_build_owed
 *
 * File/print side effects are seams: the bootstrap result is returned as the dict
 * that would be written and the report helper returns the lines it would print.
 */
#include "noc_w16_updatecomp_c7d826.h"
#include "hermes_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* DESKTOP_BUILD_OWED @ hermes_cli/update_completion.py:DESKTOP_BUILD_OWED */
static const char *const DESKTOP_BUILD_OWED = "Desktop app build owed:";

static char *dupstr(const char *s) {
    if (!s) s = "";
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

static int json_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
        default: return 0;
    }
}

static char *fmt_line(const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (n < 0) { va_end(ap2); return dupstr(""); }
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) { va_end(ap2); return dupstr(""); }
    vsnprintf(b, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return b;
}

double w16_updatecomp_c7d826_exit_status(double code) {
    return code >= 0 ? code : 128.0 - code;
}

struct json_t *w16_updatecomp_c7d826_owed_user_action(const json_t *request) {
    const json_t *m = json_obj_get(request, "completion_message");
    if (!json_truthy(m) || m->type != JSON_STRING) return json_null();
    const char *s = m->str_val ? m->str_val : "";
    /* Python: message if message and not message.startswith("\xe2\x9c\x93") else None */
    if (strncmp(s, "\xe2\x9c\x93", 3) == 0) return json_null();
    return json_string(s);
}

struct json_t *w16_updatecomp_c7d826_write_bootstrap_result(const json_t *request,
                                                            double code,
                                                            const json_t *receipt) {
    struct json_t *out = json_object();
    json_set(out, "schema", json_number(1));
    const json_t *r = json_obj_get(request, "receipt");
    const json_t *uid = json_is_object(r) ? json_obj_get(r, "update_id") : NULL;
    json_set(out, "update_id", uid ? json_copy(uid) : json_null());
    json_set(out, "exit_code", json_number(code));
    json_set(out, "receipt", receipt ? json_copy(receipt) : json_null());
    json_set(out, "windows_resume", json_null());
    const json_t *pm = json_obj_get(request, "pm_receipt");
    json_set(out, "pm_receipt", pm ? json_copy(pm) : json_null());
    return out;
}

struct json_t *w16_updatecomp_c7d826_report_desktop_build_owed(const json_t *request,
                                                               const json_t *record,
                                                               const char *reason) {
    struct json_t *lines = json_array();
    if (!json_truthy(json_obj_get(request, "desktop"))) return lines;
    if (json_is_object(record)) {
        const json_t *stages = json_obj_get(record, "stages");
        if (json_is_array(stages)) {
            for (size_t i = 0; i < json_len(stages); i++) {
                const json_t *mark = json_get(stages, i);
                if (json_is_object(mark)) {
                    const json_t *name = json_obj_get(mark, "name");
                    if (name && name->type == JSON_STRING && name->str_val &&
                        !strcmp(name->str_val, "build")) return lines;
                }
            }
        }
    }
    char *line = fmt_line("  %s %s", DESKTOP_BUILD_OWED, reason ? reason : "");
    json_append(lines, json_string(line));
    free(line);
    return lines;
}
