/*
 * tts_command_provider.c — C port of upstream Hermes tools/tts_command_provider.py.
 * Gate: w14_ttscommand_bd97e2
 *
 * PoP: w14_ttscommand_bd97e2_shell_quote_context @ tools/tts_command_provider.py:shell_quote_context
 * PoP: w14_ttscommand_bd97e2_quote_command_placeholder @ tools/tts_command_provider.py:quote_command_placeholder
 * PoP: w14_ttscommand_bd97e2_render_command_template @ tools/tts_command_provider.py:render_command_template
 * PoP: w14_ttscommand_bd97e2_command_failure_detail @ tools/tts_command_provider.py:command_failure_detail
 * PoP: w14_ttscommand_bd97e2_get_provider_section @ tools/tts_command_provider.py:_get_provider_section
 * PoP: w14_ttscommand_bd97e2_is_command_provider_config @ tools/tts_command_provider.py:_is_command_provider_config
 * PoP: w14_ttscommand_bd97e2_named_provider_config @ tools/tts_command_provider.py:_named_provider_config
 * PoP: w14_ttscommand_bd97e2_resolve_command_config @ tools/tts_command_provider.py:_resolve_command_config
 * PoP: w14_ttscommand_bd97e2_command_timeout @ tools/tts_command_provider.py:_command_timeout
 * PoP: w14_ttscommand_bd97e2_command_output_format @ tools/tts_command_provider.py:_command_output_format
 * PoP: w14_ttscommand_bd97e2_get_command_tts_output_format @ tools/tts_command_provider.py:_get_command_tts_output_format
 *
 * os_name is an explicit fixture argument ("posix"/"nt") because the oracle
 * runs on Windows where os.name == "nt". Windows-unsafe placeholder values
 * (live raises ValueError) are not exercised.
 */
#include "noc_w14_ttscommand_bd97e2.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static const char *jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? (v->str_val ? v->str_val : "") : NULL;
}

static char *strip_ws(const char *s) {
    size_t a = 0, b = s ? strlen(s) : 0;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    if (out) { if (b > a) memcpy(out, s + a, b - a); out[b - a] = '\0'; }
    return out;
}

static char *lower_str(const char *s) {
    char *out = dupstr(s ? s : "");
    for (char *p = out; *p; p++) *p = (char)tolower((unsigned char)*p);
    return out;
}

/* Replace every occurrence of `from` with `to`, returning a fresh allocation. */
static char *replace_all(const char *s, const char *from, const char *to) {
    size_t fn = strlen(from), tn = strlen(to);
    size_t count = 0;
    for (const char *p = s; (p = strstr(p, from)) != NULL; p += fn) count++;
    size_t total = strlen(s) + count * (tn > fn ? tn - fn : 0) + 1;
    char *out = (char *)malloc(total + 1);
    if (!out) return dupstr(s ? s : "");
    char *q = out;
    const char *p = s;
    while (*p) {
        const char *hit = strstr(p, from);
        if (!hit) { strcpy(q, p); q += strlen(p); break; }
        size_t n = (size_t)(hit - p);
        if (n) { memcpy(q, p, n); q += n; }
        memcpy(q, to, tn); q += tn;
        p = hit + fn;
    }
    *q = '\0';
    return out;
}

/* Shell quote context in effect just before `position`. NULL == no quote. */
static char *quote_context_at(const char *t, int position) {
    char quote = 0;
    int escaped = 0;
    int i = 0;
    while (i < position && t[i]) {
        char c = t[i];
        if (quote == '\'') {
            if (c == '\'') quote = 0;
        } else if (quote == '"') {
            if (escaped) escaped = 0;
            else if (c == '\\') escaped = 1;
            else if (c == '"') quote = 0;
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '\\') {
            i++;
        }
        i++;
    }
    if (!quote) return NULL;
    char *out = (char *)malloc(2);
    if (out) { out[0] = quote; out[1] = '\0'; }
    return out;
}

json_t *w14_ttscommand_bd97e2_shell_quote_context(const struct json_t *command_template,
                                                 int position) {
    char *q = quote_context_at(jstr(command_template) ? jstr(command_template) : "", position);
    json_t *out = q ? json_string(q) : json_null();
    free(q);
    return out;
}

static int shlex_safe_char(unsigned char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return 1;
    return strchr("_@%+=:,./-", c) != NULL;
}

