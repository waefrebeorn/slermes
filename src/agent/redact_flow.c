/* redact_flow.c — FLOW + leaf port of agent/redact.py secret-redaction flows.
 *
 * Ports the redaction DECISION CHAINS (gate ordering, short-circuit `or`/`and`,
 * scope/source gates, masking branches, error paths) plus the pure key/URL-param
 * policy leaves. Every regex-dependent or foreign sub-call is INJECTED through
 * the rf_io_t universal seam: io->call(io, name, args) appends `name` to
 * io->trace and returns the scripted result (NULL/json_null == None;
 * {"__raise__": cls, "message": msg} == a raised exception). The decision logic
 * around each seam matches the live Python exactly; the regex leaves themselves
 * are verified by their own direct ports/cases, so scripting them here hides no
 * divergence.
 *
 * Seam name table (name -> live call):
 *   env_lookup_match                 _ENV_LOOKUP_VALUE_RE.match(value)
 *   key_has_secret_keyword           _key_has_secret_keyword(key)
 *   has_word_bounded_keyword_password _has_word_bounded_keyword(key, _PASSWORD_KEY_RE)
 *   has_word_bounded_keyword_strong  _has_word_bounded_keyword(key, _STRONG_KEY_KEYWORD_RE)
 *   has_word_bounded_keyword_yaml    _has_word_bounded_keyword(key, _YAML_CFG_WORD_RE)
 *   has_password_keyword             _has_password_keyword(key)
 *   path_or_var_match                _PATH_OR_VAR_VALUE_RE.match(value)
 *   opaque_path_segment_fullmatch    _OPAQUE_PATH_SEGMENT_RE.fullmatch(seg)
 *   is_glued_credential_env          _is_glued_credential_env(key)
 *   is_qualified_key_name            _is_qualified_key_name(key)
 *   config_placeholder_fullmatch     _CONFIG_PLACEHOLDER_VALUE_RE.fullmatch(value)
 *   looks_like_opaque_credential     _looks_like_opaque_credential(value)
 *   json_key_names_full_fullmatch    _JSON_KEY_NAMES_FULL_RE.fullmatch(x)
 *   dsn_pw_placeholder_fullmatch     _DSN_PW_PLACEHOLDER_RE.fullmatch(value)
 *   should_redact_assignment         _should_redact_assignment(key, value, check_keyword=True)
 *   json_value_suffix_sub            _JSON_VALUE_SUFFIX_RE.sub("", x)
 *   is_python_repr_secret_key        _is_python_repr_secret_key(x)
 *   canonical_url_param_name         _canonical_url_param_name(name)
 *   decode_url_param_name            _decode_url_param_name(name)
 *   redact_enabled                   _redact_enabled()
 *   redact_strict_url_credentials    _redact_strict_url_credentials(text)
 *   redact_sensitive_text            redact_sensitive_text(text, **kw)
 *   bearer_residue_sub               _BEARER_RESIDUE_RE.sub("Bearer [redacted]", text)
 *   env_dump_line_sub                _ENV_DUMP_LINE_RE.sub(..., redacted)
 *   get_hermes_home_override         hermes_constants.get_hermes_home_override()
 *   hermes_home_key                  hermes_constants.hermes_home_key()
 *   current_secret_scope             agent.secret_scope.current_secret_scope()
 *   command_segments                 _command_segments(command)
 *   ps_colon_param_fullmatch         _PS_COLON_PARAM_RE.fullmatch(arg) -> group(1) or None
 *   is_secret_file_arg               _is_secret_file_arg(arg)
 *   is_under_hermes_home             _is_under_hermes_home(path)
 *
 * Pure leaves ported directly (no seam): _key_name_words, _is_qualified_key_name,
 * _is_python_repr_secret_key, is_secret_field_name, _decode_url_param_name,
 * _canonical_url_param_name, note_env_policy, note_config_policy. ASCII subset:
 * casefold/isupper/strip model ASCII (fixtures are ASCII); unquote_plus decodes
 * ASCII percent-escapes. State (_REDACT_ENABLED / _ENV_POLICY_BY_HOME /
 * _CONFIG_POLICY_BY_HOME) rides io->state and is mutated in place.
 *
 * Ownership: seam results are caller-owned (json_free). io->trace / io->state are
 * harness-owned. rf_seam() frees its args.
 */
#ifndef REDACT_FLOW_C
#define REDACT_FLOW_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

typedef struct rf_io_t {
    json_t *(*call)(struct rf_io_t *io, const char *name, json_t *args);
    json_t *trace;
    json_t *state;
    void   *ctx;
} rf_io_t;

static void rf_trace(rf_io_t *io, const char *name) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(name));
}

