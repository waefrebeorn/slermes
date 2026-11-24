/*
 * values.c - C11 port of upstream plugins/config_backends/remote/values.py
 * (gate w24_values_5f77ea).
 *
 * PoP: w24_values_5f77ea_to_wire @ plugins/config_backends/remote/values.py:to_wire
 * PoP: w24_values_5f77ea_wire_key @ plugins/config_backends/remote/values.py:_wire_key
 * PoP: w24_values_5f77ea_is_secret_path @ plugins/config_backends/remote/values.py:is_secret_path
 * PoP: w24_values_5f77ea_secret_literal_path @ plugins/config_backends/remote/values.py:secret_literal_path
 * PoP: w24_values_5f77ea_constants @ plugins/config_backends/remote/values.py:MAX_SAFE_INTEGER,_secret_rules
 *
 * Ported: the wire conversion and the secret-shaped-key rule, both pure.  The
 * seams are the vendored fixture -- config-secret-keys.json, whose contents are
 * the constant tables below, so no file is opened -- and the plane's own
 * re-check on every write, which is the plane's job, not this client's.
 *
 * Two Python arms cannot be reached from a JSON harness and are therefore
 * stated, not exercised: a NaN/Infinity float (no JSON number literal) and a
 * non-JSON value type (the "a <type> value cannot be stored" arm).  Both are
 * still guarded below for C callers that build json_t by hand.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_values_5f77ea.h"

#define VW_MAX_SAFE_INTEGER 9007199254740991.0

/* config-secret-keys.json: secretKeys */
static const char *const VW_SECRET_KEYS[W24_VALUES_5F77EA_SECRET_KEYS] = {
    "api_key", "apikey", "key", "token", "access_token", "refresh_token", "id_token",
    "secret", "client_secret", "password", "passwd", "authorization", "private_key",
    "bearer", "jwt",
};
/* secretKeySuffixes */
static const char *const VW_SECRET_SUFFIXES[W24_VALUES_5F77EA_SECRET_SUFFIXES] = {
    "_api_key", "_token", "_secret", "_password", "_key", "_access_key",
};
/* nonSecretKeySuffixes */
static const char *const VW_NON_SECRET_SUFFIXES[W24_VALUES_5F77EA_NON_SECRET_SUFFIXES] = {
    "_url", "_host", "_user", "_id", "_domain", "_scheme",
};
/* envConfigKeys */
static const char *const VW_ENV_KEYS[W24_VALUES_5F77EA_ENV_KEYS] = {
    "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY", "VOICE_TOOLS_OPENAI_KEY",
    "EXA_API_KEY", "PARALLEL_API_KEY", "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL",
    "FIRECRAWL_GATEWAY_URL", "TOOL_GATEWAY_URL", "CONNECTOR_GATEWAY_URL", "TOOL_GATEWAY_DOMAIN",
    "TOOL_GATEWAY_SCHEME", "TOOL_GATEWAY_USER_TOKEN", "TAVILY_API_KEY", "PERPLEXITY_API_KEY",
    "API_SERVER_KEY", "BROWSERBASE_API_KEY", "BROWSERBASE_PROJECT_ID", "BROWSER_USE_API_KEY",
    "FAL_KEY", "TELEGRAM_BOT_TOKEN", "DISCORD_BOT_TOKEN", "TERMINAL_SSH_HOST",
    "TERMINAL_SSH_USER", "TERMINAL_SSH_KEY", "SUDO_PASSWORD", "SLACK_BOT_TOKEN",
    "SLACK_APP_TOKEN", "GITHUB_TOKEN", "HONCHO_API_KEY",
};
/* envConfigKeySuffixes */
static const char *const VW_ENV_SUFFIXES[W24_VALUES_5F77EA_ENV_SUFFIXES] = {
    "_API_KEY", "_TOKEN", "_SECRET",
};
/* envConfigKeyPrefixes */
static const char *const VW_ENV_PREFIXES[W24_VALUES_5F77EA_ENV_PREFIXES] = {
    "TERMINAL_SSH",
};
/* nonSecretPaths: the three tuples, flattened with their lengths. */
static const char *const VW_NON_SECRET_PATHS[W24_VALUES_5F77EA_NON_SECRET_PATHS][3] = {
    { "voice", "record_key", NULL },
    { "terminal", "docker_shared_container_key", NULL },
    { "browser", "camofox", "session_key" },
};
static const size_t VW_NON_SECRET_PATH_LEN[W24_VALUES_5F77EA_NON_SECRET_PATHS] = { 2, 2, 3 };

