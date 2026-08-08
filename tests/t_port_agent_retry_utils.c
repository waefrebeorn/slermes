/* Oracle harness: agent/retry_utils.py vs LIVE Python.
 *
 * Reads JSON array of {func, ...args} cases from argv[1]; runs the matching C
 * function on each; emits one JSON line per case.
 *
 * The Python oracle (tests/sta_oracle_agent_retry_utils.py) imports the LIVE
 * agent.retry_utils module and recomputes the same function on each case;
 * the runner diffs the two JSONL streams byte-for-byte.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_retry_utils.h"

/* ------------------------------------------------------------------ */
/* File slurp                                                          */
/* ------------------------------------------------------------------ */

static char *read_all(const char *path)
{
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

/* ------------------------------------------------------------------ */
/* jstr: JSON-encode a NUL-terminated string into a static buffer      */
/* ------------------------------------------------------------------ */

static const char *jstr(const char *s)
{
    static char b[4][1024];
    static int bi = 0;
    int idx = bi;
    char *q = b[idx];
    bi = (bi + 1) & 3;
    *q++ = '"';
    for (const char *p = s; p && *p && (q - b[idx]) < 1000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c < 0x20) {
            *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
            *q++ = "0123456789abcdef"[c >> 4];
            *q++ = "0123456789abcdef"[c & 0xf];
        } else {
            *q++ = (char)c;
        }
    }
    *q++ = '"';
    *q = '\0';
    return b[idx];
}

/* ------------------------------------------------------------------ */
/* Minimal JSON string parser (handles \n, \uXXXX, etc.)              */
/* ------------------------------------------------------------------ */

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/* Parse a JSON string literal at **pp (pointing at opening ").
 * Returns malloc'd string, advances *pp past closing quote. */