static bool rf_raised(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

/* BaseException-only trio propagates through `except Exception` sites. */
static bool rf_is_base_exc(const json_t *r) {
    if (!rf_raised(r)) return false;
    json_t *k = json_obj_get(r, "__raise__");
    if (!k || k->type != JSON_STRING) return false;
    const char *s = k->str_val ? k->str_val : "";
    return strcmp(s, "SystemExit") == 0 || strcmp(s, "KeyboardInterrupt") == 0
        || strcmp(s, "GeneratorExit") == 0 || strcmp(s, "CancelledError") == 0;
}

/* Call a seam, record its name, free args, normalize NULL -> json_null. */
static json_t *rf_seam(rf_io_t *io, const char *name, json_t *args) {
    rf_trace(io, name);
    json_t *r = (io && io->call) ? io->call(io, name, args) : NULL;
    if (args) json_free(args);
    return r ? r : json_null();
}

static json_t *rf_args1(const char *k1, const char *v1) {
    json_t *a = json_object();
    json_set(a, k1, json_string(v1 ? v1 : ""));
    return a;
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool rf_truthy(const json_t *v) {
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

static bool rf_is_none(const json_t *v) {
    return !v || v->type == JSON_NULL;
}

static json_t *rf_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static const char *rf_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = rf_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}

static bool rf_get_bool(const json_t *obj, const char *key, bool def) {
    json_t *v = rf_get(obj, key);
    if (v && v->type == JSON_BOOL) return v->bool_val;
    return def;
}

/* str(v) for the scalar types the flows feed (None/bool/number/string). */
static char *rf_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    return ser ? ser : strdup("");
}

/* Python type name of a seam result (for AttributeError / TypeError messages). */
static const char *rf_py_type_name(const json_t *v) {
    if (!v || v->type == JSON_NULL) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER: {
            double d = v->num_val;
            return (d == floor(d) && fabs(d) < 1e15) ? "int" : "float";
        }
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

/* AttributeError: '<type>' object has no attribute '<attr>' (live .replace/.lower
 * on a non-str seam result). The flows call these OUTSIDE any try, so it propagates. */
static json_t *rf_attr_error(const json_t *v, const char *attr) {
    char msg[160];
    snprintf(msg, sizeof msg, "'%s' object has no attribute '%s'", rf_py_type_name(v), attr);
    json_t *e = json_object();
    json_set(e, "__raise__", json_string("AttributeError"));
    json_set(e, "message", json_string(msg));
    return e;
}

/* TypeError: argument of type '<type>' is not a container or iterable
 * (live `x in <scalar>`, CPython 3.14 wording — the oracle's interpreter). */
static json_t *rf_type_error_not_iterable(const json_t *v) {
    char msg[160];
    snprintf(msg, sizeof msg, "argument of type '%s' is not a container or iterable", rf_py_type_name(v));
    json_t *e = json_object();
    json_set(e, "__raise__", json_string("TypeError"));
    json_set(e, "message", json_string(msg));
    return e;
}

/* Python `needle in hay`: substring for str, element equality for list, key
 * equality for dict. Returns 1 present, 0 absent, -1 TypeError (non-iterable
 * scalar: int/float/bool/None). Only reached for TRUTHY hay (falsy short-circuits
 * earlier in the live `not x or needle in x` gates). */
static int rf_contains(const json_t *hay, const char *needle) {
    if (!hay) return -1;
    switch (hay->type) {
        case JSON_STRING:
            return (hay->str_val && strstr(hay->str_val, needle)) ? 1 : 0;
        case JSON_ARRAY:
            for (size_t i = 0; i < json_len(hay); i++) {
                json_t *e = json_get(hay, i);
                if (e && e->type == JSON_STRING && e->str_val && strcmp(e->str_val, needle) == 0)
                    return 1;
            }
            return 0;
        case JSON_OBJECT:
            return json_obj_get(hay, needle) ? 1 : 0;
        default:
            return -1;
    }
}

static char *rf_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (char)tolower((unsigned char)p[i]);
    o[n] = '\0';
    return o;
}

static bool rf_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* ASCII str.strip() (fixtures are ASCII policy strings). */
static char *rf_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && rf_is_ws((unsigned char)p[a])) a++;
    while (b > a && rf_is_ws((unsigned char)p[b - 1])) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}

static bool rf_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}

static bool rf_endswith(const char *s, const char *suf) {
    if (!s || !suf) return false;
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && strcmp(s + n - m, suf) == 0;
}

static bool rf_in_set(const char *const *set, const char *val) {
    if (!val) return false;
    for (size_t i = 0; set[i]; i++)
        if (strcmp(set[i], val) == 0) return true;
    return false;
}

/* str.strip("\"'"): strip any run of double/single quotes from both ends. */
static char *rf_strip_quotes(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && (p[a] == '"' || p[a] == '\'')) a++;
    while (b > a && (p[b - 1] == '"' || p[b - 1] == '\'')) b--;
    char *o = malloc(b - a + 1);
    if (!o) return NULL;
    memcpy(o, p + a, b - a);
    o[b - a] = '\0';
    return o;
}

/* str.replace(old_char, new_char) for single chars. */
static char *rf_replace_char(const char *s, char oldc, char newc) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *o = malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; i++) o[i] = (p[i] == oldc) ? newc : p[i];
    o[n] = '\0';
    return o;
}