static char *vw_strdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    if (s) memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

/* paths.encode: each segment escapes \ as \\ and . as \., joined with ".". */
static char *vw_encode(const char *const *seg, size_t n) {
    size_t i, j, cap = 1, o = 0;
    char *out;

    for (i = 0; i < n; i++) {
        const char *s = seg[i] ? seg[i] : "";
        for (j = 0; s[j]; j++) cap += (s[j] == '\\' || s[j] == '.') ? 2 : 1;
        cap += 1; /* the joining dot */
    }
    out = (char *)malloc(cap);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        const char *s = seg[i] ? seg[i] : "";
        if (i > 0) out[o++] = '.';
        for (j = 0; s[j]; j++) {
            if (s[j] == '\\' || s[j] == '.') out[o++] = '\\';
            out[o++] = s[j];
        }
    }
    out[o] = '\0';
    return out;
}

/* f"{encode(path) or '<root>'}" */
static char *vw_label(const char *const *seg, size_t n) {
    char *enc = vw_encode(seg, n);
    if (!enc) return NULL;
    if (enc[0] == '\0') {
        char *root = vw_strdup("<root>");
        free(enc);
        return root;
    }
    return enc;
}

static char *vw_concat(const char *a, const char *b, const char *c, const char *d,
                       const char *e, const char *f) {
    size_t n = 0;
    char *out;
    if (a) n += strlen(a);
    if (b) n += strlen(b);
    if (c) n += strlen(c);
    if (d) n += strlen(d);
    if (e) n += strlen(e);
    if (f) n += strlen(f);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    out[0] = '\0';
    if (a) strcat(out, a);
    if (b) strcat(out, b);
    if (c) strcat(out, c);
    if (d) strcat(out, d);
    if (e) strcat(out, e);
    if (f) strcat(out, f);
    return out;
}

/* A JSON number that is an exact int64 is a Python int on the wire (JSON has
 * no int/float tag); anything else is a Python float. */
static int vw_num_is_int(double v) {
    return v == (double)(long long)v && v > -9.3e18 && v < 9.3e18;
}

/* type(v).__name__ */
static const char *vw_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
    case JSON_BOOL:   return "bool";
    case JSON_NUMBER: return vw_num_is_int(v->num_val) ? "int" : "float";
    case JSON_STRING: return "str";
    case JSON_ARRAY:  return "list";
    case JSON_OBJECT: return "dict";
    default:          return "object";
    }
}

/* repr(s) for a C string. */
static char *vw_repr_str(const char *s) {
    size_t n, i, o = 0;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n * 4 + 3);
    if (!out) return NULL;
    out[o++] = '\'';
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\'' || c == '\\') { out[o++] = '\\'; out[o++] = (char)c; }
        else if (c == '\n') { out[o++] = '\\'; out[o++] = 'n'; }
        else if (c == '\t') { out[o++] = '\\'; out[o++] = 't'; }
        else if (c == '\r') { out[o++] = '\\'; out[o++] = 'r'; }
        else if (c < 0x20 || c == 0x7f) { snprintf(out + o, 5, "\\x%02x", c); o += 4; }
        else out[o++] = (char)c;
    }
    out[o++] = '\'';
    out[o] = '\0';
    return out;
}

/* repr(v) for the keys and values that appear in an error message. */
static char *vw_repr(const json_t *v) {
    char buf[64];
    char *out;
    if (!v || v->type == JSON_NULL) return vw_strdup("None");
    switch (v->type) {
    case JSON_BOOL: return vw_strdup(v->bool_val ? "True" : "False");
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            return vw_strdup(buf);
        }
        snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return vw_strdup(buf);
    case JSON_STRING:
        return vw_repr_str(v->str_val ? v->str_val : "");
    default: {
        char *d = json_dumps(v, 0);
        out = vw_strdup(d ? d : "None");
        free(d);
        return out;
    }
    }
}