static char *shlex_quote(const char *v) {
    if (!*v) return dupstr("''");
    int safe = 1;
    for (const char *p = v; *p; p++) if (!shlex_safe_char((unsigned char)*p)) { safe = 0; break; }
    if (safe) return dupstr(v);
    char *repl = replace_all(v, "'", "'\"'\"'");
    size_t n = strlen(repl);
    char *out = (char *)malloc(n + 3);
    if (out) { out[0] = '\''; memcpy(out + 1, repl, n); out[n + 1] = '\''; out[n + 2] = '\0'; }
    free(repl);
    return out;
}

/* subprocess.list2cmdline([value]) for a single argument. */
static char *list2cmdline_one(const char *v) {
    int needquote = !*v || strchr(v, ' ') || strchr(v, '\t');
    size_t n = strlen(v);
    char *out = (char *)malloc(n * 3 + 4);
    if (!out) return dupstr(v);
    size_t j = 0;
    if (needquote) out[j++] = '"';
    size_t bs = 0;
    for (size_t i = 0; i < n; i++) {
        char c = v[i];
        if (c == '\\') { bs++; continue; }
        if (c == '"') {
            for (size_t k = 0; k < bs * 2; k++) out[j++] = '\\';
            bs = 0;
            out[j++] = '\\'; out[j++] = '"';
            continue;
        }
        for (size_t k = 0; k < bs; k++) out[j++] = '\\';
        bs = 0;
        out[j++] = c;
    }
    for (size_t k = 0; k < bs * 2; k++) out[j++] = '\\';
    if (needquote) out[j++] = '"';
    out[j] = '\0';
    return out;
}

static char *quote_placeholder(const char *value, const char *ctx, int windows) {
    if (ctx && !strcmp(ctx, "'")) return replace_all(value, "'", "'\\''");
    if (ctx && !strcmp(ctx, "\"")) {
        if (windows) {
            size_t n = strlen(value), tail = 0;
            while (tail < n && value[n - 1 - tail] == '\\') tail++;
            size_t total = n + tail + 1;
            char *out = (char *)malloc(total);
            if (out) {
                memcpy(out, value, n);
                for (size_t i = 0; i < tail; i++) out[n + i] = '\\';
                out[n + tail] = '\0';
            }
            return out ? out : dupstr(value);
        }
        char *a = replace_all(value, "\\", "\\\\");
        char *b = replace_all(a, "\"", "\\\"");
        char *c = replace_all(b, "$", "\\$");
        char *d = replace_all(c, "`", "\\`");
        free(a); free(b); free(c);
        return d;
    }
    return windows ? list2cmdline_one(value) : shlex_quote(value);
}

json_t *w14_ttscommand_bd97e2_quote_command_placeholder(const struct json_t *value,
                                                       const struct json_t *quote_context,
                                                       const char *os_name) {
    int windows = os_name && !strcmp(os_name, "nt");
    char *out = quote_placeholder(jstr(value) ? jstr(value) : "", jstr(quote_context), windows);
    json_t *j = json_string(out ? out : "");
    free(out);
    return j;
}

static char *path_suffix(const char *p) {
    if (!p || !*p) return dupstr("");
    const char *slash = strrchr(p, '/');
    const char *bs = strrchr(p, '\\');
    const char *base = (slash && bs) ? (slash > bs ? slash + 1 : bs + 1)
                     : (slash ? slash + 1 : (bs ? bs + 1 : p));
    const char *dot = strrchr(base, '.');
    if (!dot || dot == base) return dupstr("");
    char *lower = lower_str(dot);
    char *out = dupstr(lower);
    size_t skip = 0;
    while (out[skip] == '.') skip++;
    if (skip) memmove(out, out + skip, strlen(out + skip) + 1);
    free(lower);
    return out;
}

static int in_formats(const struct json_t *formats, const char *fmt) {
    if (!json_is_array(formats)) return 0;
    for (size_t i = 0; i < json_len(formats); i++) {
        const char *v = jstr(json_get(formats, i));
        if (v && !strcmp(v, fmt)) return 1;
    }
    return 0;
}