/* ── live module constants (byte-pinned to agent/redact.py + siblings) ─── */
static const char *RF_CONFIG_LITERAL_VALUES[] = {
    "true", "false", "yes", "no", "on", "off", "null", "none", NULL,
};
static const char *RF_SENSITIVE_QUERY_PARAMS[] = {
    "access_token", "refresh_token", "id_token", "token", "api_key", "apikey",
    "client_secret", "password", "auth", "jwt", "session", "secret", "key",
    "code", "signature", "x-amz-signature", "x-goog-signature", "sig",
    "usertoken", NULL,
};
static const char *RF_PYTHON_REPR_SECRET_KEYS[] = {
    "access_token", "refresh_token", "id_token", "auth_token", "token",
    "api_key", "apikey", "client_secret", "secret", "password", "passwd",
    "private_key", "credential", "credentials", "authorization", "bearer",
    "secret_value", "raw_secret", "secret_input", "key_material", NULL,
};
static const char *RF_PYTHON_REPR_ENV_SUFFIXES[] = {
    "_API_KEY", "_TOKEN", "_SECRET", "_PASSWORD", "_PASSWD", "_CREDENTIAL",
    "_CREDENTIALS", NULL,
};
static const char *RF_PYTHON_REPR_CREDENTIAL_SUFFIXES[] = {
    "apikey", "api_key", "token", "secret", "password", "passwd",
    "credential", "credentials", NULL,
};
static const char *RF_KEY_SECRET_QUALIFIERS[] = {
    "secret", "private", "signing", "encryption", "encrypt", "master", "hmac",
    "symmetric", "access", NULL,
};
static const char *RF_ENV_FILE_BASENAMES[] = {
    ".env", ".env.local", ".env.development", ".env.production", ".env.test",
    ".env.staging", ".envrc", NULL,
};
static const char *RF_SHELL_RC_BASENAMES[] = {
    ".bashrc", ".bash_profile", ".bash_login", ".profile",
    ".zshrc", ".zprofile", ".zlogin", ".zshenv", NULL,
};
static const char *RF_HOME_CREDENTIAL_FILES[] = {
    ".netrc", "_netrc", ".pgpass", ".npmrc", ".pypirc", ".git-credentials", NULL,
};
static const char *RF_FILE_READ_COMMANDS[] = {
    "cat", "head", "tail", "type", "bat", "less", "more", "nl",
    "zcat", "tac", "view", "batcat", "grep", "awk", "sed", "get-content", "gc",
    NULL,
};
static const char *RF_PATTERN_FIRST_COMMANDS[] = { "grep", "awk", "sed", NULL };
static const char *RF_HERMES_HOME_PREFIXES[] = { "$HERMES_HOME/", "${HERMES_HOME}/", NULL };
static const char *RF_HOME_PREFIXES[] = { "$HOME/", "${HOME}/", NULL };
static const char *RF_ENABLED_TRUE_WORDS[] = { "1", "true", "yes", "on", NULL };
#define RF_REDACTION_UNAVAILABLE "[redaction-unavailable]"

/* ── pure policy leaves (ported directly, no seam) ─────────────────────── */

/* split s on runs of non-[A-Za-z0-9], drop empty fields -> json array of str */
static json_t *rf_split_alnum_lower(const char *s) {
    json_t *arr = json_array();
    const char *p = s ? s : "";
    size_t n = strlen(p), i = 0;
    while (i < n) {
        while (i < n && !isalnum((unsigned char)p[i])) i++;
        size_t a = i;
        while (i < n && isalnum((unsigned char)p[i])) i++;
        if (i > a) {
            char *w = malloc(i - a + 1);
            if (w) {
                for (size_t k = a; k < i; k++) w[k - a] = (char)tolower((unsigned char)p[k]);
                w[i - a] = '\0';
                json_append(arr, json_string(w));
                free(w);
            }
        }
    }
    return arr;
}

/* PoP: rf_key_name_words @ agent/redact.py:_key_name_words */
json_t *rf_key_name_words(const char *key) {
    const char *p = key ? key : "";
    size_t n = strlen(p);
    /* re.sub(r"(?<=[a-z0-9])(?=[A-Z])", "_", key): zero-width insert before an
     * uppercase char whose ORIGINAL predecessor is [a-z0-9]. */
    char *spaced = malloc(2 * n + 1);
    if (!spaced) return json_array();
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char cur = (unsigned char)p[i];
        if (i > 0) {
            unsigned char prev = (unsigned char)p[i - 1];
            bool prev_ok = (prev >= 'a' && prev <= 'z') || (prev >= '0' && prev <= '9');
            bool cur_up = (cur >= 'A' && cur <= 'Z');
            if (prev_ok && cur_up) spaced[o++] = '_';
        }
        spaced[o++] = (char)cur;
    }
    spaced[o] = '\0';
    char *low = rf_lower(spaced);
    free(spaced);
    json_t *arr = rf_split_alnum_lower(low ? low : "");
    free(low);
    return arr;
}

/* PoP: rf_is_qualified_key_name @ agent/redact.py:_is_qualified_key_name */
bool rf_is_qualified_key_name(const char *key) {
    json_t *words = rf_key_name_words(key);
    size_t nw = json_len(words);
    bool res = false;
    if (nw >= 2) {
        json_t *last = json_get(words, nw - 1);
        const char *lw = (last && last->type == JSON_STRING) ? last->str_val : "";
        if (strcmp(lw ? lw : "", "key") == 0) {
            for (size_t i = 0; i + 1 < nw; i++) {
                json_t *w = json_get(words, i);
                const char *ws = (w && w->type == JSON_STRING) ? w->str_val : "";
                if (rf_in_set(RF_KEY_SECRET_QUALIFIERS, ws ? ws : "")) { res = true; break; }
            }
        }
    }
    json_free(words);
    return res;
}

/* ASCII key.isupper(): >=1 cased char, all cased chars uppercase. */
static bool rf_isupper(const char *s) {
    const char *p = s ? s : "";
    bool any_cased = false;
    for (size_t i = 0; p[i]; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 'a' && c <= 'z') return false;
        if (c >= 'A' && c <= 'Z') any_cased = true;
    }
    return any_cased;
}

static bool rf_endswith_any(const char *s, const char *const *sufs) {
    for (size_t i = 0; sufs[i]; i++)
        if (rf_endswith(s, sufs[i])) return true;
    return false;
}

/* PoP: rf_is_python_repr_secret_key @ agent/redact.py:_is_python_repr_secret_key */
bool rf_is_python_repr_secret_key(const char *key) {
    const char *k = key ? key : "";
    char *folded = rf_lower(k);  /* ASCII casefold */
    bool res = false;
    if (rf_in_set(RF_PYTHON_REPR_SECRET_KEYS, folded)) res = true;
    else if (rf_isupper(k) && rf_endswith_any(k, RF_PYTHON_REPR_ENV_SUFFIXES)) res = true;
    else if (rf_endswith_any(folded, RF_PYTHON_REPR_CREDENTIAL_SUFFIXES)) res = true;
    else res = rf_is_qualified_key_name(k);
    free(folded);
    return res;
}