/* The error node every arm returns: {"ok": false, "error": <msg>}.  Takes
 * ownership of msg. */
static json_t *vw_fail(char *msg) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(false));
    json_set(o, "error", json_string(msg ? msg : ""));
    free(msg);
    return o;
}

static json_t *vw_ok(json_t *value) {
    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "value", value ? value : json_null());
    return o;
}

/* An integral JSON number is a Python int (the wire has no int/float tag); a
 * fractional one is a Python float. */
static int vw_is_int(const json_t *v) {
    return v && v->type == JSON_NUMBER && vw_num_is_int(v->num_val);
}

/* _wire_key(key, path) -> malloc'd string, or NULL with *err set. */
static char *vw_wire_key(const json_t *key, const char *const *seg, size_t n, char **err) {
    if (key && key->type == JSON_STRING) return vw_strdup(key->str_val ? key->str_val : "");
    if (vw_is_int(key)) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%lld", (long long)key->num_val);
        return vw_strdup(buf);
    }
    {
        char *label = vw_label(seg, n);
        char *repr = vw_repr(key);
        char *msg = vw_concat(label, ": a ", vw_type_name(key),
                              " mapping key (", repr,
                              ") cannot be stored in Remote Config; quote it in YAML");
        free(label);
        free(repr);
        *err = msg;
    }
    return NULL;
}

/* The value-level conversion; NULL with *err set on a ConfigValueError. */
static json_t *vw_to_wire(const json_t *value, const char *const *seg, size_t n, char **err);

/* A {"$map": [[k, v], ...]} document: the harness form of a mapping whose keys
 * are not all strings. */
static int vw_is_map(const json_t *v, const json_t **entries) {
    if (!v || v->type != JSON_OBJECT || json_object_size(v) != 1) return 0;
    if (!json_obj_get(v, "$map")) return 0;
    *entries = json_obj_get(v, "$map");
    return (*entries && (*entries)->type == JSON_ARRAY);
}

static json_t *vw_to_wire_map(const json_t *entries, const char *const *seg, size_t n,
                              char **err) {
    json_t *out = json_object();
    size_t i, count = json_len(entries);

    for (i = 0; i < count; i++) {
        const json_t *pair = json_get(entries, i);
        const json_t *k, *v;
        char *key, *child[W24_VALUES_5F77EA_MAX_DEPTH];
        json_t *conv;

        if (!pair || pair->type != JSON_ARRAY || json_len(pair) != 2) continue;
        k = json_get(pair, 0);
        v = json_get(pair, 1);
        key = vw_wire_key(k, seg, n, err);
        if (!key) { json_free(out); return NULL; }
        if (json_obj_get(out, key)) {
            char *childseg[W24_VALUES_5F77EA_MAX_DEPTH];
            char *keyrepr = vw_repr_str(key);
            char *label;
            size_t c;
            for (c = 0; c < n; c++) childseg[c] = (char *)seg[c];
            childseg[n] = key;
            label = vw_label((const char *const *)childseg, n + 1);
            *err = vw_concat(label, ": two keys become the same string key ", keyrepr, NULL, NULL, NULL);
            free(label);
            free(keyrepr);
            free(key);
            json_free(out);
            return NULL;
        }
        {
            size_t c;
            for (c = 0; c < n; c++) child[c] = (char *)seg[c];
        }
        child[n] = key;
        conv = vw_to_wire(v, (const char *const *)child, n + 1, err);
        if (!conv) { free(key); json_free(out); return NULL; }
        json_set(out, key, conv);
        free(key);
    }
    return out;
}

