/* port_agent_runtime_helpers_flow.c — FLOW-core port of agent/agent_runtime_helpers.py.
 * See include/port_agent_runtime_helpers_flow.h for the seam table and the
 * representation contract. libjson-only; no real I/O inside the decision core.
 */
#include "port_agent_runtime_helpers_flow.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ── seam dispatch ─────────────────────────────────────────────────────── */
json_t *arhf_call(arhf_io_t *io, const char *name, json_t *args) {
    json_t *ev = json_object();
    json_set_hdr(ev, "name", name);
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_append(io->trace, ev);
    if (!io->call) return json_null();
    return io->call(io, name, args);
}

bool arhf_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

/* ── Python truthiness / scalar str() ──────────────────────────────────── */
static bool jtruth(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static bool j_is_str(const json_t *v) { return v && v->type == JSON_STRING; }
static bool j_is_obj(const json_t *v) { return v && v->type == JSON_OBJECT; }
static bool j_is_arr(const json_t *v) { return v && v->type == JSON_ARRAY; }
static bool j_is_num(const json_t *v) { return v && v->type == JSON_NUMBER; }

/* shortest round-trip repr of a double (Python float repr semantics for the
 * fixture range): try 15/16/17 significant digits, first that round-trips. */
static void dtoa_repr(double x, char *buf, size_t cap) {
    if (isnan(x)) { snprintf(buf, cap, "nan"); return; }
    if (isinf(x)) { snprintf(buf, cap, x < 0 ? "-inf" : "inf"); return; }
    for (int prec = 15; prec <= 17; prec++) {
        snprintf(buf, cap, "%.*g", prec, x);
        if (strtod(buf, NULL) == x) return;
    }
    snprintf(buf, cap, "%.17g", x);
}

/* Python str() of a JSON scalar. Containers are out of fixture scope. */
static char *py_str_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        double x = v->num_val;
        if (isfinite(x) && x == floor(x) && fabs(x) < 1e16) {
            snprintf(buf, sizeof buf, "%.0f", x);
        } else {
            dtoa_repr(x, buf, sizeof buf);
        }
        return strdup(buf);
    }
    return strdup("");  /* containers excluded from fixtures */
}

/* non-finite float projection shared with the oracle */
static json_t *num_node(double x) {
    if (!isfinite(x)) {
        json_t *o = json_object();
        json_set_hdr(o, "__float__", isnan(x) ? "nan" : (x < 0 ? "-inf" : "inf"));
        return o;
    }
    return json_number(x);
}

/* ── ASCII whitespace strip / lower (fixture scope: ASCII) ─────────────── */
static bool py_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'
        || (c >= 0x1c && c <= 0x1f);
}

static char *py_strip(const char *s) {
    if (!s) return strdup("");
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && py_isspace((unsigned char)s[a])) a++;
    while (b > a && py_isspace((unsigned char)s[b - 1])) b--;
    char *out = malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    return out;
}

static void py_rstrip_inplace(char *s) {
    size_t n = strlen(s);
    while (n > 0 && py_isspace((unsigned char)s[n - 1])) n--;
    s[n] = '\0';
}

static char *py_lower(const char *s) {
    char *out = strdup(s ? s : "");
    for (char *p = out; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p += 32;
    }
    return out;
}

/* ── Python float() grammar (ASCII): sign, underscores between digits,
 *    decimal/exponent forms, inf/infinity/nan ──────────────────────────── */