/* PoP: rf_is_secret_field_name @ agent/redact.py:is_secret_field_name */
bool rf_is_secret_field_name(const json_t *key) {
    if (!key || key->type != JSON_STRING) return false;
    return rf_is_python_repr_secret_key(key->str_val ? key->str_val : "");
}

/* UTF-8 decode with errors='replace' (U+FFFD per invalid lead byte). */
static char *rf_utf8_decode_replace(const unsigned char *b, size_t n) {
    char *o = malloc(3 * n + 1);
    if (!o) return NULL;
    size_t oi = 0, i = 0;
    while (i < n) {
        unsigned char c = b[i];
        if (c < 0x80) { o[oi++] = (char)c; i++; continue; }
        int need; unsigned int cp; unsigned int lo = 0x80, hi = 0xBF;
        if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) {
            need = 2; cp = c & 0x0F;
            if (c == 0xE0) lo = 0xA0;
            if (c == 0xED) hi = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 3; cp = c & 0x07;
            if (c == 0xF0) lo = 0x90;
            if (c == 0xF4) hi = 0x8F;
        } else { o[oi++] = (char)0xEF; o[oi++] = (char)0xBF; o[oi++] = (char)0xBD; i++; continue; }
        bool ok = true;
        unsigned int v = cp;
        for (int k = 1; k <= need; k++) {
            if (i + (size_t)k >= n) { ok = false; break; }
            unsigned char cc = b[i + k];
            unsigned int l = (k == 1) ? lo : 0x80, h = (k == 1) ? hi : 0xBF;
            if (cc < l || cc > h) { ok = false; break; }
            v = (v << 6) | (cc & 0x3F);
        }
        if (!ok) { o[oi++] = (char)0xEF; o[oi++] = (char)0xBF; o[oi++] = (char)0xBD; i++; continue; }
        if (v <= 0x7F) o[oi++] = (char)v;
        else if (v <= 0x7FF) { o[oi++] = (char)(0xC0 | (v >> 6)); o[oi++] = (char)(0x80 | (v & 0x3F)); }
        else if (v <= 0xFFFF) { o[oi++] = (char)(0xE0 | (v >> 12)); o[oi++] = (char)(0x80 | ((v >> 6) & 0x3F)); o[oi++] = (char)(0x80 | (v & 0x3F)); }
        else { o[oi++] = (char)(0xF0 | (v >> 18)); o[oi++] = (char)(0x80 | ((v >> 12) & 0x3F)); o[oi++] = (char)(0x80 | ((v >> 6) & 0x3F)); o[oi++] = (char)(0x80 | (v & 0x3F)); }
        i += (size_t)need + 1;
    }
    o[oi] = '\0';
    return o;
}

static int rf_hexval(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* urllib.parse.unquote_plus (UTF-8, errors='replace'). */
static char *rf_unquote_plus(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    unsigned char *bytes = malloc(n + 1);
    if (!bytes) return strdup("");
    size_t bn = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '+') { bytes[bn++] = ' '; continue; }
        if (p[i] == '%' && i + 2 < n) {
            int h1 = rf_hexval((unsigned char)p[i + 1]), h2 = rf_hexval((unsigned char)p[i + 2]);
            if (h1 >= 0 && h2 >= 0) { bytes[bn++] = (unsigned char)((h1 << 4) | h2); i += 2; continue; }
        }
        bytes[bn++] = (unsigned char)p[i];
    }
    char *o = rf_utf8_decode_replace(bytes, bn);
    free(bytes);
    return o ? o : strdup("");
}

/* PoP: rf_decode_url_param_name @ agent/redact.py:_decode_url_param_name */
char *rf_decode_url_param_name(const char *name) {
    char *decoded = strdup(name ? name : "");
    for (int round = 0; round < 3; round++) {
        char *next = rf_unquote_plus(decoded);
        if (strcmp(next, decoded) == 0) { free(next); break; }
        free(decoded);
        decoded = next;
    }
    return decoded;
}

/* PoP: rf_canonical_url_param_name @ agent/redact.py:_canonical_url_param_name */
char *rf_canonical_url_param_name(const char *name) {
    char *dec = rf_decode_url_param_name(name);
    char *folded = rf_lower(dec);
    free(dec);
    if (rf_in_set(RF_SENSITIVE_QUERY_PARAMS, folded)) return folded;
    char *o = rf_replace_char(folded, '-', '_');
    free(folded);
    return o;
}

/* state dict helpers for the policy registry */
static json_t *rf_state_dict(rf_io_t *io, const char *key) {
    json_t *d = rf_get(io->state, key);
    if (!d || d->type != JSON_OBJECT) {
        d = json_object();
        json_set(io->state, key, d);
    }
    return d;
}

/* PoP: rf_note_env_policy @ agent/redact.py:note_env_policy */
void rf_note_env_policy(rf_io_t *io, const char *home_key, const json_t *raw) {
    json_t *d = rf_state_dict(io, "_ENV_POLICY_BY_HOME");
    if (rf_is_none(raw)) {
        json_set(d, home_key ? home_key : "", json_null());
    } else {
        char *tmp = rf_pystr(raw);
        json_set(d, home_key ? home_key : "", json_string(tmp));
        free(tmp);
    }
}

/* PoP: rf_note_config_policy @ agent/redact.py:note_config_policy */
void rf_note_config_policy(rf_io_t *io, const char *home_key, const json_t *raw) {
    json_t *d = rf_state_dict(io, "_CONFIG_POLICY_BY_HOME");
    if (rf_is_none(raw)) {
        json_set(d, home_key ? home_key : "", json_null());
    } else {
        char *tmp = rf_pystr(raw);
        json_set(d, home_key ? home_key : "", json_string(tmp));
        free(tmp);
    }
}