static json_t *vw_to_wire(const json_t *value, const char *const *seg, size_t n, char **err) {
    json_t *out, *conv;
    size_t i, count;

    if (n > W24_VALUES_5F77EA_MAX_DEPTH) {
        *err = vw_strdup("config value nests deeper than the wire allows");
        return NULL;
    }
    if (!value || value->type == JSON_NULL) return json_null();
    switch (value->type) {
    case JSON_NULL:   return json_null();
    case JSON_STRING: return json_string(value->str_val ? value->str_val : "");
    case JSON_BOOL:   return json_bool(value->bool_val);
    case JSON_NUMBER:
        if (vw_is_int(value)) {
            if (value->num_val >= -VW_MAX_SAFE_INTEGER && value->num_val <= VW_MAX_SAFE_INTEGER)
                return json_number(value->num_val);
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)value->num_val);
                return json_string(buf);
            }
        }
        if (!isfinite(value->num_val)) {
            char *label = vw_label(seg, n);
            *err = vw_concat(label, ": NaN/Infinity cannot be stored in Remote Config",
                             NULL, NULL, NULL, NULL);
            free(label);
            return NULL;
        }
        return json_number(value->num_val);
    case JSON_ARRAY:
        count = json_len(value);
        out = json_array();
        for (i = 0; i < count; i++) {
            /* list elements inherit the list's path (§14.3) */
            conv = vw_to_wire(json_get(value, i), seg, n, err);
            if (!conv) { json_free(out); return NULL; }
            json_append(out, conv);
        }
        return out;
    case JSON_OBJECT: {
        const json_t *entries = NULL;
        char *key, *child[W24_VALUES_5F77EA_MAX_DEPTH];
        size_t c;

        if (n + 1 >= W24_VALUES_5F77EA_MAX_DEPTH) {
            *err = vw_strdup("config value nests deeper than the wire allows");
            return NULL;
        }
        if (vw_is_map(value, &entries)) return vw_to_wire_map(entries, seg, n, err);
        out = json_object();
        count = json_object_size(value);
        for (i = 0; i < count; i++) {
            const char *k = json_object_get_key_at(value, i);
            const json_t *v = json_object_get_at(value, i);
            /* a JSON object's keys are strings, and parsing cannot repeat one,
             * so _wire_key is the identity here. */
            key = vw_strdup(k ? k : "");
            if (!key) { json_free(out); *err = vw_strdup("out of memory"); return NULL; }
            for (c = 0; c < n; c++) child[c] = (char *)seg[c];
            child[n] = key;
            conv = vw_to_wire(v, (const char *const *)child, n + 1, err);
            if (!conv) { free(key); json_free(out); return NULL; }
            json_set(out, key, conv);
            free(key);
        }
        return out;
    }
    }
    *err = vw_strdup("value cannot be stored in Remote Config");
    return NULL;
}

/* The request's "path": [...] as a segment vector; missing -> the root. */
static size_t vw_path_of(const json_t *req, const char **seg) {
    const json_t *p = req && req->type == JSON_OBJECT ? json_obj_get(req, "path") : NULL;
    size_t i, n = (p && p->type == JSON_ARRAY) ? json_len(p) : 0;
    if (n > W24_VALUES_5F77EA_MAX_DEPTH) n = W24_VALUES_5F77EA_MAX_DEPTH;
    for (i = 0; i < n; i++) {
        const json_t *s = json_get(p, i);
        seg[i] = (s && s->type == JSON_STRING && s->str_val) ? s->str_val : "";
    }
    return n;
}

json_t *w24_values_5f77ea_to_wire(const json_t *req) {
    const char *seg[W24_VALUES_5F77EA_MAX_DEPTH];
    size_t n = vw_path_of(req, seg);
    char *err = NULL;
    json_t *out = vw_to_wire(req && req->type == JSON_OBJECT ? json_obj_get(req, "value") : NULL,
                             seg, n, &err);
    if (!out) return vw_fail(err);
    return vw_ok(out);
}

json_t *w24_values_5f77ea_wire_key(const json_t *req) {
    const char *seg[W24_VALUES_5F77EA_MAX_DEPTH];
    size_t n = vw_path_of(req, seg);
    char *err = NULL;
    char *key = vw_wire_key(req && req->type == JSON_OBJECT ? json_obj_get(req, "key") : NULL,
                            seg, n, &err);
    if (!key) return vw_fail(err);
    {
        json_t *out = vw_ok(json_string(key));
        free(key);
        return out;
    }
}

/* Python str.lower() / str.replace("-", "_") over the ASCII a key carries. */
static char *vw_leaf(const char *s) {
    size_t i, n = strlen(s ? s : "");
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c == '-') c = '_';
        else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out[i] = c;
    }
    out[n] = '\0';
    return out;
}

static int vw_ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return b <= a && strcmp(s + a - b, suffix) == 0;
}

static int vw_in_list(const char *s, const char *const *items, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (strcmp(s, items[i]) == 0) return 1;
    return 0;
}