static char *command_output_format_str(const struct json_t *config, const struct json_t *formats,
                                       const char *def) {
    json_t *raw = NULL;
    json_t *f = json_obj_get(config, "format");
    if (f && (f->type == JSON_NULL ? 0 : (f->type == JSON_STRING ? *f->str_val :
        (f->type == JSON_NUMBER ? f->num_val != 0 : (f->type == JSON_BOOL ? f->bool_val : 1)))))
        raw = f;
    if (!raw) {
        json_t *o = json_obj_get(config, "output_format");
        if (o && (o->type == JSON_NULL ? 0 : (o->type == JSON_STRING ? *o->str_val :
            (o->type == JSON_NUMBER ? o->num_val != 0 : (o->type == JSON_BOOL ? o->bool_val : 1)))))
            raw = o;
    }
    const char *s = raw ? (jstr(raw) ? jstr(raw) : (raw->type == JSON_NUMBER ? "number" : "")) : def;
    char *low = lower_str(s);
    char *strip = strip_ws(low);
    char *fmt = strip;
    while (*fmt == '.') fmt++;
    char *out = dupstr(in_formats(formats, fmt) ? fmt : def);
    free(low); free(strip);
    return out;
}

json_t *w14_ttscommand_bd97e2_render_command_template(const struct json_t *command_template,
                                                     const struct json_t *placeholders,
                                                     const char *os_name) {
    const char *t = jstr(command_template) ? jstr(command_template) : "";
    size_t n = strlen(t);
    int windows = os_name && !strcmp(os_name, "nt");
    char *out = (char *)malloc(n * 16 + 256);
    if (!out) return json_string("");
    size_t j = 0;
    char *tokens[64], *quoted[64];
    int nt = 0;
    for (size_t i = 0; i < n;) {
        int matched = 0;
        int look_ok = (i == 0) || (t[i - 1] != '$' && (windows || t[i - 1] != '\\'));
        if (look_ok && t[i] == '{' && nt < 64) {
            int dbl = (i + 1 < n && t[i + 1] == '{');
            size_t start = dbl ? i + 2 : i + 1;
            if (json_is_object(placeholders)) {
                for (size_t k = 0; k < json_object_size(placeholders); k++) {
                    const char *name = json_object_get_key_at(placeholders, k);
                    if (!name) continue;
                    size_t kl = strlen(name);
                    if (start + kl >= n || strncmp(t + start, name, kl)) continue;
                    if (t[start + kl] != '}') continue;
                    if (dbl && (start + kl + 1 >= n || t[start + kl + 1] != '}')) continue;
                    size_t mlen = (start - i) + kl + (dbl ? 2 : 1);
                    char *ctx = quote_context_at(t, (int)i);
                    json_t *pv = json_obj_get(placeholders, name);
                    char *q = quote_placeholder(jstr(pv) ? jstr(pv) : "", ctx, windows);
                    char token[64];
                    snprintf(token, sizeof(token), "__HERMES_CMD_PLACEHOLDER_%d__", nt);
                    memcpy(out + j, token, strlen(token)); j += strlen(token);
                    tokens[nt] = dupstr(token);
                    quoted[nt] = q;
                    nt++;
                    free(ctx);
                    i += mlen;
                    matched = 1;
                    break;
                }
            }
        }
        if (!matched) out[j++] = t[i++];
    }
    out[j] = '\0';
    char *collapsed = replace_all(out, "{{", "{");
    char *collapsed2 = replace_all(collapsed, "}}", "}");
    free(out); free(collapsed);
    char *result = collapsed2;
    for (int k = 0; k < nt; k++) {
        char *next = replace_all(result, tokens[k], quoted[k]);
        free(result);
        result = next;
    }
    for (int k = 0; k < nt; k++) { free(tokens[k]); free(quoted[k]); }
    json_t *final = json_string(result);
    free(result);
    return final;
}

json_t *w14_ttscommand_bd97e2_command_failure_detail(const struct json_t *err_text,
                                                     const struct json_t *out_text) {
    const char *names[2] = {"stderr", "stdout"};
    const json_t *vals[2] = {err_text, out_text};
    char *parts[2] = {NULL, NULL};
    int np = 0;
    for (int i = 0; i < 2; i++) {
        const char *v = jstr(vals[i]);
        if (!v || !*v) continue;
        char *s = strip_ws(v);
        size_t total = strlen(names[i]) + strlen(s) + 4;
        char *p = (char *)malloc(total);
        if (p) snprintf(p, total, "%s: %s", names[i], s);
        parts[np++] = p;
        free(s);
    }
    if (!np) return json_string("no command output");
    size_t total = 2;
    for (int i = 0; i < np; i++) total += strlen(parts[i]) + 2;
    char *out = (char *)malloc(total);
    if (out) {
        out[0] = '\0';
        for (int i = 0; i < np; i++) {
            if (i) strcat(out, "; ");
            strcat(out, parts[i]);
        }
    }
    for (int i = 0; i < np; i++) free(parts[i]);
    json_t *j = json_string(out ? out : "");
    free(out);
    return j;
}