/* ── flow helpers ──────────────────────────────────────────────────────── */
static char *rf_substr(const char *s, size_t start, size_t len) {
    char *o = malloc(len + 1);
    if (!o) return NULL;
    memcpy(o, s + start, len);
    o[len] = '\0';
    return o;
}

/* str.split() (no args): runs of ASCII whitespace, drop empties. */
static json_t *rf_split_ws(const char *s) {
    json_t *arr = json_array();
    const char *p = s ? s : "";
    size_t n = strlen(p), i = 0;
    while (i < n) {
        while (i < n && rf_is_ws((unsigned char)p[i])) i++;
        size_t a = i;
        while (i < n && !rf_is_ws((unsigned char)p[i])) i++;
        if (i > a) {
            char *w = rf_substr(p, a, i - a);
            json_append(arr, json_string(w ? w : ""));
            free(w);
        }
    }
    return arr;
}

/* tokens[0].rsplit("/", 1)[-1].lower() */
static char *rf_basename_lower(const char *s) {
    const char *p = s ? s : "";
    const char *slash = strrchr(p, '/');
    const char *base = slash ? slash + 1 : p;
    return rf_lower(base);
}

/* [part.lower() for part in path.split("/") if part] */
static json_t *rf_split_slash_lower_nonempty(const char *s) {
    json_t *arr = json_array();
    const char *p = s ? s : "";
    size_t n = strlen(p), start = 0;
    for (;;) {
        size_t end = start;
        while (end < n && p[end] != '/') end++;
        if (end > start) {
            char *seg = rf_substr(p, start, end - start);
            char *low = rf_lower(seg);
            free(seg);
            json_append(arr, json_string(low ? low : ""));
            free(low);
        }
        if (end >= n) break;
        start = end + 1;
    }
    return arr;
}

/* ── FLOW ports (seamed decision chains) ───────────────────────────────── */

/* PoP: rf_should_redact_assignment @ agent/redact.py:_should_redact_assignment */
json_t *rf_should_redact_assignment(rf_io_t *io, const char *key, const char *value, bool check_keyword) {
    json_t *r;
    const char *k = key ? key : "";
    const char *v = value ? value : "";
    r = rf_seam(io, "env_lookup_match", rf_args1("value", v));
    if (rf_raised(r)) return r;
    bool m = rf_truthy(r); json_free(r);
    if (m) return json_bool(false);
    if (strcmp(v, "***") == 0 || rf_startswith(v, "\xc2\xab" "redacted")) return json_bool(false);
    char *vl = rf_lower(v);
    bool lit = rf_in_set(RF_CONFIG_LITERAL_VALUES, vl);
    free(vl);
    if (lit) return json_bool(false);
    if (check_keyword) {
        r = rf_seam(io, "key_has_secret_keyword", rf_args1("key", k));
        if (rf_raised(r)) return r;
        bool kw = rf_truthy(r); json_free(r);
        if (!kw) return json_bool(false);
    }
    /* password_key = hwb_password or has_password_keyword: Python `or` keeps the
     * RAW operand (a scripted None stays None), so carry the json value, not a bool. */
    json_t *password_key = rf_seam(io, "has_word_bounded_keyword_password", rf_args1("key", k));
    if (rf_raised(password_key)) return password_key;
    if (!rf_truthy(password_key)) {
        json_free(password_key);
        password_key = rf_seam(io, "has_password_keyword", rf_args1("key", k));
        if (rf_raised(password_key)) return password_key;
    }
    r = rf_seam(io, "path_or_var_match", rf_args1("value", v));
    if (rf_raised(r)) { json_free(password_key); return r; }
    bool pv = rf_truthy(r); json_free(r);
    if (pv && !rf_truthy(password_key)) {
        if (v[0] == '$') { json_free(password_key); return json_bool(false); }
        bool any_opaque = false;
        /* any(fullmatch(seg) for seg in value.split("/")): short-circuit on True */
        size_t n = strlen(v), start = 0;
        for (;;) {
            size_t end = start;
            while (end < n && v[end] != '/') end++;
            char *seg = rf_substr(v, start, end - start);
            r = rf_seam(io, "opaque_path_segment_fullmatch", rf_args1("seg", seg ? seg : ""));
            free(seg);
            if (rf_raised(r)) { json_free(password_key); return r; }
            bool sm = rf_truthy(r); json_free(r);
            if (sm) { any_opaque = true; break; }
            if (end >= n) break;
            start = end + 1;
        }
        if (!any_opaque) { json_free(password_key); return json_bool(false); }
    }
    /* return (password_key or strong or glued or (qual and not ph) or opaque):
     * each truthy operand is returned RAW; `not ph` is a real bool; the final
     * operand is returned as-is (None included) — no bool coercion. */
    if (rf_truthy(password_key)) return password_key;
    json_free(password_key);
    r = rf_seam(io, "has_word_bounded_keyword_strong", rf_args1("key", k));
    if (rf_raised(r)) return r;
    if (rf_truthy(r)) return r;
    json_free(r);
    r = rf_seam(io, "is_glued_credential_env", rf_args1("key", k));
    if (rf_raised(r)) return r;
    if (rf_truthy(r)) return r;
    json_free(r);
    r = rf_seam(io, "is_qualified_key_name", rf_args1("key", k));
    if (rf_raised(r)) return r;
    bool qual = rf_truthy(r); json_free(r);
    if (qual) {
        r = rf_seam(io, "config_placeholder_fullmatch", rf_args1("value", v));
        if (rf_raised(r)) return r;
        bool ph = rf_truthy(r); json_free(r);
        if (!ph) return json_bool(true);
    }
    r = rf_seam(io, "looks_like_opaque_credential", rf_args1("value", v));
    if (rf_raised(r)) return r;
    return r;
}