static bool py_float_parse(const char *s, double *out) {
    if (!s) return false;
    const char *p = s;
    while (py_isspace((unsigned char)*p)) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    /* inf / infinity / nan (case-insensitive) */
    {
        size_t rest = strlen(p);
        char *tail = malloc(rest + 1);
        for (size_t i = 0; i < rest; i++) {
            char c = p[i];
            tail[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        tail[rest] = '\0';
        /* trim trailing whitespace for the token test */
        size_t tn = rest;
        while (tn > 0 && py_isspace((unsigned char)tail[tn - 1])) tn--;
        tail[tn] = '\0';
        bool special = false;
        double val = 0.0;
        if (strcmp(tail, "inf") == 0 || strcmp(tail, "infinity") == 0) { special = true; val = INFINITY; }
        else if (strcmp(tail, "nan") == 0) { special = true; val = NAN; }
        free(tail);
        if (special) {
            *out = (start[0] == '-') ? -val : val;
            return true;
        }
    }
    /* numeric grammar with underscores between digits */
    char clean[512];
    size_t cn = 0;
    bool prev_digit = false, any_digit = false, dot = false, exp = false;
    const char *q = p;
    while (*q) {
        char c = *q;
        if (c >= '0' && c <= '9') {
            if (cn + 1 >= sizeof clean) return false;
            clean[cn++] = c;
            prev_digit = true; any_digit = true;
        } else if (c == '_') {
            if (!prev_digit) return false;
            /* underscore must be followed by a digit (or dot after digits) */
            if (!(q[1] >= '0' && q[1] <= '9')) return false;
            prev_digit = false;
        } else if (c == '.' && !dot && !exp) {
            if (cn + 1 >= sizeof clean) return false;
            clean[cn++] = c;
            dot = true; prev_digit = false;
        } else if ((c == 'e' || c == 'E') && !exp && any_digit) {
            if (cn + 1 >= sizeof clean) return false;
            clean[cn++] = 'e';
            exp = true; prev_digit = false;
            if (q[1] == '+' || q[1] == '-') {
                if (cn + 1 >= sizeof clean) return false;
                clean[cn++] = q[1];
                q++;
            }
        } else {
            break;
        }
        q++;
    }
    if (!any_digit) return false;
    /* trailing whitespace only */
    while (py_isspace((unsigned char)*q)) q++;
    if (*q != '\0') return false;
    /* a bare dot with no digits on either side is invalid; "5." and ".5" ok */
    clean[cn] = '\0';
    if (strcmp(clean, ".") == 0) return false;
    /* exponent must have digits */
    {
        char *e = strchr(clean, 'e');
        if (e) {
            char *d = e + 1;
            if (*d == '+' || *d == '-') d++;
            if (!(*d >= '0' && *d <= '9')) return false;
        }
    }
    char full[520];
    snprintf(full, sizeof full, "%s%s", (start[0] == '-') ? "-" : "", clean);
    *out = strtod(full, NULL);
    return true;
}

/* ── Python json.dumps(ensure_ascii=False) byte-exact for the fixture
 *    surface: insertion-order keys, ", "/": " separators, Python escape
 *    set, integral doubles < 1e16 as integers, shortest-repr floats ────── */
typedef struct { char *buf; size_t len, cap; } sbuf_t;

static void sb_init(sbuf_t *b) { b->cap = 256; b->len = 0; b->buf = malloc(b->cap); b->buf[0] = '\0'; }
static void sb_putc(sbuf_t *b, char c) {
    if (b->len + 2 > b->cap) { b->cap *= 2; b->buf = realloc(b->buf, b->cap); }
    b->buf[b->len++] = c; b->buf[b->len] = '\0';
}
static void sb_puts(sbuf_t *b, const char *s) {
    size_t n = strlen(s);
    while (b->len + n + 1 > b->cap) { b->cap *= 2; b->buf = realloc(b->buf, b->cap); }
    memcpy(b->buf + b->len, s, n); b->len += n; b->buf[b->len] = '\0';
}

static void dumps_str(sbuf_t *b, const char *s) {
    sb_putc(b, '"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': sb_puts(b, "\\\""); break;
            case '\\': sb_puts(b, "\\\\"); break;
            case '\n': sb_puts(b, "\\n"); break;
            case '\r': sb_puts(b, "\\r"); break;
            case '\t': sb_puts(b, "\\t"); break;
            case '\b': sb_puts(b, "\\b"); break;
            case '\f': sb_puts(b, "\\f"); break;
            default:
                if (c < 0x20) {
                    char esc[8];
                    snprintf(esc, sizeof esc, "\\u%04x", c);
                    sb_puts(b, esc);
                } else {
                    sb_putc(b, (char)c);  /* ensure_ascii=False: UTF-8 passthrough */
                }
        }
    }
    sb_putc(b, '"');
}

static void dumps_val(sbuf_t *b, const json_t *v) {
    if (!v || v->type == JSON_NULL) { sb_puts(b, "null"); return; }
    switch (v->type) {
        case JSON_BOOL: sb_puts(b, v->bool_val ? "true" : "false"); break;
        case JSON_NUMBER: {
            double x = v->num_val;
            char buf[64];
            if (!isfinite(x)) {
                sb_puts(b, isnan(x) ? "NaN" : (x < 0 ? "-Infinity" : "Infinity"));
            } else if (x == floor(x) && fabs(x) < 1e16) {
                snprintf(buf, sizeof buf, "%.0f", x);
                sb_puts(b, buf);
            } else {
                dtoa_repr(x, buf, sizeof buf);
                sb_puts(b, buf);
            }
            break;
        }
        case JSON_STRING: dumps_str(b, v->str_val ? v->str_val : ""); break;
        case JSON_ARRAY:
            sb_putc(b, '[');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                dumps_val(b, v->c.items[i]);
            }
            sb_putc(b, ']');
            break;
        case JSON_OBJECT:
            sb_putc(b, '{');
            for (size_t i = 0; i < v->c.count; i++) {
                if (i) sb_puts(b, ", ");
                dumps_str(b, v->c.keys[i]);
                sb_puts(b, ": ");
                dumps_val(b, v->c.items[i]);
            }
            sb_putc(b, '}');
            break;
        default: sb_puts(b, "null");
    }
}