static char *parse_json_string(const char **pp)
{
    const char *p = *pp;
    if (*p != '"') return NULL;
    p++;
    char *out = NULL;
    size_t len = 0, cap = 0;
    while (*p && *p != '"') {
        char c;
        if (*p == '\\' && p[1]) {
            p++;
            char e = *p;
            switch (e) {
                case '"':  c = '"';  break;
                case '\\': c = '\\'; break;
                case '/':  c = '/';  break;
                case 'n':  c = '\n'; break;
                case 'r':  c = '\r'; break;
                case 't':  c = '\t'; break;
                case 'b':  c = '\b'; break;
                case 'f':  c = '\f'; break;
                case 'u': {
                    unsigned int cp = 0;
                    if (p[1] && p[2] && p[3] && p[4]) {
                        sscanf(p + 1, "%4x", &cp);
                    }
                    p += 4;
                    if (cp < 0x80) {
                        c = (char)cp;
                    } else if (cp < 0x800) {
                        if (len + 2 >= cap) { cap = cap ? cap * 2 : 64; out = (char *)realloc(out, cap); }
                        if (!out) abort();
                        out[len++] = (char)(0xC0 | (cp >> 6));
                        c = (char)(0x80 | (cp & 0x3F));
                    } else {
                        if (len + 3 >= cap) { cap = cap ? cap * 2 : 64; out = (char *)realloc(out, cap); }
                        if (!out) abort();
                        out[len++] = (char)(0xE0 | (cp >> 12));
                        out[len++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        c = (char)(0x80 | (cp & 0x3F));
                    }
                    if (len >= cap) { cap = cap ? cap * 2 : 64; out = (char *)realloc(out, cap); }
                    if (!out) abort();
                    out[len++] = c;
                    p++;
                    continue;
                }
                default: c = e; break;
            }
            p++;
        } else {
            c = *p++;
        }
        if (len >= cap) { cap = cap ? cap * 2 : 64; out = (char *)realloc(out, cap); }
        if (!out) abort();
        out[len++] = c;
    }
    if (*p == '"') p++;
    *pp = p;
    if (len >= cap) { cap = len + 1; out = (char *)realloc(out, cap); }
    if (!out) { out = (char *)malloc(1); out[0] = '\0'; return out; }
    out[len] = '\0';
    return out;
}

static double parse_json_number(const char **pp)
{
    const char *p = *pp;
    char *end = NULL;
    double val = strtod(p, &end);
    if (end != p) {
        *pp = end;
        return val;
    }
    return 0.0;
}

/* ------------------------------------------------------------------ */
/* Case type                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *func;
    char  *base;
    char  *model;
    int    status;
    char  *text;
    char  *value;
    int    has_value;
    int    has_numeric;
    double numeric;
} retry_case_t;

static void free_case(retry_case_t *c)
{
    free(c->func);
    free(c->base);
    free(c->model);
    free(c->text);
    free(c->value);
}

/* Parse one JSON object { "func": ..., ... } into a retry_case_t. */
static int parse_case(const char *obj_start, const char *obj_end, retry_case_t *c)
{
    memset(c, 0, sizeof(*c));
    const char *p = obj_start;

    while (p < obj_end) {
        p = skip_ws(p);
        if (p >= obj_end || *p != '"') break;
        char *key = parse_json_string(&p);
        if (!key) break;
        p = skip_ws(p);
        if (*p != ':') { free(key); break; }
        p = skip_ws(p + 1);

        if (strcmp(key, "func") == 0) {
            c->func = parse_json_string(&p);
        } else if (strcmp(key, "base") == 0) {
            c->base = parse_json_string(&p);
        } else if (strcmp(key, "model") == 0) {
            c->model = parse_json_string(&p);
        } else if (strcmp(key, "text") == 0) {
            c->text = parse_json_string(&p);
        } else if (strcmp(key, "status") == 0) {
            c->status = (int)parse_json_number(&p);
        } else if (strcmp(key, "value") == 0) {
            p = skip_ws(p);
            if (p < obj_end && *p == 'n') {
                /* null — has_value stays 0 */
                if (p + 4 <= obj_end && strncmp(p, "null", 4) == 0)
                    p += 4;
            } else {
                c->value = parse_json_string(&p);
                c->has_value = 1;
            }
        } else if (strcmp(key, "numeric") == 0) {
            /* Handle null, string, or number */
            p = skip_ws(p);
            if (p < obj_end && *p == 'n') {
                /* null — has_numeric stays 0 */
                if (p + 4 <= obj_end && strncmp(p, "null", 4) == 0)
                    p += 4;
            } else if (p < obj_end && *p == '"') {
                char *tmp = parse_json_string(&p);
                free(tmp);
            } else {
                c->numeric = parse_json_number(&p);
                c->has_numeric = 1;
            }
        } else {
            /* Skip unknown value */
            if (*p == '"') {
                char *tmp = parse_json_string(&p);
                free(tmp);
            } else {
                parse_json_number(&p);
            }
        }

        free(key);
        p = skip_ws(p);
        if (p < obj_end && *p == ',') p++;
    }

    return c->func != NULL ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Dispatch + emit                                                     */
/* ------------------------------------------------------------------ */

static void emit_zai(const retry_case_t *c)
{
    retry_utils_err_t e;
    e.status_code = c->status;
    snprintf(e.text, sizeof(e.text), "%s", c->text ? c->text : "");
    int r = retry_utils_is_zai_coding_overload_error(
        c->base, c->model, &e) ? 1 : 0;
    printf("{\"func\":\"%s\",\"code\":%d,\"base\":%s,\"model\":%s,\"text\":%s,\"out\":%d}\n",
           c->func,
           c->status,
           (c->base  ? jstr(c->base)  : "null"),
           (c->model ? jstr(c->model) : "null"),
           (c->text  ? jstr(c->text)  : "null"),
           r);
}

static void emit_retry_after(const retry_case_t *c)
{
    int ok = 0;
    double val = 0.0;

    if (c->has_value) {
        val = retry_utils_parse_retry_after_seconds(c->value, &ok);
    } else if (c->has_numeric) {
        /* Numeric input (int/float) — Python returns max(0.0, float(raw)) */
        ok = 1;
        val = c->numeric > 0.0 ? c->numeric : 0.0;
    }

    /* Build numeric field string: actual value when has_numeric, else null */
    char numbuf[64];
    const char *num_str = "null";
    if (c->has_numeric) {
        snprintf(numbuf, sizeof(numbuf), "%.17g", c->numeric);
        num_str = numbuf;
    }

    if (ok)
        printf("{\"func\":\"%s\",\"value\":%s,\"numeric\":%s,\"ok\":1,\"out\":%.17g}\n",
               c->func,
               (c->value ? jstr(c->value) : "null"),
               num_str, val);
    else
        printf("{\"func\":\"%s\",\"value\":%s,\"numeric\":%s,\"ok\":0,\"out\":null}\n",
               c->func,
               (c->value ? jstr(c->value) : "null"),
               num_str);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <cases.in>\n", argv[0]);
        return 2;
    }
    char *json = read_all(argv[1]);
    if (!json) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    const char *p = skip_ws(json);
    if (*p != '[') {
        fprintf(stderr, "expected JSON array\n");
        free(json);
        return 2;
    }
    p++;

    const char *end = json + strlen(json);
    while (1) {
        p = skip_ws(p);
        if (p >= end || *p == ']') break;
        if (*p != '{') break;

        /* Find matching close brace */
        int depth = 1;
        const char *q = p + 1;
        while (q < end && depth > 0) {
            if (*q == '{') depth++;
            else if (*q == '}') depth--;
            if (depth > 0) q++;
        }
        const char *obj_start = p + 1;

        retry_case_t c;
        if (parse_case(obj_start, q, &c) == 0) {
            if (c.func && strcmp(c.func, "parse_retry_after_seconds") == 0)
                emit_retry_after(&c);
            else
                emit_zai(&c);
        }
        free_case(&c);

        p = q + 1;
        p = skip_ws(p);
        if (p < end && *p == ',') p++;
    }

    free(json);
    return 0;
}