/* PoP: rf_json_affixed_key_secret @ agent/redact.py:_json_affixed_key_secret */
json_t *rf_json_affixed_key_secret(rf_io_t *io, const char *key, const char *value) {
    json_t *r;
    const char *k = key ? key : "";
    const char *v = value ? value : "";
    r = rf_seam(io, "json_key_names_full_fullmatch", rf_args1("key", k));
    if (rf_raised(r)) return r;
    bool closed = rf_truthy(r); json_free(r);
    if (closed) return json_bool(false);
    r = rf_seam(io, "has_word_bounded_keyword_yaml", rf_args1("key", k));
    if (rf_raised(r)) return r;
    bool y = rf_truthy(r); json_free(r);
    if (!y) {
        r = rf_seam(io, "has_password_keyword", rf_args1("key", k));
        if (rf_raised(r)) return r;
        y = rf_truthy(r); json_free(r);
    }
    if (!y) return json_bool(false);
    r = rf_seam(io, "dsn_pw_placeholder_fullmatch", rf_args1("value", v));
    if (rf_raised(r)) return r;
    bool dsn = rf_truthy(r); json_free(r);
    if (dsn) return json_bool(false);
    json_t *a = json_object();
    json_set(a, "key", json_string(k));
    json_set(a, "value", json_string(v));
    json_set(a, "check_keyword", json_bool(true));
    r = rf_seam(io, "should_redact_assignment", a);
    if (rf_raised(r)) return r;
    bool sra = rf_truthy(r); json_free(r);
    if (!sra) return json_bool(false);
    r = rf_seam(io, "json_value_suffix_sub", rf_args1("key", k));
    if (rf_raised(r)) return r;
    const char *stripped = (r->type == JSON_STRING && r->str_val) ? r->str_val : "";
    json_t *a2 = rf_args1("key", stripped);
    json_free(r);
    r = rf_seam(io, "is_python_repr_secret_key", a2);
    if (rf_raised(r)) return r;
    /* return _is_python_repr_secret_key(sub): RAW operand (None included). */
    return r;
}

/* PoP: rf_is_sensitive_url_param @ agent/redact.py:_is_sensitive_url_param */
json_t *rf_is_sensitive_url_param(rf_io_t *io, const char *name) {
    json_t *r;
    const char *nm = name ? name : "";
    r = rf_seam(io, "canonical_url_param_name", rf_args1("name", nm));
    if (rf_raised(r)) return r;
    const char *canon = (r->type == JSON_STRING && r->str_val) ? r->str_val : "";
    bool in_set = rf_in_set(RF_SENSITIVE_QUERY_PARAMS, canon);
    json_free(r);
    if (in_set) return json_bool(true);
    r = rf_seam(io, "decode_url_param_name", rf_args1("name", nm));
    if (rf_raised(r)) return r;
    /* decoded = _decode_url_param_name(name).replace("-", "_"): a non-str seam
     * result (unscripted seam -> None) raises AttributeError in live; do NOT
     * swallow it to "". */
    if (r->type != JSON_STRING) {
        json_t *e = rf_attr_error(r, "replace");
        json_free(r);
        return e;
    }
    char *decoded = rf_replace_char(r->str_val ? r->str_val : "", '-', '_');
    json_free(r);
    r = rf_seam(io, "json_key_names_full_fullmatch", rf_args1("key", decoded));
    if (rf_raised(r)) { free(decoded); return r; }
    bool full = rf_truthy(r); json_free(r);
    if (full) { free(decoded); return json_bool(true); }
    r = rf_seam(io, "json_value_suffix_sub", rf_args1("key", decoded));
    if (rf_raised(r)) { free(decoded); return r; }
    const char *stripped = (r->type == JSON_STRING && r->str_val) ? r->str_val : "";
    json_t *a2 = rf_args1("key", stripped);
    json_free(r);
    free(decoded);
    r = rf_seam(io, "is_python_repr_secret_key", a2);
    if (rf_raised(r)) return r;
    /* return (bool(fullmatch) or _is_python_repr_secret_key(sub)): the final
     * operand is returned RAW (a scripted None stays None), not coerced. */
    return r;
}

/* PoP: rf_redact_log_urls @ agent/redact.py:redact_log_urls */
json_t *rf_redact_log_urls(rf_io_t *io, const json_t *text) {
    /* if not text or "://" not in text or not _redact_enabled(): return text.
     * A falsy argument (None/""/0/false/[]) is returned RAW — live hands the
     * input back unchanged, so an int 0 must not be flattened to "". */
    if (!rf_truthy(text)) return text ? json_copy(text) : json_null();
    int has = rf_contains(text, "://");
    if (has < 0) return rf_type_error_not_iterable(text);  /* `"://" in <scalar>` */
    if (!has) return json_copy(text);
    json_t *r = rf_seam(io, "redact_enabled", json_object());
    if (rf_raised(r)) return r;
    bool en = rf_truthy(r); json_free(r);
    if (!en) return json_copy(text);
    json_t *a = json_object();
    json_set(a, "text", json_copy(text));
    r = rf_seam(io, "redact_strict_url_credentials", a);
    if (rf_raised(r)) return r;
    return r;
}