static char *py_json_dumps(const json_t *v) {
    sbuf_t b; sb_init(&b);
    dumps_val(&b, v);
    return b.buf;
}

/* raise descriptor helper (str(exc) parity with the oracle's projection) */
static json_t *raise_desc(const char *type, const char *message) {
    json_t *o = json_object();
    json_set_hdr(o, "__raise__", type);
    json_set_hdr(o, "message", message);
    return o;
}

static json_t *key_error(const char *key) {
    char msg[256];
    snprintf(msg, sizeof msg, "'%s'", key);
    return raise_desc("KeyError", msg);
}/* ── PoP ports: think/trajectory strip+extract chains ──────────────────── */

/* PoP: arhf_with_think_block @ agent/agent_runtime_helpers.py:_with_think_block */
json_t *arhf_with_think_block(arhf_io_t *io, json_t *in) {
    (void)io;
    const char *content = json_get_str(in, "content", "");
    if (strstr(content, "<think>") != NULL) return json_string(content);
    size_t n = strlen(content);
    char *out = malloc(n + 18);
    memcpy(out, "<think>\n</think>\n", 17);
    memcpy(out + 17, content, n + 1);
    json_t *r = json_string(out);
    free(out);
    return r;
}

/* PoP: arhf_flatten_content_text @ agent/agent_runtime_helpers.py:_flatten_content_text */
json_t *arhf_flatten_content_text(arhf_io_t *io, json_t *in) {
    (void)io;
    json_t *content = json_obj_get(in, "content");
    if (j_is_str(content)) return json_string(content->str_val);
    if (j_is_arr(content)) {
        sbuf_t b; sb_init(&b);
        for (size_t i = 0; i < content->c.count; i++) {
            json_t *part = content->c.items[i];
            if (j_is_str(part)) { sb_puts(&b, part->str_val); continue; }
            if (!j_is_obj(part)) continue;
            /* str(part.get("type") or "").strip().lower() not in deny set */
            json_t *ty = json_obj_get(part, "type");
            char *ty_s = jtruth(ty) ? py_str_scalar(ty) : strdup("");
            char *ty_strip = py_strip(ty_s);
            char *ty_low = py_lower(ty_strip);
            bool denied = strcmp(ty_low, "thinking") == 0
                || strcmp(ty_low, "reasoning") == 0
                || strcmp(ty_low, "redacted_thinking") == 0;
            free(ty_s); free(ty_strip); free(ty_low);
            if (denied) continue;
            json_t *text = json_obj_get(part, "text");
            if (j_is_str(text) && jtruth(text)) sb_puts(&b, text->str_val);
        }
        json_t *r = json_string(b.buf);
        free(b.buf);
        return r;
    }
    if (j_is_obj(content)) {
        /* str(content.get("text") or content.get("content") or "") */
        json_t *v = json_obj_get(content, "text");
        if (!jtruth(v)) v = json_obj_get(content, "content");
        if (!jtruth(v)) v = NULL;  /* "" */
        char *s = v ? py_str_scalar(v) : strdup("");
        json_t *r = json_string(s);
        free(s);
        return r;
    }
    char *s = py_str_scalar(content);
    json_t *r = json_string(s);
    free(s);
    return r;
}