static json_t *provider_section(const struct json_t *config, const char *name) {
    if (!json_is_object(config)) return json_object();
    json_t *v = json_obj_get(config, name);
    return json_is_object(v) ? json_copy(v) : json_object();
}

json_t *w14_ttscommand_bd97e2_get_provider_section(const struct json_t *config, const char *name) {
    return provider_section(config, name);
}

static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    return v->c.count != 0;
}

json_t *w14_ttscommand_bd97e2_is_command_provider_config(const struct json_t *config) {
    if (!json_is_object(config)) return json_bool(false);
    json_t *t = json_obj_get(config, "type");
    const char *ptype = t && t->type == JSON_STRING ? jstr(t) : "";
    char *low = lower_str(ptype);
    char *st = strip_ws(low);
    json_t *cmd = json_obj_get(config, "command");
    const char *cs = jstr(cmd);
    char *ct = strip_ws(cs ? cs : "");
    int ok = (!*st || !strcmp(st, "command")) && cs && *ct;
    free(low); free(st); free(ct);
    return json_bool(ok);
}

static int str_in_array(const struct json_t *arr, const char *s) {
    if (!json_is_array(arr)) return 0;
    for (size_t i = 0; i < json_len(arr); i++) {
        const char *v = jstr(json_get(arr, i));
        if (v && !strcmp(v, s)) return 1;
    }
    return 0;
}

json_t *w14_ttscommand_bd97e2_named_provider_config(const struct json_t *config, const char *name,
                                                   const struct json_t *builtins) {
    json_t *providers = provider_section(config, "providers");
    json_t *section = json_obj_get(providers, name);
    if (json_is_object(section)) { json_t *out = json_copy(section); json_free(providers); return out; }
    json_free(providers);
    char *low = lower_str(name);
    if (str_in_array(builtins, low)) { free(low); return json_object(); }
    free(low);
    return provider_section(config, name);
}

json_t *w14_ttscommand_bd97e2_resolve_command_config(const struct json_t *provider,
                                                    const struct json_t *config,
                                                    const struct json_t *reserved) {
    const char *p = jstr(provider) ? jstr(provider) : "";
    char *low0 = lower_str(p);
    char *key = strip_ws(low0);
    free(low0);
    if (!*key || str_in_array(reserved, key)) { free(key); return json_null(); }
    json_t *named = w14_ttscommand_bd97e2_named_provider_config(config, key, reserved);
    json_t *res = json_is_true(w14_ttscommand_bd97e2_is_command_provider_config(named))
                  ? named : NULL;
    if (!res) json_free(named);
    free(key);
    return res ? res : json_null();
}

static double cast_double(const json_t *v, double def, int *ok) {
    *ok = 1;
    if (!v) { *ok = 0; return def; }
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end == v->str_val) { *ok = 0; return def; }
        while (*end && isspace((unsigned char)*end)) end++;
        if (*end) { *ok = 0; return def; }
        return d;
    }
    *ok = 0;
    return def;
}

json_t *w14_ttscommand_bd97e2_command_timeout(const struct json_t *config, double default_value) {
    json_t *raw = json_obj_get(config, "timeout");
    if (!raw) raw = json_obj_get(config, "timeout_seconds");
    if (!raw) raw = json_number(default_value);
    int ok = 0;
    double v = cast_double(raw, default_value, &ok);
    return json_number((ok && v > 0) ? v : default_value);
}

json_t *w14_ttscommand_bd97e2_command_output_format(const struct json_t *config,
                                                   const struct json_t *formats,
                                                   const char *default_value) {
    char *out = command_output_format_str(config, formats, default_value);
    json_t *j = json_string(out);
    free(out);
    return j;
}

json_t *w14_ttscommand_bd97e2_get_command_tts_output_format(const struct json_t *config,
                                                           const struct json_t *output_path) {
    char *suffix = path_suffix(jstr(output_path));
    const char *fmts[] = {"mp3", "wav", "ogg", "flac", "m4a", "aac", "amr", "opus"};
    json_t *formats = json_array();
    for (size_t i = 0; i < 8; i++) json_append(formats, json_string(fmts[i]));
    json_t *out;
    if (in_formats(formats, suffix)) out = json_string(suffix);
    else out = w14_ttscommand_bd97e2_command_output_format(config, formats, "mp3");
    free(suffix);
    json_free(formats);
    return out;
}