/* PoP: rf_redact_for_egress @ agent/redact.py:redact_for_egress */
json_t *rf_redact_for_egress(rf_io_t *io, const char *text, bool urls, bool keep_phone_numbers) {
    const char *t = (text && text[0]) ? text : "";
    json_t *a = json_object();
    json_set(a, "text", json_string(t));
    json_set(a, "force", json_bool(true));
    json_set(a, "redact_url_credentials", json_bool(urls));
    json_set(a, "keep_phone_numbers", json_bool(keep_phone_numbers));
    json_t *r = rf_seam(io, "redact_sensitive_text", a);
    if (rf_raised(r)) {
        if (rf_is_base_exc(r)) return r;
        json_free(r);
        return json_string(RF_REDACTION_UNAVAILABLE);
    }
    /* if "earer" in text.lower(): live calls .lower() OUTSIDE the try, so a
     * non-str seam result raises AttributeError (None -> 'NoneType' ...). */
    if (r->type != JSON_STRING) {
        json_t *e = rf_attr_error(r, "lower");
        json_free(r);
        return e;
    }
    char *redacted = strdup(r->str_val ? r->str_val : "");
    json_free(r);
    char *low = rf_lower(redacted);
    bool gate = strstr(low, "earer") != NULL;
    free(low);
    if (gate) {
        r = rf_seam(io, "bearer_residue_sub", rf_args1("text", redacted));
        free(redacted);
        if (rf_raised(r)) return r;
        return r;
    }
    json_t *ret = json_string(redacted);
    free(redacted);
    return ret;
}

/* PoP: rf_redact_code_output @ agent/redact.py:redact_code_output */
json_t *rf_redact_code_output(rf_io_t *io, const char *output, bool force) {
    json_t *a = json_object();
    json_set(a, "text", json_string(output ? output : ""));
    json_set(a, "force", json_bool(force));
    json_set(a, "code_file", json_bool(true));
    json_t *r = rf_seam(io, "redact_sensitive_text", a);
    if (rf_raised(r)) return r;
    /* if not redacted or "=" not in redacted or not (force or _redact_enabled()):
     * return redacted — RAW (a scripted None/0/[] is handed back unchanged). */
    if (!rf_truthy(r)) return r;
    int has_eq = rf_contains(r, "=");
    if (has_eq < 0) { json_t *e = rf_type_error_not_iterable(r); json_free(r); return e; }
    if (!has_eq) return r;
    bool en;
    if (force) {
        en = true;
    } else {
        json_t *r2 = rf_seam(io, "redact_enabled", json_object());
        if (rf_raised(r2)) { json_free(r); return r2; }
        en = rf_truthy(r2); json_free(r2);
    }
    if (!en) return r;
    json_t *a2 = json_object();
    json_set(a2, "text", json_copy(r));
    json_free(r);
    r = rf_seam(io, "env_dump_line_sub", a2);
    if (rf_raised(r)) return r;
    return r;
}

/* PoP: rf_redact_derived_text @ agent/redact.py:redact_derived_text */
json_t *rf_redact_derived_text(rf_io_t *io, const json_t *text) {
    char *t = rf_is_none(text) ? strdup("") : rf_pystr(text);
    json_t *a = json_object();
    json_set(a, "text", json_string(t ? t : ""));
    json_set(a, "force", json_bool(true));
    json_set(a, "redact_url_credentials", json_bool(true));
    free(t);
    json_t *r = rf_seam(io, "redact_sensitive_text", a);
    if (rf_raised(r)) return r;
    return r;
}

/* PoP: rf_redact_enabled @ agent/redact.py:_redact_enabled */
json_t *rf_redact_enabled(rf_io_t *io) {
    json_t *r = rf_seam(io, "get_hermes_home_override", json_object());
    if (rf_raised(r)) return r;
    bool override_none = rf_is_none(r);
    json_free(r);
    if (override_none) {
        return json_bool(rf_get_bool(io->state, "_REDACT_ENABLED", true));
    }
    r = rf_seam(io, "hermes_home_key", json_object());
    if (rf_raised(r)) return r;
    char *home_key = strdup((r->type == JSON_STRING && r->str_val) ? r->str_val : "");
    json_free(r);
    r = rf_seam(io, "current_secret_scope", json_object());
    if (rf_raised(r)) { free(home_key); return r; }
    bool scope_is_none = rf_is_none(r);
    bool scope_truthy = rf_truthy(r);
    json_t *raw = NULL;
    if (scope_truthy) {
        json_t *v = rf_get(r, "HERMES_REDACT_SECRETS");
        raw = v ? json_copy(v) : NULL;
    }
    json_free(r);
    bool raw_is_none = rf_is_none(raw);
    if (raw_is_none && scope_is_none) {
        json_t *envd = rf_get(io->state, "_ENV_POLICY_BY_HOME");
        bool has = envd && envd->type == JSON_OBJECT && json_obj_get(envd, home_key) != NULL;
        if (!has) { json_free(raw); free(home_key); return json_bool(true); }
        json_free(raw);
        json_t *v = json_obj_get(envd, home_key);
        raw = v ? json_copy(v) : NULL;
        raw_is_none = rf_is_none(raw);
    }
    if (raw_is_none) {
        json_free(raw);
        json_t *cfgd = rf_get(io->state, "_CONFIG_POLICY_BY_HOME");
        json_t *v = (cfgd && cfgd->type == JSON_OBJECT) ? json_obj_get(cfgd, home_key) : NULL;
        raw = v ? json_copy(v) : NULL;
        raw_is_none = rf_is_none(raw);
    }
    if (raw_is_none) { json_free(raw); free(home_key); return json_bool(true); }
    char *s = rf_pystr(raw);
    json_free(raw);
    char *st = rf_strip(s); free(s);
    char *lo = rf_lower(st); free(st);
    bool res = rf_in_set(RF_ENABLED_TRUE_WORDS, lo);
    free(lo); free(home_key);
    return json_bool(res);
}

