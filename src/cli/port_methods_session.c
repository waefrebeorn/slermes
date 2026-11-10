/*
 * port_methods_session.c
 *
 * Pure helpers ported from tui_gateway/methods_session.py (117 defs, previously
 * 0 ported).
 *
 *   ms_str_param              <- _str_param
 *   ms_flag                   <- _flag
 *   ms_int_param              <- _int_param
 *   ms_is_truthy              <- utils.py:is_truthy_value
 *   ms_denied_source          <- _denied_source
 *   ms_auto_resume_denied     <- _auto_resume_denied_source
 *
 * The RPC dispatch / db / session-registry half stays in Python.
 */

#include "port_methods_session.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* utils.py:TRUTHY_STRINGS */
static const char *const MS_TRUTHY_STRINGS[] = { "1", "true", "yes", "on" };
#define MS_TRUTHY_N (sizeof(MS_TRUTHY_STRINGS) / sizeof(MS_TRUTHY_STRINGS[0]))

/* hermes_state_sessions.py:INTERNAL_LISTING_SOURCES */
static const char *const MS_INTERNAL_SOURCES[] = { "kanban", "tool", "oneshot" };
#define MS_INTERNAL_N (sizeof(MS_INTERNAL_SOURCES) / sizeof(MS_INTERNAL_SOURCES[0]))

/* ===========================================================================
 *  is_truthy_value  (utils.py:is_truthy_value)
 *
 *      if value is None: return default
 *      if isinstance(value, str): return value.strip().lower() in TRUTHY_STRINGS
 *      return bool(value)
 *
 *  `kind` carries the Python type: 0 = None, 2 = str, 3 = int/bool (use the
 *  numeric value's truthiness). Returns 1/0.
 * =========================================================================== */

/* PoP: is_truthy_value @ utils.py:is_truthy_value */
int ms_is_truthy(int kind, const char *sval, int ival, int dflt)
{
    if (kind == 0) return dflt;
    if (kind == 2) {
        if (!sval) return 0;
        char *low = (char *)malloc(strlen(sval) + 1);
        if (!low) return 0;
        size_t n = strlen(sval);
        for (size_t i = 0; i < n; i++) low[i] = (char)tolower((unsigned char)sval[i]);
        low[n] = '\0';
        size_t a = 0, b = n;
        while (a < b && isspace((unsigned char)low[a])) a++;
        while (b > a && isspace((unsigned char)low[b - 1])) low[--b] = '\0';
        int hit = 0;
        for (size_t i = 0; i < MS_TRUTHY_N; i++)
            if (strcmp(low + a, MS_TRUTHY_STRINGS[i]) == 0) { hit = 1; break; }
        free(low);
        return hit;
    }
    return ival != 0;
}

/* ===========================================================================
 *  _str_param  (methods_session.py:_str_param)
 *
 *      return str(params.get(key) or "").strip() or default
 *
 *  `str(value or "")` makes None / "" / 0 / false all become ""; then strip;
 *  then `or default` catches the empty result. Note this means a value of "0"
 * survives (it is a non-empty string) but 0 (int) does not.
 *  `params` is a flat [key, value, ...] array; `kind` is the Python type of the
 *  found value (0 = absent). Caller frees.
 * =========================================================================== */

/* PoP: _str_param @ tui_gateway/methods_session.py:_str_param */
char *ms_str_param(const char *const *params, const char *key, const char *dflt)
{
    const char *val = NULL;
    for (int i = 0; params && params[i]; i += 2)
        if (strcmp(params[i], key) == 0) { val = params[i + 1]; break; }

    /* value or "" - a NULL value is falsy */
    char *s;
    if (val && val[0]) {
        s = strdup(val);
    } else {
        s = (char *)malloc(1);
        if (s) s[0] = '\0';
    }
    if (!s) return NULL;

    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) s[--b] = '\0';
    memmove(s, s + a, b - a + 1);

    if (s[0] == '\0') {
        free(s);
        return strdup(dflt ? dflt : "");
    }
    return s;
}

/* ===========================================================================
 *  _flag  (methods_session.py:_flag)
 *
 *      return is_truthy_value(params.get(name, False))
 *
 *  A missing key reads as False, which is_truthy_value maps to False (not the
 *  default) - so a missing flag is off.
 * =========================================================================== */

