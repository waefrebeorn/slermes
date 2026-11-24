/* JSON-lines harness for the w21_desktopupd_4b3eb1 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash:
 * an absent number reads as 0 and an absent flag as false.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_desktopupd_4b3eb1.h"
#include "hermes_cli/desktop_update_verify.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* A number, or a decimal string, as the decimal text the port reads. */
static const char *num_text(json_t *in, const char *key, char *buf, size_t cap) {
    json_t *v = json_obj_get(in, key);
    if (!v) { buf[0] = '\0'; return buf; }
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%.17g", v->num_val);
        return buf;
    }
    if (v->type == JSON_STRING && v->str_val) {
        snprintf(buf, cap, "%s", v->str_val);
        return buf;
    }
    buf[0] = '\0';
    return buf;
}

static int flag(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    default: return 0;
    }
}

int main(void) {
    char line[131072];
    char a[64], b[64], c[64], d[64], e[64];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "root_to_verify")) {
            char *s = w21_desktopupd_4b3eb1_root_to_verify(json_get_str(in, "project_root", NULL),
                                                          json_get_str(in, "checkout_root", ""),
                                                          flag(in, "has_packaged"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "asar_header_check")) {
            char *s = w21_desktopupd_4b3eb1_asar_header_check(num_text(in, "size", a, sizeof(a)),
                                                             num_text(in, "header_size", b, sizeof(b)),
                                                             num_text(in, "payload_size", c, sizeof(c)),
                                                             num_text(in, "json_size", d, sizeof(d)),
                                                             num_text(in, "file_size", e, sizeof(e)));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "asar_entry_path_check")) {
            char *s = w21_desktopupd_4b3eb1_asar_entry_path_check(json_get_str(in, "name", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "asar_entry_check")) {
            char *s = w21_desktopupd_4b3eb1_asar_entry_check(json_get_str(in, "name", ""),
                                                            num_text(in, "length", a, sizeof(a)),
                                                            num_text(in, "header_size", b, sizeof(b)),
                                                            num_text(in, "offset", c, sizeof(c)),
                                                            flag(in, "unpacked"),
                                                            num_text(in, "file_size", d, sizeof(d)),
                                                            num_text(in, "read_len", e, sizeof(e)),
                                                            flag(in, "data_blank"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "renderer_has_module_entry")) {
            out = json_bool(w21_desktopupd_4b3eb1_renderer_has_module_entry(json_get_str(in, "html", "")));
        } else if (!strcmp(func, "verify")) {
            out = w21_desktopupd_4b3eb1_verify(in);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        (void)0;
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
        json_free(out);
        json_free(in);
    }
    return 0;
}
