/*
 * gateway_setting_commands.c — C11 port of upstream Hermes
 * hermes_cli/gateway_setting_commands.py (gate w25_gatewayset_b9bfd0).
 * STATUS: real behaviour port of the pure value core; the gateway connection
 *          is a seam and is never opened here.
 * PoP: w25_gatewayset_b9bfd0_is_setting @ hermes_cli/gateway_setting_commands.py:GATEWAY_SETTINGS
 * PoP: w25_gatewayset_b9bfd0_split_arg @ hermes_cli/gateway_setting_commands.py:run
 * PoP: w25_gatewayset_b9bfd0_mirror_busy @ hermes_cli/gateway_setting_commands.py:_mirror_busy
 * PoP: w25_gatewayset_b9bfd0_unbounded @ hermes_cli/gateway_setting_commands.py:_unbounded
 * PoP: w25_gatewayset_b9bfd0_call_command @ hermes_cli/gateway_setting_commands.py:_call
 * PoP: w25_gatewayset_b9bfd0_render @ hermes_cli/gateway_setting_commands.py:run
 * PoP: w25_gatewayset_b9bfd0_constants @ hermes_cli/gateway_setting_commands.py:GATEWAY_SETTINGS
 *
 * The session's agent lives in the gateway, so the CLI only decides around it:
 * which canonical names the gateway owns, how the command line splits into
 * (canonical, arg), which arg mirrors local client state, which /cron forms
 * lose their deadline, and how the reply's lines are printed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_gatewayset_b9bfd0.h"

/* GATEWAY_SETTINGS keys, in source order. */
static const char *const GS_SETTINGS[] = {
    "personality", "busy", "approvals", "footer", "fast",
    "refine", "review", "cron", "toolsets",
};
#define GS_SETTINGS_N (sizeof(GS_SETTINGS) / sizeof(GS_SETTINGS[0]))

/* _BUSY_MODES (a frozenset; exposed sorted so the table has one order) */
static const char *const GS_BUSY_MODES[] = {"interrupt", "queue", "steer"};
#define GS_BUSY_MODES_N (sizeof(GS_BUSY_MODES) / sizeof(GS_BUSY_MODES[0]))

/* _VERBATIM: reports the owner already lays out; printed as is. */
static const char *const GS_VERBATIM[] = {"toolsets"};
#define GS_VERBATIM_N (sizeof(GS_VERBATIM) / sizeof(GS_VERBATIM[0]))

/* Python truthiness of a JSON value. */
static int gs_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