/* PoP: arhf_merge_user_content @ agent/agent_runtime_helpers.py:_merge_user_content */
json_t *arhf_merge_user_content(arhf_io_t *io, json_t *in) {
    (void)io;
    json_t *prev = json_obj_get(in, "prev");
    json_t *cur = json_obj_get(in, "cur");
    if (j_is_str(prev) && j_is_str(cur)) {
        const char *sep = (jtruth(prev) && jtruth(cur)) ? "\n\n" : "";
        size_t n = strlen(prev->str_val) + strlen(sep) + strlen(cur->str_val);
        char *out = malloc(n + 1);
        snprintf(out, n + 1, "%s%s%s", prev->str_val, sep, cur->str_val);
        json_t *r = json_string(out);
        free(out);
        return r;
    }
    if (j_is_arr(prev) && j_is_arr(cur)) {
        json_t *r = json_array();
        for (size_t i = 0; i < prev->c.count; i++) json_append(r, json_copy(prev->c.items[i]));
        for (size_t i = 0; i < cur->c.count; i++) json_append(r, json_copy(cur->c.items[i]));
        return r;
    }
    if (j_is_arr(prev) && j_is_str(cur)) {
        json_t *r = json_array();
        for (size_t i = 0; i < prev->c.count; i++) json_append(r, json_copy(prev->c.items[i]));
        if (jtruth(cur)) {
            json_t *blk = json_object();
            json_set_hdr(blk, "type", "text");
            json_set(blk, "text", json_copy(cur));
            json_append(r, blk);
        }
        return r;
    }
    if (j_is_str(prev) && j_is_arr(cur)) {
        json_t *r = json_array();
        if (jtruth(prev)) {
            json_t *blk = json_object();
            json_set_hdr(blk, "type", "text");
            json_set(blk, "text", json_copy(prev));
            json_append(r, blk);
        }
        for (size_t i = 0; i < cur->c.count; i++) json_append(r, json_copy(cur->c.items[i]));
        return r;
    }
    json_t *r = json_object();
    json_set(r, "__unmergeable__", json_bool(true));
    return r;
}

/* PoP: arhf_trajectory_gpt_prefix @ agent/agent_runtime_helpers.py:_trajectory_gpt_prefix */
json_t *arhf_trajectory_gpt_prefix(arhf_io_t *io, json_t *in) {
    (void)io;
    json_t *msg = json_obj_get(in, "msg");
    json_t *r = json_obj_get(msg, "reasoning");
    if (jtruth(r) && j_is_str(r)) {
        char *stripped = py_strip(r->str_val);
        bool nonempty = stripped[0] != '\0';
        free(stripped);
        if (nonempty) {
            size_t n = strlen(r->str_val);
            char *out = malloc(n + 20);
            snprintf(out, n + 20, "<think>\n%s\n</think>\n", r->str_val);
            json_t *res = json_string(out);
            free(out);
            return res;
        }
    }
    return json_string("");
}

/* internal: the gpt prefix over a borrowed msg node (no io) */
static char *gpt_prefix_of(const json_t *msg) {
    json_t *r = json_obj_get(msg, "reasoning");
    if (jtruth(r) && j_is_str(r)) {
        char *stripped = py_strip(r->str_val);
        bool nonempty = stripped[0] != '\0';
        free(stripped);
        if (nonempty) {
            size_t n = strlen(r->str_val);
            char *out = malloc(n + 20);
            snprintf(out, n + 20, "<think>\n%s\n</think>\n", r->str_val);
            return out;
        }
    }
    return strdup("");
}

