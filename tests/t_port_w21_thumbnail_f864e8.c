/* JSON-lines harness for the w21_thumbnail_f864e8 gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_thumbnail_f864e8.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* "hex" -> raw bytes for the data_url branch (odd/invalid input yields null). */
static unsigned char *hex_decode(const char *hex, size_t *out_len) {
    size_t n = hex ? strlen(hex) : 0, i;
    unsigned char *out;
    if (n == 0 || (n % 2)) return NULL;
    out = (unsigned char *)malloc(n / 2);
    if (!out) return NULL;
    for (i = 0; i < n / 2; i++) {
        int hv, lv;
        char a = hex[2 * i], b = hex[2 * i + 1];
        hv = (a >= '0' && a <= '9') ? a - '0' : (a >= 'a' && a <= 'f') ? a - 'a' + 10
             : (a >= 'A' && a <= 'F') ? a - 'A' + 10 : -1;
        lv = (b >= '0' && b <= '9') ? b - '0' : (b >= 'a' && b <= 'f') ? b - 'a' + 10
             : (b >= 'A' && b <= 'F') ? b - 'A' + 10 : -1;
        if (hv < 0 || lv < 0) { free(out); return NULL; }
        out[i] = (unsigned char)((hv << 4) | lv);
    }
    *out_len = n / 2;
    return out;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "size")) {
            out = w21_thumbnail_f864e8_thumbnail_size(
                (long long)json_get_num(in, "width", 0.0), (long long)json_get_num(in, "height", 0.0),
                (long long)json_get_num(in, "max_width", 960.0),
                (long long)json_get_num(in, "max_height", 600.0));
        } else if (!strcmp(func, "data_url")) {
            size_t len = 0;
            unsigned char *bytes = hex_decode(json_get_str(in, "hex", NULL), &len);
            char *url = w21_thumbnail_f864e8_data_url(bytes, len);
            out = url ? json_string(url) : json_null();
            free(url);
            free(bytes);
        } else if (!strcmp(func, "flow")) {
            out = w21_thumbnail_f864e8_thumbnail_data_url(in);
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