/* str.strip()/split() whitespace, ASCII side. */
static int gs_ws(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static json_t *gs_str_range(const char *s, size_t n) {
    char *buf = (char *)malloc(n + 1);
    json_t *out;
    if (!buf) return json_null();
    if (n) memcpy(buf, s, n);
    buf[n] = '\0';
    out = json_string(buf);
    free(buf);
    return out;
}

static int gs_in(const char *s, const char *const *set, size_t n) {
    size_t i;
    if (!s) return 0;
    for (i = 0; i < n; i++)
        if (strcmp(s, set[i]) == 0) return 1;
    return 0;
}

json_t *w25_gatewayset_b9bfd0_is_setting(const json_t *canonical) {
    if (!canonical || canonical->type != JSON_STRING)
        return json_bool(0); /* a non-str key is never in the dict */
    return json_bool(gs_in(canonical->str_val, GS_SETTINGS, GS_SETTINGS_N));
}

json_t *w25_gatewayset_b9bfd0_split_arg(const json_t *cmd) {
    const char *s;
    size_t n, b, e, i, j;

    if (!cmd || cmd->type == JSON_NULL) return json_string("");
    if (cmd->type != JSON_STRING) return json_null();
    s = cmd->str_val ? cmd->str_val : "";
    n = strlen(s);
    b = 0;
    e = n;
    while (b < e && gs_ws(s[b])) b++;
    while (e > b && gs_ws(s[e - 1])) e--;
    i = b;
    while (i < e && !gs_ws(s[i])) i++;
    if (i >= e) return json_string(""); /* split() gave one part */
    j = i;
    while (j < e && gs_ws(s[j])) j++;
    return gs_str_range(s + j, e - j);
}

/* ASCII lower(), which is what str.lower() does to these keywords. */
static json_t *gs_lower(const char *s) {
    size_t n = strlen(s), i;
    char *buf = (char *)malloc(n + 1);
    json_t *out;
    if (!buf) return json_null();
    for (i = 0; i < n; i++) {
        char c = s[i];
        buf[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    buf[n] = '\0';
    out = json_string(buf);
    free(buf);
    return out;
}

json_t *w25_gatewayset_b9bfd0_mirror_busy(const json_t *arg) {
    json_t *low;
    if (!arg || arg->type != JSON_STRING) return json_null();
    low = gs_lower(arg->str_val ? arg->str_val : "");
    if (low && low->type == JSON_STRING && gs_in(low->str_val, GS_BUSY_MODES, GS_BUSY_MODES_N))
        return low;
    json_free(low);
    return json_null();
}

/* shlex.split(arg) raises ValueError only on an unclosed quotation; the word
 * list itself reaches no decision here, so validity is all that matters. */
static int gs_shlex_ok(const char *s) {
    int in_single = 0, in_double = 0;
    size_t i;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (in_single) {
            if (c == '\'') in_single = 0;
            continue;
        }
        if (in_double) {
            if (c == '\\') { if (s[i + 1]) i++; continue; }
            if (c == '"') in_double = 0;
            continue;
        }
        if (c == '\\') { if (s[i + 1]) i++; continue; }
        if (c == '\'') { in_single = 1; continue; }
        if (c == '"') { in_double = 1; continue; }
    }
    return !in_single && !in_double;
}

json_t *w25_gatewayset_b9bfd0_unbounded(const json_t *canonical, const json_t *arg,
                                        const json_t *cron_read) {
    const char *canon = canonical && canonical->type == JSON_STRING && canonical->str_val
                            ? canonical->str_val : "";
    const char *text = arg && arg->type == JSON_STRING && arg->str_val ? arg->str_val : "";

    if (strcmp(canon, "cron") != 0) return json_bool(0);
    if (!gs_shlex_ok(text)) return json_bool(0); /* the owner refuses at once */
    /* cron_read(words) is None -> a write answered only when its work is done */
    return json_bool(cron_read == NULL || cron_read->type == JSON_NULL);
}

json_t *w25_gatewayset_b9bfd0_call_command(const json_t *canonical, const json_t *arg) {
    const char *canon = canonical && canonical->type == JSON_STRING && canonical->str_val
                            ? canonical->str_val : "";
    size_t cn = strlen(canon), an = 0, need;
    char *buf;
    json_t *out;

    if (gs_truthy(arg) && arg->type == JSON_STRING)
        an = strlen(arg->str_val ? arg->str_val : "");
    need = cn + (an ? an + 1 : 0) + 1;
    buf = (char *)malloc(need);
    if (!buf) return json_null();
    memcpy(buf, canon, cn);
    if (an) {
        buf[cn] = ' ';
        memcpy(buf + cn + 1, arg->str_val, an);
        buf[cn + 1 + an] = '\0';
    } else {
        buf[cn] = '\0';
    }
    out = json_string(buf);
    free(buf);
    return out;
}

static json_t *gs_line_out(const char *line, size_t n, int prefix) {
    char *buf = (char *)malloc(n + (prefix ? 3 : 1));
    json_t *out;
    if (!buf) return json_null();
    if (prefix) {
        buf[0] = ' ';
        buf[1] = ' ';
        if (n) memcpy(buf + 2, line, n);
        buf[2 + n] = '\0';
    } else {
        if (n) memcpy(buf, line, n);
        buf[n] = '\0';
    }
    out = json_string(buf);
    free(buf);
    return out;
}

/* str.splitlines(): the separators a streamed reply can carry. */
static void gs_push_lines(json_t *arr, const char *s, int verbatim) {
    size_t n = strlen(s), start = 0, i = 0;
    while (i < n) {
        char c = s[i];
        int sep = 0;
        size_t skip = 1;
        if (c == '\n' || c == '\v' || c == '\f' || c == '\r') {
            sep = 1;
            if (c == '\r' && i + 1 < n && s[i + 1] == '\n') skip = 2;
        }
        if (sep) {
            size_t ln = i - start;
            json_append(arr, gs_line_out(s + start, ln, ln > 0 && !verbatim));
            i += skip;
            start = i;
            continue;
        }
        i++;
    }
    if (start < n) json_append(arr, gs_line_out(s + start, n - start, !verbatim));
    /* a trailing separator yields no final empty line, as splitlines() doesn't */
}

/* str(output) for the scalar shapes a reply's "output" can hold; NULL means
 * "not a shape this prints" (a container is out of scope). */
static json_t *gs_str_of(const json_t *v) {
    char buf[64];
    switch (v->type) {
    case JSON_STRING: return json_string(v->str_val ? v->str_val : "");
    case JSON_BOOL:   return json_string(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return json_string(buf);
    default: return NULL;
    }
}

json_t *w25_gatewayset_b9bfd0_render(const json_t *canonical, const json_t *reply,
                                     const json_t *stopped) {
    const char *canon = canonical && canonical->type == JSON_STRING && canonical->str_val
                            ? canonical->str_val : "";
    const json_t *output = NULL;
    json_t *text, *out;
    int verbatim;

    if (gs_truthy(stopped)) return json_null();
    if (reply && reply->type == JSON_OBJECT) output = json_obj_get(reply, "output");
    verbatim = gs_in(canon, GS_VERBATIM, GS_VERBATIM_N);
    text = output && gs_truthy(output) ? gs_str_of(output) : json_string("");
    out = json_array();
    if (text && text->type == JSON_STRING)
        gs_push_lines(out, text->str_val ? text->str_val : "", verbatim);
    json_free(text);
    return out;
}

json_t *w25_gatewayset_b9bfd0_constants(void) {
    json_t *o = json_object();
    json_t *settings = json_array(), *busy = json_array(), *verbatim = json_array();
    size_t i;
    for (i = 0; i < GS_SETTINGS_N; i++) json_append(settings, json_string(GS_SETTINGS[i]));
    for (i = 0; i < GS_BUSY_MODES_N; i++) json_append(busy, json_string(GS_BUSY_MODES[i]));
    for (i = 0; i < GS_VERBATIM_N; i++) json_append(verbatim, json_string(GS_VERBATIM[i]));
    json_set(o, "gateway_settings", settings);
    json_set(o, "busy_modes", busy);
    json_set(o, "verbatim", verbatim);
    json_set(o, "cron_command", json_string("cron"));
    return o;
}