/* PoP: _flag @ tui_gateway/methods_session.py:_flag */
int ms_flag(const char *const *params, const char *name)
{
    const char *val = NULL;
    for (int i = 0; params && params[i]; i += 2)
        if (strcmp(params[i], name) == 0) { val = params[i + 1]; break; }
    if (!val) return 0;                       /* .get(name, False) */
    return ms_is_truthy(2, val, 0, 0);
}

/* ===========================================================================
 *  _int_param  (methods_session.py:_int_param)
 *
 *      try: return int(params.get(key, default))
 *      except (TypeError, ValueError): return default
 *
 *  Python's int() accepts surrounding whitespace and a leading +/-. `params`
 *  is a flat [key, value, ...] array; the value is the raw string.
 * =========================================================================== */

/* PoP: _int_param @ tui_gateway/methods_session.py:_int_param */
int ms_int_param(const char *const *params, const char *key, int dflt)
{
    const char *val = NULL;
    for (int i = 0; params && params[i]; i += 2)
        if (strcmp(params[i], key) == 0) { val = params[i + 1]; break; }
    if (!val) return dflt;

    /* a strict int() parse: optional sign, then at least one digit, then nothing */
    const char *p = val;
    while (*p && isspace((unsigned char)*p)) p++;
    const char *start = p;
    if (*p == '+' || *p == '-') p++;
    if (!isdigit((unsigned char)*p)) return dflt;
    long long v = 0;
    while (isdigit((unsigned char)*p)) {
        v = v * 10 + (*p - '0');
        if (v > 2147483647LL) return dflt;   /* overflow: Python would widen, C cannot */
        p++;
    }
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p) return dflt;
    return (int)(*start == '-' ? -v : v);
}

/* ===========================================================================
 *  _denied_source  (methods_session.py:_denied_source)
 *
 *      return (row.get("source") or "").strip().lower() in _LISTING_DENY_SOURCES
 *
 *  `_LISTING_DENY_SOURCES` is `frozenset(INTERNAL_LISTING_SOURCES)` - the
 *  kanban / tool / oneshot sources are hidden from human listings. `row` is a
 *  flat [key, value, ...] array.
 * =========================================================================== */

/* PoP: _denied_source @ tui_gateway/methods_session.py:_denied_source */
int ms_denied_source(const char *const *row)
{
    const char *val = NULL;
    for (int i = 0; row && row[i]; i += 2)
        if (strcmp(row[i], "source") == 0) { val = row[i + 1]; break; }

    char *low;
    if (val && val[0]) {
        low = strdup(val);
    } else {
        low = (char *)malloc(1);
        if (low) low[0] = '\0';
    }
    if (!low) return 0;
    for (size_t i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    size_t n = strlen(low), a = 0, b = n;
    while (a < b && isspace((unsigned char)low[a])) a++;
    while (b > a && isspace((unsigned char)low[b - 1])) low[--b] = '\0';

    int hit = 0;
    for (size_t i = 0; i < MS_INTERNAL_N; i++)
        if (strcmp(low + a, MS_INTERNAL_SOURCES[i]) == 0) { hit = 1; break; }
    free(low);
    return hit;
}

/* ===========================================================================
 *  _auto_resume_denied_source  (methods_session.py:_auto_resume_denied_source)
 *
 *      source = (row.get("source") or "").strip().lower()
 *      return source in _LISTING_DENY_SOURCES or source == "unknown"
 *
 *  Stricter than _denied_source: the pick-a-session-for-me paths also skip
 *  source='unknown', which the token-accounting guard mints for legacy rows
 *  lacking a sessions row. Human-facing listings still show them.
 * =========================================================================== */

/* PoP: _auto_resume_denied_source @ tui_gateway/methods_session.py:_auto_resume_denied_source */
int ms_auto_resume_denied(const char *const *row)
{
    if (ms_denied_source(row)) return 1;
    const char *val = NULL;
    for (int i = 0; row && row[i]; i += 2)
        if (strcmp(row[i], "source") == 0) { val = row[i + 1]; break; }
    char *low;
    if (val && val[0]) low = strdup(val);
    else { low = (char *)malloc(1); if (low) low[0] = '\0'; }
    if (!low) return 0;
    for (size_t i = 0; low[i]; i++) low[i] = (char)tolower((unsigned char)low[i]);
    size_t n = strlen(low), a = 0, b = n;
    while (a < b && isspace((unsigned char)low[a])) a++;
    while (b > a && isspace((unsigned char)low[b - 1])) low[--b] = '\0';
    int hit = (strcmp(low + a, "unknown") == 0);
    free(low);
    return hit;
}