/* PoP: arhf_trajectory_tool_call_turn @ agent/agent_runtime_helpers.py:_trajectory_tool_call_turn */
json_t *arhf_trajectory_tool_call_turn(arhf_io_t *io, json_t *in) {
    json_t *msg = json_obj_get(in, "msg");
    if (!j_is_obj(msg)) return raise_desc("TypeError", "msg must be a dict");
    sbuf_t content; sb_init(&content);
    char *prefix = gpt_prefix_of(msg);
    sb_puts(&content, prefix);
    free(prefix);
    json_t *c = json_obj_get(msg, "content");
    if (jtruth(c)) {
        if (!j_is_str(c)) { free(content.buf); return raise_desc("AttributeError", "'content' object has no attribute 'strip'"); }
        char *stripped = py_strip(c->str_val);
        bool nonempty = stripped[0] != '\0';
        free(stripped);
        if (nonempty) {
            json_t *args = json_object();
            json_set(args, "content", json_copy(c));
            json_t *r = arhf_call(io, "convert_scratchpad_to_think", args);
            json_free(args);
            if (arhf_raised(r)) { free(content.buf); return r; }
            if (!j_is_str(r)) { json_free(r); free(content.buf); return raise_desc("TypeError", "scratchpad seam must return str"); }
            sb_puts(&content, r->str_val);
            sb_puts(&content, "\n");
            json_free(r);
        }
    }
    json_t *tcs = json_obj_get(msg, "tool_calls");
    if (!tcs) { free(content.buf); return key_error("tool_calls"); }
    if (!j_is_arr(tcs)) { free(content.buf); return raise_desc("TypeError", "'tool_calls' must be a list"); }
    for (size_t i = 0; i < tcs->c.count; i++) {
        json_t *tc = tcs->c.items[i];
        if (!jtruth(tc) || !j_is_obj(tc)) continue;
        json_t *fn = json_obj_get(tc, "function");
        if (!fn) { free(content.buf); return key_error("function"); }
        json_t *raw = json_obj_get(fn, "arguments");
        if (!raw) { free(content.buf); return key_error("arguments"); }
        json_t *arguments = NULL;
        bool args_owned = true;
        if (j_is_str(raw)) {
            arguments = json_parse(raw->str_val, NULL);
            if (!arguments) {
                /* logger.warning("Unexpected invalid JSON in trajectory conversion: %s", raw_args[:100]) */
                char head[101];
                snprintf(head, sizeof head, "%s", raw->str_val);
                json_t *wargs = json_object();
                json_set_hdr(wargs, "fmt", "Unexpected invalid JSON in trajectory conversion: %s");
                json_t *vals = json_array();
                json_append(vals, json_string(head));
                json_set(wargs, "args", vals);
                json_t *w = arhf_call(io, "logger.warning", wargs);
                json_free(wargs);
                if (arhf_raised(w)) { free(content.buf); return w; }
                json_free(w);
                arguments = json_object();
            }
        } else {
            arguments = json_copy(raw);
        }
        json_t *name = json_obj_get(fn, "name");
        if (!name) { json_free(arguments); free(content.buf); return key_error("name"); }
        json_t *obj = json_object();
        json_set(obj, "name", json_copy(name));
        json_set(obj, "arguments", arguments);
        (void)args_owned;
        char *dumped = py_json_dumps(obj);
        json_free(obj);
        sb_puts(&content, "<tool_call>\n");
        sb_puts(&content, dumped);
        sb_puts(&content, "\n</tool_call>\n");
        free(dumped);
    }
    /* _with_think_block(content).rstrip() */
    if (strstr(content.buf, "<think>") == NULL) {
        sbuf_t wrapped; sb_init(&wrapped);
        sb_puts(&wrapped, "<think>\n</think>\n");
        sb_puts(&wrapped, content.buf);
        free(content.buf);
        content = wrapped;
    }
    py_rstrip_inplace(content.buf);
    json_t *r = json_string(content.buf);
    free(content.buf);
    return r;
}

