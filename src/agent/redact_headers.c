/*
 * redact_headers.c — C11 port of the pure credential-predicate core of
 * upstream Hermes agent/redact_headers.py.
 *
 * PoP: w7_redactheade_29d7db_cookie_value_credential @ agent/redact_headers.py:cookie_value_is_credential
 * PoP: w7_redactheade_29d7db_json_header_credential @ agent/redact_headers.py:json_header_value_is_credential
 *
 * The regex substitution passes in the Python module are display redaction;
 * the two decision predicates below are the testable semantic core and are
 * mirrored exactly, including the agent.redact `_should_redact_assignment`
 * fallback for the two keys this module uses ("cookie", "session_token").
 */
#include "noc_w7_redactheade_29d7db.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static bool starts_with_ci(const char *s, const char *p) {
    if (!s || !p) return false;
    while (*p) {
        if (!*s) return false;
        if (tolower((unsigned char)*s) != tolower((unsigned char)*p)) return false;
        s++; p++;
    }
    return true;
}

/* value == "***" or value.startswith("«redacted") */
static bool already_masked(const char *v) {
    return v && (!strcmp(v, "***") || !strncmp(v, "\xc2\xab" "redacted", 10));
}

/* _ENV_LOOKUP_VALUE_RE.match: ^(?:os\.(?:getenv|environ)|process\.env|\$ENV\{) */
static bool env_lookup(const char *v) {
    return v && (starts_with_ci(v, "os.getenv") || starts_with_ci(v, "os.environ") ||
                 starts_with_ci(v, "process.env") || !strncmp(v, "$ENV{", 5));
}

/* value.lower() in _CONFIG_LITERAL_VALUES */
static bool config_literal(const char *v) {
    static const char *const vals[] = {"true","false","yes","no","on","off","null","none",NULL};
    if (!v) return false;
    for (int i = 0; vals[i]; i++) {
        size_t n = strlen(vals[i]);
        if (strlen(v) == n && !strncasecmp(v, vals[i], n)) return true;
    }
    return false;
}

/* _looks_like_opaque_credential(value) */
static bool looks_opaque(const char *v) {
    if (!v) return false;
    if (!strcmp(v, "***") || !strncmp(v, "\xc2\xab" "redacted:", 11)) return true;
    size_t n = strlen(v);
    if (n >= 16) {
        bool hex = true;
        for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)v[i])) { hex = false; break; }
        if (hex) return true;
    }
    if (n >= 20) {
        bool b64ish = true;
        for (size_t i = 0; i < n; i++) {
            unsigned char c = (unsigned char)v[i];
            if (!(isalnum(c) || c=='_'||c=='.'||c=='/'||c=='+'||c=='='||c=='-')) { b64ish = false; break; }
        }
        if (b64ish) return true;
    }
    if (n < 12) return false;
    int lower=0, upper=0, digit=0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)v[i];
        if (islower(c)) lower=1;
        if (isupper(c)) upper=1;
        if (isdigit(c)) digit=1;
    }
    return (lower + upper + digit) >= 2;
}

/* _SHELL_VAR_REF at a position: \$(?:\{[A-Za-z_]\w*[^}]*\}|[A-Za-z_]\w*) */
static bool shell_var_at(const char *p, const char **end_out) {
    if (!p || *p != '$') return false;
    const char *q = p + 1;
    if (*q == '{') {
        q++;
        if (!(*q == '_' || isalpha((unsigned char)*q))) return false;
        while (*q == '_' || isalnum((unsigned char)*q)) q++;
        const char *close = strchr(q, '}');
        if (!close) return false;
        q = close + 1;
    } else {
        if (!(*q == '_' || isalpha((unsigned char)*q))) return false;
        while (*q == '_' || isalnum((unsigned char)*q)) q++;
    }
    if (end_out) *end_out = q;
    return true;
}

/* _PATH_OR_VAR_VALUE_RE.match:
 * ^(?:\$(?:...)|$\(|~|/)(?:[\w./:-]|SHELL_VAR_REF)*$ */
static bool path_or_var(const char *v) {
    if (!v || !*v) return false;
    const char *p = v;
    if (*p == '$') {
        const char *e = NULL;
        if (!shell_var_at(p, &e)) {
            if (p[1] != '(') return false;
            p += 2;
        } else {
            p = e;
        }
    } else if (*p == '~' || *p == '/') {
        p++;
    } else {
        return false;
    }
    while (*p) {
        if (*p == '_' || isalnum((unsigned char)*p) || *p=='.' || *p=='/' || *p==':' || *p=='-') { p++; continue; }
        const char *e = NULL;
        if (shell_var_at(p, &e) && e > p) { p = e; continue; }
        return false;
    }
    return true;
}

/* _should_redact_assignment for the fixed keys "cookie"/"session_token",
 * check_keyword=False: env-lookup/marker/config-literal/variable exemptions,
 * qualified key masks any literal, otherwise the value shape decides. */
static bool should_redact(const char *key, const char *value) {
    if (!value) return false;
    if (env_lookup(value)) return false;
    if (already_masked(value)) return false;
    if (config_literal(value)) return false;
    if (path_or_var(value)) return false;  /* not a password key: reference/config */
    bool qualified = (key && strstr(key, "session_token"));
    if (qualified) return true; /* _is_qualified_key_name, no config placeholder here */
    return looks_opaque(value);
}

/* _SHELL_VAR_REF fullmatch */
static bool shell_var_fullmatch(const char *v) {
    const char *end = NULL;
    return shell_var_at(v, &end) && end && *end == '\0';
}

bool w7_redactheade_29d7db_cookie_value_credential(const char *value, bool qualified) {
    if (!value) value = "";
    if (!strncmp(value, "\xc2\xab" "redacted", 10)) return false;
    /* ("=" in value and not value.startswith("$")) or _should_redact_assignment(...) */
    if (strchr(value, '=') && value[0] != '$') return true;
    return should_redact(qualified ? "session_token" : "cookie", value);
}

bool w7_redactheade_29d7db_json_header_credential(const char *key, const char *value) {
    if (!value) value = "";
    /* Python: "cookie" in key.lower() — IGNORECASE, so "Cookie" / "Set-Cookie" match. */
    char lower_key[256];
    size_t klen = key ? strlen(key) : 0;
    if (klen >= sizeof(lower_key)) klen = sizeof(lower_key) - 1;
    for (size_t i = 0; i < klen; i++)
        lower_key[i] = (char)tolower((unsigned char)(key ? key[i] : 0));
    lower_key[klen] = '\0';
    if (strstr(lower_key, "cookie"))
        return w7_redactheade_29d7db_cookie_value_credential(value, false);
    /* not(value == "***" or startswith("«redacted") or ENV_LOOKUP.match or SHELL_VAR fullmatch) */
    if (!strcmp(value, "***") || !strncmp(value, "\xc2\xab" "redacted", 10)) return false;
    if (env_lookup(value)) return false;
    if (shell_var_fullmatch(value)) return false;
    return true;
}
