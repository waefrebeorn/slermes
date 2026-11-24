/* JSON-lines harness for the w33_portabilit_b54bb9 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" for that case. */
#include "hermes_json.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_portabilit_b54bb9.h"

#define LINE_CAP (16u * 1024u * 1024u)

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "constants")) return w33_portabilit_b54bb9_constants();
    if (!strcmp(func, "export_row"))
        return w33_portabilit_b54bb9_export_row(json_obj_get(in, "session"));
    if (!strcmp(func, "text_or_none"))
        return w33_portabilit_b54bb9_text_or_none(json_obj_get(in, "value"), json_get_str(in, "field", ""));
    if (!strcmp(func, "int_or_none"))
        return w33_portabilit_b54bb9_int_or_none(json_obj_get(in, "value"), json_get_str(in, "field", ""));
    if (!strcmp(func, "int_or_bounded"))
        return w33_portabilit_b54bb9_int_or_bounded(json_obj_get(in, "value"), json_get_str(in, "field", ""),
                                                    json_obj_get(in, "default"));
    if (!strcmp(func, "float_or_finite"))
        return w33_portabilit_b54bb9_float_or_finite(json_obj_get(in, "value"), json_get_str(in, "field", ""));
    if (!strcmp(func, "json_object_or_none"))
        return w33_portabilit_b54bb9_json_object_or_none(json_obj_get(in, "value"), json_get_str(in, "field", ""));
    if (!strcmp(func, "non_finite_path"))
        return w33_portabilit_b54bb9_non_finite_path(json_obj_get(in, "value"), json_get_str(in, "path", ""));
    if (!strcmp(func, "load_json"))
        return w33_portabilit_b54bb9_load_json(json_get_str(in, "text", ""), json_get_str(in, "path", ""));
    if (!strcmp(func, "message_content"))
        return w33_portabilit_b54bb9_message_content(json_obj_get(in, "content"), json_get_str(in, "path", ""));
    if (!strcmp(func, "normalize_message"))
        return w33_portabilit_b54bb9_normalize_message(json_obj_get(in, "message"),
                                                       (int)json_get_num(in, "index", 0.0));
    if (!strcmp(func, "normalize_session")) {
        const json_t *msgs = json_obj_get(in, "messages");
        if (!msgs) msgs = json_obj_get(json_obj_get(in, "session"), "messages");
        return w33_portabilit_b54bb9_normalize_session(json_obj_get(in, "session"),
                                                       json_get_str(in, "session_id", ""), msgs);
    }
    if (!strcmp(func, "validate_payload"))
        return w33_portabilit_b54bb9_validate_payload(json_obj_get(in, "sessions"));
    if (!strcmp(func, "foreign_match"))
        return w33_portabilit_b54bb9_foreign_match(json_obj_get(in, "rows"), json_obj_get(in, "origin"));
    if (!strcmp(func, "parent_attach"))
        return w33_portabilit_b54bb9_parent_attach(json_obj_get(in, "parent_updates"),
                                                   json_obj_get(in, "rows"));
    if (!strcmp(func, "import_flags"))
        return w33_portabilit_b54bb9_import_flags(json_obj_get(in, "raw"));
    if (!strcmp(func, "message_split"))
        return w33_portabilit_b54bb9_message_split(json_obj_get(in, "messages"));
    return json_string("UNKNOWN_FUNC");
}

static void echo_num(double v) {
    char tmp[64];
    int prec;
    if (!isfinite(v)) { fputs(v > 0 ? "1e309" : "-1e309", stdout); return; }
    if (v == floor(v) && fabs(v) < 1e21) {
        printf("%.0f", v);
        return;
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    fputs(tmp, stdout);
}

static void echo_str(const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    putchar('"');
    for (; *p; p++) {
        switch (*p) {
        case '"':  fputs("\\\"", stdout); break;
        case '\\': fputs("\\\\", stdout); break;
        case '\b': fputs("\\b", stdout); break;
        case '\f': fputs("\\f", stdout); break;
        case '\n': fputs("\\n", stdout); break;
        case '\r': fputs("\\r", stdout); break;
        case '\t': fputs("\\t", stdout); break;
        default:
            if (*p < 0x20) printf("\\u%04x", (unsigned)*p);
            else putchar((char)*p);
        }
    }
    putchar('"');
}

static void echo_val(const json_t *v) {
    size_t i;
    if (!v || v->type == JSON_NULL) { fputs("null", stdout); return; }
    switch (v->type) {
    case JSON_BOOL: fputs(v->bool_val ? "true" : "false", stdout); return;
    case JSON_NUMBER: echo_num(v->num_val); return;
    case JSON_STRING: echo_str(v->str_val); return;
    case JSON_ARRAY:
        putchar('[');
        for (i = 0; i < json_len(v); i++) {
            if (i) putchar(',');
            echo_val(json_get(v, i));
        }
        putchar(']');
        return;
    case JSON_OBJECT:
        putchar('{');
        for (i = 0; i < json_object_size(v); i++) {
            const char *k = json_object_get_key_at(v, i);
            if (i) putchar(',');
            echo_str(k ? k : "");
            putchar(':');
            echo_val(json_object_get_at(v, i));
        }
        putchar('}');
        return;
    default:
        fputs("null", stdout);
        return;
    }
}

int main(void) {
    char *line = (char *)malloc(LINE_CAP);
    if (!line) return 1;
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, (int)LINE_CAP, stdin)) {
        json_t *in, *out;
        const char *func;
        size_t i;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        out = dispatch(in, func);
        fputs("{\"func\":", stdout);
        echo_str(func);
        fputs(",\"case\":", stdout);
        echo_val(json_obj_get(in, "case"));
        fputs(",\"out\":", stdout);
        {
            char *s = json_serialize(out);
            fputs(s ? s : "null", stdout);
            free(s);
        }
        for (i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func") || !strcmp(k, "case")) continue;
            putchar(',');
            echo_str(k);
            putchar(':');
            echo_val(json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    free(line);
    return 0;
}
