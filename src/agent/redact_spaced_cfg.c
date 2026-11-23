/*
 * redact_spaced_cfg.c - real C11 port of upstream Hermes agent/redact_spaced_cfg.py.
 *
 * PoP: w9_redactspace_453fc1_key_names_secret  @ agent/redact_spaced_cfg.py:_key_names_secret
 * PoP: w9_redactspace_453fc1_is_code_expression @ agent/redact_spaced_cfg.py:_is_code_expression
 *
 * Faithful ports of the module's two pure decision predicates, including the
 * case-insensitive secret-keyword regex (api[ ._-]?key|token|secret|passwd|
 * password|credential|auth), the opaque-credential shape test, and the
 * identifier-path full match.  The full regex substitution pass is out of
 * scope for this gate (see notes).
 */
#include "hermes_json.h"
#include "noc_w9_redactspace_453fc1.h"
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

/* Case-insensitive, end-bounded literal match at s[pos]. */
static int ci_at(const char *s, size_t pos, size_t end, const char *lit) {
    size_t l = strlen(lit);
    if (pos + l > end) return 0;
    for (size_t i = 0; i < l; i++)
        if (tolower((unsigned char)s[pos + i]) != tolower((unsigned char)lit[i])) return 0;
    return 1;
}

static long ci_find(const char *s, size_t start, size_t end, const char *lit) {
    size_t l = strlen(lit);
    if (!l) return -1;
    for (size_t i = start; i + l <= end; i++)
        if (ci_at(s, i, end, lit)) return (long)i;
    return -1;
}

/* Equivalent of _CFG_SECRET_WORD_RE.search(s[start:end]). */
static long secret_word_find(const char *s, size_t start, size_t end) {
    static const char *const words[] = {
        "token", "secret", "passwd", "password", "credential", "auth", NULL
    };
    long best = -1;
    for (int w = 0; words[w]; w++) {
        long p = ci_find(s, start, end, words[w]);
        if (p >= 0 && (best < 0 || p < best)) best = p;
    }
    for (size_t i = start; i + 3 <= end; i++) {
        if (!ci_at(s, i, end, "api")) continue;
        size_t j = i + 3;
        if (j < end && (s[j] == ' ' || s[j] == '.' || s[j] == '_' || s[j] == '-')) j++;
        if (j + 3 <= end && ci_at(s, j, end, "key")) {
            if (best < 0 || (long)i < best) best = (long)i;
            break;
        }
    }
    return best;
}

static int has_secret_range(const char *s, size_t start, size_t end) {
    return secret_word_find(s, start, end) >= 0;
}

bool w9_redactspace_453fc1_key_names_secret(const char *name, bool dotted) {
    if (!name) return false;
    size_t len = strlen(name);
    if (!dotted) {
        size_t first = len;
        int count = 0;
        for (size_t i = 0; i < len; i++)
            if (name[i] == '.' || name[i] == ' ') { count++; if (first == len) first = i; }
        int ok = 1;
        if (count > 0) {
            if (count > 1) {
                ok = 0;
            } else {
                size_t a = first >= 3 ? first - 3 : 0;
                if (first - a != 3 || !ci_at(name, a, first, "api")) ok = 0;
                if (first + 4 > len || !ci_at(name, first + 1, first + 4, "key")) ok = 0;
            }
        }
        return ok && has_secret_range(name, 0, len);
    }
    long first_dot = -1, last_dot = -1;
    for (size_t i = 0; i < len; i++) {
        if (name[i] == '.') {
            if (first_dot < 0) first_dot = (long)i;
            if (i + 1 < len) last_dot = (long)i;   /* rfind(".", 0, len-1) */
        }
    }
    return (first_dot > 0 && has_secret_range(name, (size_t)first_dot + 1, len)) ||
           (last_dot > 0 && has_secret_range(name, 0, (size_t)last_dot));
}

static bool ci_equal(const char *a, const char *b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return false;
        a++; b++;
    }
    return *a == '\0' && *b == '\0';
}

static bool is_code_keyword(const char *v) {
    static const char *const kws[] = { "none", "null", "true", "false", "nil", "undefined", NULL };
    for (int i = 0; kws[i]; i++) if (ci_equal(v, kws[i])) return true;
    return false;
}

/* re.split(r"[^\w$-]", key)[-1] */
static const char *last_key_token(const char *key) {
    if (!key) return "";
    const char *last = "";
    const char *start = key;
    for (const char *p = key; ; p++) {
        unsigned char c = (unsigned char)*p;
        int tok = isalnum(c) || c == '_' || c == '$' || c == '-';
        if (!tok || c == '\0') {
            size_t n = (size_t)(p - start);
            /* last becomes the (possibly empty) token just consumed */
            if (n == 0) {
                last = "";
            } else {
                static char slot[512];
                size_t m = n < sizeof(slot) - 1 ? n : sizeof(slot) - 1;
                memcpy(slot, start, m); slot[m] = '\0'; last = slot;
            }
            if (c == '\0') break;
            start = p + 1;
        }
    }
    return last;
}

static bool is_hex_digit(unsigned char c) {
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

/* Faithful port of agent.redact._looks_like_opaque_credential (ASCII domain). */
static bool is_opaque(const char *v) {
    if (!v) return false;
    if (!strcmp(v, "***")) return true;
    size_t n = strlen(v);
    if (n >= 16) {
        int all = 1;
        for (size_t i = 0; i < n; i++) if (!is_hex_digit((unsigned char)v[i])) { all = 0; break; }
        if (all) return true;
    }
    if (n >= 20) {
        int all = 1;
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)v[i];
            if (!(isalnum(c) || c == '_' || c == '.' || c == '/' || c == '+' || c == '=' || c == '-')) {
                all = 0; break;
            }
        }
        if (all) return true;
    }
    if (n < 12) return false;
    int lo = 0, up = 0, dig = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)v[i];
        if (islower(c)) lo = 1;
        if (isupper(c)) up = 1;
        if (isdigit(c)) dig = 1;
    }
    return (lo + up + dig) >= 2;
}

static bool id_start(unsigned char c) { return isalpha(c) || c == '_' || c == '$'; }
static bool id_cont(unsigned char c)  { return isalnum(c) || c == '_' || c == '$'; }

/* _IDENTIFIER_PATH_RE.fullmatch: [A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)* */
static bool identifier_path(const char *v) {
    if (!v || !*v) return false;
    size_t n = strlen(v), i = 0;
    while (i < n) {
        if (!id_start((unsigned char)v[i])) return false;
        i++;
        while (i < n && id_cont((unsigned char)v[i])) i++;
        if (i < n) {
            if (v[i] != '.') return false;
            i++;
            if (i >= n) return false;
        }
    }
    return true;
}

bool w9_redactspace_453fc1_is_code_expression(const char *key, const char *value) {
    if (!value) value = "";
    if (strpbrk(value, "([{")) return true;
    size_t n = strlen(value);
    if (n > 0 && value[n - 1] == ';') return true;
    if (is_code_keyword(value)) return true;
    if (ci_equal(value, last_key_token(key))) return true;
    if (!is_opaque(value)) return true;
    int has_dot = strchr(value, '.') != NULL;
    int has_digit = 0;
    for (size_t i = 0; i < n; i++) if (isdigit((unsigned char)value[i])) { has_digit = 1; break; }
    return identifier_path(value) && (has_dot || !has_digit);
}
