/*
 * plugins_session_env.c -- faithful C11 port of the deterministic name
 * handling in hermes_cli/plugins_session_env.py (wave18 gate w18_pluginsses_3f39fa).
 *
 * PoP: w18_pluginsses_3f39fa_env_key @ hermes_cli/plugins_session_env.py:_env_key
 * PoP: w18_pluginsses_3f39fa_name_valid @ hermes_cli/plugins_session_env.py:register_session_env
 *
 * The registry itself (lock, resolvers, scopes) stays in Python. Only the pure
 * key computation and name validation are ported.
 */
#include "noc_w18_pluginsses_3f39fa.h"

#include <stdlib.h>
#include <string.h>

char *w18_pluginsses_3f39fa_env_key(const char *name)
{
    const char *s = name ? name : "";
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 32) : (char)c;
    }
    out[n] = '\0';                                  /* NUL at new length */
    return out;
}

static int is_alnum_(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
        || (c >= '0' && c <= '9') || c == '_';
}

bool w18_pluginsses_3f39fa_name_valid(json_t *name)
{
    if (!name || !json_is_string(name) || !name->str_val) return false;
    const char *s = name->str_val;
    size_t n = strlen(s);
    if (n < 1 || n > 128) return false;             /* [A-Za-z_][A-Za-z0-9_]{0,127} */
    unsigned char c0 = (unsigned char)s[0];
    if (!((c0 >= 'A' && c0 <= 'Z') || (c0 >= 'a' && c0 <= 'z') || c0 == '_'))
        return false;
    for (size_t i = 1; i < n; i++)
        if (!is_alnum_((unsigned char)s[i])) return false;
    if (n >= 7) {                                   /* not name.upper().startswith("HERMES_") */
        static const char pfx[7] = {'H','E','R','M','E','S','_'};
        int same = 1;
        for (size_t i = 0; i < 7; i++) {
            unsigned char a = (unsigned char)s[i];
            if (a >= 'a' && a <= 'z') a = (unsigned char)(a - 32);
            if (a != (unsigned char)pfx[i]) { same = 0; break; }
        }
        if (same) return false;
    }
    return true;
}