/* PoP: arhf_trajectory_tool_responses @ agent/agent_runtime_helpers.py:_trajectory_tool_responses */
json_t *arhf_trajectory_tool_responses(arhf_io_t *io, json_t *in) {
    (void)io;
    json_t *msg = json_obj_get(in, "msg");
    json_t *messages = json_obj_get(in, "messages");
    size_t j = (size_t)json_get_num(in, "start", 0);
    json_t *blocks = json_array();
    if (!j_is_arr(messages)) { json_free(blocks); return raise_desc("TypeError", "messages must be a list"); }
    while (j < messages->c.count) {
        json_t *tool_msg = messages->c.items[j];
        if (!j_is_obj(tool_msg)) { json_free(blocks); return raise_desc("TypeError", "'message' object is not subscriptable"); }
        json_t *role = json_obj_get(tool_msg, "role");
        if (!role) { json_free(blocks); return key_error("role"); }
        if (!j_is_str(role) || strcmp(role->str_val, "tool") != 0) break;
        json_t *tool_content = json_obj_get(tool_msg, "content");
        if (!tool_content) { json_free(blocks); return key_error("content"); }
        json_t *content_owned = NULL;
        const json_t *content_val = tool_content;
        if (j_is_str(tool_content)) {
            char *stripped = py_strip(tool_content->str_val);
            bool looks_json = stripped[0] == '{' || stripped[0] == '[';
            free(stripped);
            if (looks_json) {
                json_t *parsed = json_parse(tool_content->str_val, NULL);
                if (parsed) { content_owned = parsed; content_val = parsed; }
                /* JSONDecodeError -> keep the original string */
            }
        }
        /* non-str content: AttributeError on .strip() is caught -> keep as-is */
        size_t tool_index = blocks->c.count;
        json_t *tcs = json_obj_get(msg, "tool_calls");
        if (!tcs) { json_free(content_owned); json_free(blocks); return key_error("tool_calls"); }
        const char *tool_name = "unknown";
        json_t *name_node = NULL;
        if (tool_index < json_len(tcs)) {
            json_t *tc = json_get(tcs, tool_index);
            json_t *fn = j_is_obj(tc) ? json_obj_get(tc, "function") : NULL;
            if (!fn) { json_free(content_owned); json_free(blocks); return key_error("function"); }
            name_node = json_obj_get(fn, "name");
            if (!name_node) { json_free(content_owned); json_free(blocks); return key_error("name"); }
            if (!j_is_str(name_node)) { json_free(content_owned); json_free(blocks); return raise_desc("TypeError", "tool name must be str"); }
            tool_name = name_node->str_val;
        }
        json_t *tcid = json_obj_get(tool_msg, "tool_call_id");
        json_t *payload = json_object();
        json_set(payload, "tool_call_id", tcid ? json_copy(tcid) : json_string(""));
        json_set_hdr(payload, "name", tool_name);
        json_set(payload, "content", json_copy(content_val));
        char *dumped = py_json_dumps(payload);
        json_free(payload);
        json_free(content_owned);
        size_t n = strlen(dumped);
        char *block = malloc(n + 40);
        snprintf(block, n + 40, "<tool_response>\n%s\n</tool_response>", dumped);
        json_append(blocks, json_string(block));
        free(block);
        free(dumped);
        j++;
    }
    json_t *r = json_object();
    json_set(r, "blocks", blocks);
    json_set(r, "next", json_number((double)j));
    return r;
}/* ── duration parsing + reset flows + entitlement gate ─────────────────── */

/* internal: _duration_string_seconds over a borrowed text (no seam trace).
 * Regex surface (hand-transcribed, ASCII fixture scope):
 *   float(raw) pre-pass, else (\d+(?:\.\d+)?)(ms|h|m|s) findall whose
 *   concatenation must equal raw exactly (a tiling scan). */
static json_t *duration_seconds_of(const char *text) {
    char *raw0 = py_strip(text ? text : "");
    char *raw = py_lower(raw0);
    free(raw0);
    if (!raw[0]) { free(raw); return json_null(); }
    double f;
    if (py_float_parse(raw, &f)) { free(raw); return json_number(f); }
    /* tiling component scan */
    const char *p = raw;
    double total = 0.0;
    bool any = false;
    while (*p) {
        const char *nstart = p;
        if (!(*p >= '0' && *p <= '9')) { free(raw); return json_null(); }
        while (*p >= '0' && *p <= '9') p++;
        if (*p == '.') {
            p++;
            if (!(*p >= '0' && *p <= '9')) { free(raw); return json_null(); }
            while (*p >= '0' && *p <= '9') p++;
        }
        size_t nlen = (size_t)(p - nstart);
        char numbuf[128];
        if (nlen >= sizeof numbuf) { free(raw); return json_null(); }
        memcpy(numbuf, nstart, nlen);
        numbuf[nlen] = '\0';
        double mult;
        if (strncmp(p, "ms", 2) == 0) { mult = 0.001; p += 2; }
        else if (*p == 'h') { mult = 3600.0; p += 1; }
        else if (*p == 'm') { mult = 60.0; p += 1; }
        else if (*p == 's') { mult = 1.0; p += 1; }
        else { free(raw); return json_null(); }
        total += strtod(numbuf, NULL) * mult;
        any = true;
    }
    free(raw);
    if (!any) return json_null();
    return json_number(total);
}