static int vw_ends_any(const char *s, const char *const *items, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (vw_ends_with(s, items[i])) return 1;
    return 0;
}

static int vw_starts_any(const char *s, const char *const *items, size_t n) {
    size_t i;
    for (i = 0; i < n; i++)
        if (strncmp(s, items[i], strlen(items[i])) == 0) return 1;
    return 0;
}

/* _is_env_config_key(key, rules) */
static int vw_is_env_config_key(const char *key) {
    char *up;
    int hit;
    size_t i, n;

    if (strchr(key, '.')) return 0;
    n = strlen(key);
    up = (char *)malloc(n + 1);
    if (!up) return 0;
    for (i = 0; i < n; i++) {
        char c = key[i];
        up[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }
    up[n] = '\0';
    hit = vw_in_list(up, VW_ENV_KEYS, W24_VALUES_5F77EA_ENV_KEYS) ||
          vw_ends_any(up, VW_ENV_SUFFIXES, W24_VALUES_5F77EA_ENV_SUFFIXES) ||
          vw_starts_any(up, VW_ENV_PREFIXES, W24_VALUES_5F77EA_ENV_PREFIXES);
    free(up);
    return hit;
}

/* is_secret_path(segments) over NUL-terminated segments. */
static int vw_is_secret_segments(const char *const *seg, size_t n) {
    char *leaf;
    int secret;
    size_t i;

    if (n == 0) return 0;
    for (i = 0; i < W24_VALUES_5F77EA_NON_SECRET_PATHS; i++) {
        if (VW_NON_SECRET_PATH_LEN[i] != n) continue;
        {
            size_t j;
            int same = 1;
            for (j = 0; j < n; j++)
                if (strcmp(seg[j] ? seg[j] : "", VW_NON_SECRET_PATHS[i][j]) != 0) { same = 0; break; }
            if (same) return 0;
        }
    }
    leaf = vw_leaf(seg[n - 1]);
    if (!leaf) return 0;
    if (n == 1 && vw_is_env_config_key(seg[0] ? seg[0] : "")) {
        secret = !vw_ends_any(leaf, VW_NON_SECRET_SUFFIXES, W24_VALUES_5F77EA_NON_SECRET_SUFFIXES);
        free(leaf);
        return secret;
    }
    secret = vw_in_list(leaf, VW_SECRET_KEYS, W24_VALUES_5F77EA_SECRET_KEYS) ||
             vw_ends_any(leaf, VW_SECRET_SUFFIXES, W24_VALUES_5F77EA_SECRET_SUFFIXES);
    free(leaf);
    return secret;
}

json_t *w24_values_5f77ea_is_secret_path(const json_t *segments) {
    const char *seg[W24_VALUES_5F77EA_MAX_DEPTH];
    size_t i, n = (segments && segments->type == JSON_ARRAY) ? json_len(segments) : 0;
    if (n > W24_VALUES_5F77EA_MAX_DEPTH) n = W24_VALUES_5F77EA_MAX_DEPTH;
    for (i = 0; i < n; i++) {
        const json_t *s = json_get(segments, i);
        seg[i] = (s && s->type == JSON_STRING && s->str_val) ? s->str_val : "";
    }
    return json_bool(vw_is_secret_segments(seg, n) ? true : false);
}

/* placeholderPattern: ^\$\{[A-Za-z_][A-Za-z0-9_]*\}$ , fullmatch. */
static int vw_placeholder(const char *s) {
    size_t len = strlen(s), i;
    if (len < 4) return 0;
    if (!(s[0] == '$' && s[1] == '{' && s[len - 1] == '}')) return 0;
    if (len - 3 < 1) return 0;
    {
        char c = s[2];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_')) return 0;
    }
    for (i = 3; i + 1 < len; i++) {
        char c = s[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_')) return 0;
    }
    return 1;
}

/* The node shapes §14.3 counts as a secret literal: a non-empty str that is not
 * a ${PLACEHOLDER}, an int or a float.  A bool and None never are. */
static int vw_is_literal(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_BOOL || v->type == JSON_NULL) return 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        return s[0] != '\0' && !vw_placeholder(s);
    }
    return v->type == JSON_NUMBER;
}

