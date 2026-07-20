/*
 * t_port_cron_prompt_sanitize.c — oracle harness for the PURE cron-prompt
 * sanitization helpers in src/tools/cron_prompt_sanitize.c (port of
 * tools/cronjob_tools.py: _check_invisible_unicode, _strip_invisible_unicode,
 * _scan_cron_skill_assembled).
 *
 * Pure unicode/string transforms (no IO / network). Calls the REAL C
 * functions and emits stable JSON (one object per line) the Python oracle
 * reproduces. Invisible codepoints and emoji are token-encoded in the fixture.
 */

#include "tools/cron_prompt_sanitize.h"
#include "hermes_json.h"   /* json_t internals, json_object_get, json_free, JSON_* */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Decode fixture tokens into real characters:
 *   @ZWSP@ U+200B  @ZWJ@ U+200D  @ZWNJ@ U+200C  @BOM@ U+FEFF
 *   @LTR@ U+200E   @RLM@ U+200F   @EMO@ U+1F600 (grinning emoji) */
static char *decode_tokens(const char *in) {
    if (!in) return NULL;
    size_t n = strlen(in);
    char *out = malloc(n * 4 + 1);
    if (!out) return NULL;
    size_t oi = 0;
    static const char emo[4] = { (char)0xF0, (char)0x9F, (char)0x98, (char)0x80 };
    for (size_t i = 0; i < n; ) {
        if (strncmp(in + i, "@ZWSP@", 6) == 0) { out[oi++] = (char)0xE2; out[oi++] = (char)0x80; out[oi++] = (char)0x8B; i += 6; }
        else if (strncmp(in + i, "@ZWJ@", 5) == 0) { out[oi++] = (char)0xE2; out[oi++] = (char)0x80; out[oi++] = (char)0x8D; i += 5; }
        else if (strncmp(in + i, "@ZWNJ@", 6) == 0) { out[oi++] = (char)0xE2; out[oi++] = (char)0x80; out[oi++] = (char)0x8C; i += 6; }
        else if (strncmp(in + i, "@BOM@", 5) == 0) { out[oi++] = (char)0xEF; out[oi++] = (char)0xBB; out[oi++] = (char)0xBF; i += 5; }
        else if (strncmp(in + i, "@LTR@", 5) == 0) { out[oi++] = (char)0xE2; out[oi++] = (char)0x80; out[oi++] = (char)0x8E; i += 5; }
        else if (strncmp(in + i, "@RLM@", 5) == 0) { out[oi++] = (char)0xE2; out[oi++] = (char)0x80; out[oi++] = (char)0x8F; i += 5; }
        else if (strncmp(in + i, "@EMO@", 5) == 0) { memcpy(out + oi, emo, 4); oi += 4; i += 5; }
        else { out[oi++] = in[i++]; }
    }
    out[oi] = '\0';
    return out;
}

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

/* Emit a JSON node's "cleaned"/"removed" (strip) or "cleaned"/"error" (scan) shape. */
static void emit_kv2(const char *tag, json_t *obj, const char *k1, const char *k2) {
    json_t *a = json_object_get(obj, k1);
    json_t *b = json_object_get(obj, k2);
    printf(",\"%s\":", k1); emit_json_string(a && a->type == JSON_STRING ? a->str_val : "");
    printf(",\"%s\":", k2);
    if (b && b->type == JSON_ARRAY) {
        printf("[");
        for (int i = 0; i < (int)json_array_size(b); i++) {
            if (i) printf(",");
            json_t *e = json_array_get(b, i);
            emit_json_string(e && e->type == JSON_STRING ? e->str_val : "");
        }
        printf("]");
    } else if (b && b->type == JSON_STRING) {
        emit_json_string(b->str_val);
    } else {
        printf("\"\"");
    }
}

static void split_kv(const char *line, char *key, size_t ksz, const char **val) {
    size_t i = 0;
    while (*line && *line != ' ' && i + 1 < ksz) key[i++] = *line++;
    key[i] = '\0';
    if (*line == ' ') line++;
    *val = line;
}

int main(void) {
    cron_prompt_sanitize_t *ctx = cron_prompt_sanitize_init();
    char line[16384];
    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_kv(line, op, sizeof(op), &rest);
        char *v = decode_tokens(rest[0] ? rest : "");
        const char *s = v ? v : "";

        if (strcmp(op, "check") == 0) {
            char *err = cron_prompt_sanitize_check_invisible(s);
            printf("{\"op\":\"check\",\"in\":");
            emit_json_string(s); printf(",\"error\":");
            emit_json_string(err ? err : ""); printf("}\n");
            free(err);

        } else if (strcmp(op, "strip") == 0) {
            json_t *obj = cron_prompt_sanitize_strip_invisible(s);
            printf("{\"op\":\"strip\",\"in\":");
            emit_json_string(s);
            emit_kv2("strip", obj, "cleaned", "removed");
            printf("}\n");
            if (obj) json_free(obj);

        } else if (strcmp(op, "scan") == 0) {
            json_t *obj = cron_prompt_sanitize_scan_skill_assembled(s);
            printf("{\"op\":\"scan\",\"in\":");
            emit_json_string(s);
            emit_kv2("scan", obj, "cleaned", "error");
            printf("}\n");
            if (obj) json_free(obj);

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
        free(v);
    }
    cron_prompt_sanitize_free(ctx);
    return 0;
}