/* PoP: arhf_duration_string_seconds @ agent/agent_runtime_helpers.py:_duration_string_seconds */
json_t *arhf_duration_string_seconds(arhf_io_t *io, json_t *in) {
    (void)io;
    const char *text = json_get_str(in, "text", "");
    json_t *r = duration_seconds_of(text);
    if (r && r->type == JSON_NUMBER && !isfinite(r->num_val)) {
        double x = r->num_val;
        json_free(r);
        return num_node(x);
    }
    return r;
}

/* PoP: arhf_set_reset_from_retry_after @ agent/agent_runtime_helpers.py:_set_reset_from_retry_after */
json_t *arhf_set_reset_from_retry_after(arhf_io_t *io, json_t *in) {
    json_t *context = json_obj_get(in, "context");
    if (!j_is_obj(context)) return raise_desc("AttributeError", "context must be a dict");
    if (json_has(context, "reset_at")) return json_null();
    json_t *args = json_object();
    json_t *ra = json_obj_get(in, "retry_after");
    json_set(args, "value", ra ? json_copy(ra) : json_null());
    json_t *seconds = arhf_call(io, "parse_retry_after_seconds", args);
    json_free(args);
    if (arhf_raised(seconds)) return seconds;
    if (seconds && seconds->type != JSON_NULL) {
        if (!j_is_num(seconds)) { json_free(seconds); return raise_desc("TypeError", "seconds must be a number"); }
        json_t *t = arhf_call(io, "time.time", NULL);
        if (arhf_raised(t)) { json_free(seconds); return t; }
        double now = j_is_num(t) ? t->num_val : 0.0;
        json_set(context, "reset_at", num_node(now + seconds->num_val));
        json_free(t);
    }
    json_free(seconds);
    return json_null();
}

static const char *VENDOR_RESET_HEADERS[] = {
    "x-ratelimit-reset-requests", "x-ratelimit-reset-tokens",
    "anthropic-ratelimit-requests-reset", "anthropic-ratelimit-tokens-reset",
};

/* PoP: arhf_set_reset_from_vendor_headers @ agent/agent_runtime_helpers.py:_set_reset_from_vendor_headers */
json_t *arhf_set_reset_from_vendor_headers(arhf_io_t *io, json_t *in) {
    json_t *context = json_obj_get(in, "context");
    json_t *headers = json_obj_get(in, "headers");
    if (!j_is_obj(context)) return raise_desc("AttributeError", "context must be a dict");
    if (!j_is_obj(headers)) {
        const char *tn = (!headers || headers->type == JSON_NULL) ? "NoneType"
            : (headers->type == JSON_ARRAY ? "list"
            : (headers->type == JSON_STRING ? "str"
            : (headers->type == JSON_BOOL ? "bool" : "float")));
        char msg[128];
        snprintf(msg, sizeof msg, "'%s' object has no attribute 'get'", tn);
        return raise_desc("AttributeError", msg);
    }
    for (size_t i = 0; i < sizeof(VENDOR_RESET_HEADERS) / sizeof(char *); i++) {
        json_t *value = json_obj_get(headers, VENDOR_RESET_HEADERS[i]);
        if (!j_is_str(value)) continue;
        char *stripped = py_strip(value->str_val);
        bool nonempty = stripped[0] != '\0';
        free(stripped);
        if (!nonempty) continue;
        json_t *sec = duration_seconds_of(value->str_val);
        double seconds = 0.0;
        bool have_seconds = false;
        if (sec && sec->type == JSON_NUMBER) { seconds = sec->num_val; have_seconds = true; }
        json_free(sec);
        if (!have_seconds) {
            json_t *args = json_object();
            json_set(args, "value", json_copy(value));
            json_t *absolute = arhf_call(io, "_parse_absolute_timestamp", args);
            json_free(args);
            if (arhf_raised(absolute)) return absolute;
            if (absolute && absolute->type != JSON_NULL) {
                if (!j_is_num(absolute)) { json_free(absolute); return raise_desc("TypeError", "absolute must be a number"); }
                json_t *t = arhf_call(io, "time.time", NULL);
                if (arhf_raised(t)) { json_free(absolute); return t; }
                double now = j_is_num(t) ? t->num_val : 0.0;
                seconds = absolute->num_val - now;
                have_seconds = true;
                json_free(t);
            }
            json_free(absolute);
        }
        if (have_seconds && seconds > 0.0) {
            json_t *t2 = arhf_call(io, "time.time", NULL);
            if (arhf_raised(t2)) return t2;
            double now2 = j_is_num(t2) ? t2->num_val : 0.0;
            json_set(context, "reset_at", num_node(now2 + seconds));
            json_free(t2);
            return json_null();
        }
    }
    return json_null();
}