static char **vw_path_copy(const char *const *seg, size_t n) {
    char **hit = (char **)calloc(n + 1, sizeof(char *));
    size_t j;
    if (!hit) return NULL;
    for (j = 0; j < n; j++) hit[j] = vw_strdup(seg[j] ? seg[j] : "");
    hit[n] = NULL;
    return hit;
}

/* _walk + the §14.3 filter: the first (path, value) whose path is secret and
 * whose value is a literal.  Returns the path segments (caller frees). */
static char **vw_walk(const json_t *value, const char *const *seg, size_t n, size_t *out_n) {
    size_t i, count;

    if (n > 0 && vw_is_secret_segments(seg, n) && vw_is_literal(value)) {
        *out_n = n;
        return vw_path_copy(seg, n);
    }
    if (!value || n + 1 >= W24_VALUES_5F77EA_MAX_DEPTH) return NULL;
    if (value->type == JSON_OBJECT) {
        count = json_object_size(value);
        for (i = 0; i < count; i++) {
            const char *k = json_object_get_key_at(value, i);
            const json_t *v = json_object_get_at(value, i);
            const char *child[W24_VALUES_5F77EA_MAX_DEPTH];
            size_t j;
            char **hit;
            for (j = 0; j < n; j++) child[j] = seg[j];
            child[n] = k ? k : "";
            hit = vw_walk(v, child, n + 1, out_n);
            if (hit) return hit;
        }
    } else if (value->type == JSON_ARRAY) {
        count = json_len(value);
        for (i = 0; i < count; i++) {   /* list elements inherit the list's path */
            char **hit = vw_walk(json_get(value, i), seg, n, out_n);
            if (hit) return hit;
        }
    }
    return NULL;
}

json_t *w24_values_5f77ea_secret_literal_path(const json_t *req) {
    const char *seg[W24_VALUES_5F77EA_MAX_DEPTH];
    size_t n = vw_path_of(req, seg), out_n = 0, i;
    char **hit;
    json_t *out;

    hit = vw_walk(req && req->type == JSON_OBJECT ? json_obj_get(req, "value") : NULL,
                  seg, n, &out_n);
    if (!hit) return json_null();
    out = json_array();
    for (i = 0; i < out_n; i++) {
        json_append(out, json_string(hit[i] ? hit[i] : ""));
        free(hit[i]);
    }
    free(hit);
    return out;
}

static json_t *vw_str_array(const char *const *items, size_t n) {
    json_t *a = json_array();
    size_t i;
    for (i = 0; i < n; i++) json_append(a, json_string(items[i]));
    return a;
}

json_t *w24_values_5f77ea_constants(void) {
    json_t *o = json_object();
    json_t *paths = json_array();
    size_t i;

    for (i = 0; i < W24_VALUES_5F77EA_NON_SECRET_PATHS; i++)
        json_append(paths, vw_str_array(VW_NON_SECRET_PATHS[i], VW_NON_SECRET_PATH_LEN[i]));
    json_set(o, "max_safe_integer", json_number(VW_MAX_SAFE_INTEGER));
    json_set(o, "secret_keys", vw_str_array(VW_SECRET_KEYS, W24_VALUES_5F77EA_SECRET_KEYS));
    json_set(o, "secret_key_suffixes",
             vw_str_array(VW_SECRET_SUFFIXES, W24_VALUES_5F77EA_SECRET_SUFFIXES));
    json_set(o, "non_secret_key_suffixes",
             vw_str_array(VW_NON_SECRET_SUFFIXES, W24_VALUES_5F77EA_NON_SECRET_SUFFIXES));
    json_set(o, "env_config_keys", vw_str_array(VW_ENV_KEYS, W24_VALUES_5F77EA_ENV_KEYS));
    json_set(o, "env_config_key_suffixes",
             vw_str_array(VW_ENV_SUFFIXES, W24_VALUES_5F77EA_ENV_SUFFIXES));
    json_set(o, "env_config_key_prefixes",
             vw_str_array(VW_ENV_PREFIXES, W24_VALUES_5F77EA_ENV_PREFIXES));
    json_set(o, "non_secret_paths", paths);
    json_set(o, "placeholder_pattern", json_string("^\\$\\{[A-Za-z_][A-Za-z0-9_]*\\}$"));
    return o;
}