/* PoP: rf_command_reads_secret_file @ agent/redact.py:_command_reads_secret_file */
json_t *rf_command_reads_secret_file(rf_io_t *io, const char *command) {
    if (!command || command[0] == '\0') return json_bool(false);
    json_t *segs = rf_seam(io, "command_segments", rf_args1("command", command));
    if (rf_raised(segs)) return segs;
    if (!segs || segs->type != JSON_ARRAY) { json_free(segs); return json_bool(false); }
    bool result = false;
    for (size_t si = 0; si < json_len(segs); si++) {
        json_t *segn = json_get(segs, si);
        const char *seg = (segn && segn->type == JSON_STRING) ? segn->str_val : "";
        json_t *tokens = rf_split_ws(seg);
        size_t nt = json_len(tokens);
        if (nt == 0) { json_free(tokens); continue; }
        json_t *t0 = json_get(tokens, 0);
        char *reader = rf_basename_lower((t0 && t0->type == JSON_STRING) ? t0->str_val : "");
        if (!rf_in_set(RF_FILE_READ_COMMANDS, reader)) { free(reader); json_free(tokens); continue; }
        bool pattern_first = rf_in_set(RF_PATTERN_FIRST_COMMANDS, reader);
        json_t *positional = json_array();
        json_t *raised = NULL;
        for (size_t ti = 1; ti < nt; ti++) {
            json_t *an = json_get(tokens, ti);
            const char *arg = (an && an->type == JSON_STRING) ? an->str_val : "";
            if (arg[0] != '-') { json_append(positional, json_string(arg)); continue; }
            if (pattern_first) continue;
            json_t *r2 = rf_seam(io, "ps_colon_param_fullmatch", rf_args1("arg", arg));
            if (rf_raised(r2)) { raised = r2; break; }
            if (rf_is_none(r2)) { json_free(r2); continue; }
            json_append(positional, json_copy(r2));
            json_free(r2);
        }
        if (raised) { json_free(positional); free(reader); json_free(tokens); json_free(segs); return raised; }
        if (pattern_first) {
            json_t *shifted = json_array();
            for (size_t pi = 1; pi < json_len(positional); pi++)
                json_append(shifted, json_copy(json_get(positional, pi)));
            json_free(positional);
            positional = shifted;
        }
        bool found = false;
        for (size_t pi = 0; pi < json_len(positional); pi++) {
            json_t *pn = json_get(positional, pi);
            const char *arg = (pn && pn->type == JSON_STRING) ? pn->str_val : "";
            json_t *r2 = rf_seam(io, "is_secret_file_arg", rf_args1("arg", arg));
            if (rf_raised(r2)) { raised = r2; break; }
            bool b = rf_truthy(r2); json_free(r2);
            if (b) { found = true; break; }
        }
        json_free(positional); free(reader); json_free(tokens);
        if (raised) { json_free(segs); return raised; }
        if (found) { result = true; break; }
    }
    json_free(segs);
    return json_bool(result);
}

/* PoP: rf_is_secret_file_arg @ agent/redact.py:_is_secret_file_arg */
json_t *rf_is_secret_file_arg(rf_io_t *io, const char *arg) {
    const char *a = arg ? arg : "";
    char *q = rf_strip_quotes(a);
    char *path = rf_replace_char(q, '\\', '/');
    free(q);
    bool hermes_home = false;
    for (size_t i = 0; RF_HERMES_HOME_PREFIXES[i]; i++) {
        if (rf_startswith(path, RF_HERMES_HOME_PREFIXES[i])) {
            char *np = strdup(path + strlen(RF_HERMES_HOME_PREFIXES[i]));
            free(path); path = np; hermes_home = true; break;
        }
    }
    for (size_t i = 0; RF_HOME_PREFIXES[i]; i++) {
        if (rf_startswith(path, RF_HOME_PREFIXES[i])) {
            char *np = strdup(path + strlen(RF_HOME_PREFIXES[i]));
            free(path); path = np; break;
        }
    }
    if (strchr(path, '$')) { free(path); return json_bool(false); }
    json_t *parts = rf_split_slash_lower_nonempty(path);
    size_t np = json_len(parts);
    if (np == 0) { json_free(parts); free(path); return json_bool(false); }
    json_t *lastn = json_get(parts, np - 1);
    const char *last = (lastn && lastn->type == JSON_STRING) ? lastn->str_val : "";
    if (rf_in_set(RF_ENV_FILE_BASENAMES, last) || rf_in_set(RF_SHELL_RC_BASENAMES, last)) {
        json_free(parts); free(path); return json_bool(true);
    }
    bool home_cred = rf_in_set(RF_HOME_CREDENTIAL_FILES, last);
    bool aws = false;
    if (np >= 2) {
        json_t *p2n = json_get(parts, np - 2);
        const char *p2 = (p2n && p2n->type == JSON_STRING) ? p2n->str_val : "";
        if (strcmp(p2, ".aws") == 0 && (strcmp(last, "credentials") == 0 || strcmp(last, "config") == 0))
            aws = true;
    }
    if (home_cred || aws) { json_free(parts); free(path); return json_bool(true); }
    if (strcmp(last, "config.yaml") != 0 && !rf_startswith(last, "config.yaml.good.")
        && !rf_startswith(last, "config.yaml.corrupt.")) {
        json_free(parts); free(path); return json_bool(false);
    }
    if (hermes_home) { json_free(parts); free(path); return json_bool(true); }
    bool hermes_seg = false;
    for (size_t i = 0; i + 1 < np; i++) {
        json_t *pn = json_get(parts, i);
        const char *ps = (pn && pn->type == JSON_STRING) ? pn->str_val : "";
        if (strcmp(ps, ".hermes") == 0) { hermes_seg = true; break; }
    }
    if (hermes_seg) { json_free(parts); free(path); return json_bool(true); }
    json_t *r = rf_seam(io, "is_under_hermes_home", rf_args1("path", path));
    json_free(parts); free(path);
    if (rf_raised(r)) return r;
    bool res = rf_truthy(r); json_free(r);
    return json_bool(res);
}

#endif /* REDACT_FLOW_C */