/* PoP: arhf_is_entitlement_403 @ agent/agent_runtime_helpers.py:_is_entitlement_403 */
json_t *arhf_is_entitlement_403(arhf_io_t *io, json_t *in) {
    json_t *agent = json_obj_get(in, "agent");
    json_t *sc = json_obj_get(in, "status_code");
    json_t *ec = json_obj_get(in, "error_context");
    json_t *args = json_object();
    json_set(args, "error_context", ec ? json_copy(ec) : json_null());
    json_set(args, "status_code", sc ? json_copy(sc) : json_null());
    json_t *r = arhf_call(io, "agent._is_entitlement_failure", args);
    json_free(args);
    if (arhf_raised(r)) return r;
    bool verdict = jtruth(r);
    json_free(r);
    if (verdict) return json_bool(true);
    if (!j_is_num(sc) || sc->num_val != 403.0) return json_bool(false);
    /* haystack = " ".join(str(ec.get(k) or "").lower() for k in (...) if isinstance(ec, dict)) */
    sbuf_t hay; sb_init(&hay);
    if (j_is_obj(ec)) {
        static const char *keys[] = {"message", "reason", "code", "error"};
        for (size_t i = 0; i < 4; i++) {
            if (i) sb_putc(&hay, ' ');
            json_t *v = json_obj_get(ec, keys[i]);
            char *s = jtruth(v) ? py_str_scalar(v) : strdup("");
            char *low = py_lower(s);
            sb_puts(&hay, low);
            free(s); free(low);
        }
    }
    bool org_blocked = strstr(hay.buf, "oauth authentication is currently not allowed for this organization") != NULL;
    free(hay.buf);
    if (org_blocked) return json_bool(true);
    json_t *prov = j_is_obj(agent) ? json_obj_get(agent, "provider") : NULL;
    const char *provider = (jtruth(prov) && j_is_str(prov)) ? prov->str_val : "";
    if (strcmp(provider, "anthropic") == 0) {
        json_t *am = json_obj_get(agent, "api_mode");
        if (j_is_str(am) && strcmp(am->str_val, "anthropic_messages") == 0) return json_bool(true);
    }
    if (strcmp(provider, "xai-oauth") == 0) {
        /* recompute haystack for the stale-token exclusions */
        sbuf_t h2; sb_init(&h2);
        if (j_is_obj(ec)) {
            static const char *keys[] = {"message", "reason", "code", "error"};
            for (size_t i = 0; i < 4; i++) {
                if (i) sb_putc(&h2, ' ');
                json_t *v = json_obj_get(ec, keys[i]);
                char *s = jtruth(v) ? py_str_scalar(v) : strdup("");
                char *low = py_lower(s);
                sb_puts(&h2, low);
                free(s); free(low);
            }
        }
        bool stale = strstr(h2.buf, "[wke=unauthenticated:") != NULL
            || strstr(h2.buf, "oauth2 access token could not be validated") != NULL;
        free(h2.buf);
        return json_bool(!stale);
    }
    return json_bool(false);
}
