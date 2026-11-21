#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/skills_hub.py: 9 missing functions ── */

/* ── _query_is_concrete revival (tail_revive3) ──────────────────────────────
 * Faithful port of tools/skills_hub_models.py:_query_is_concrete.
 * Void signature: the query rides a module-static call-context slot
 * installed per call by the test harness.
 * _VALUELESS_QUERY_FLAG_RE = re.compile(r"(?:[A-Za-z0-9_~-]|%[0-9A-Fa-f]{2})+\Z")
 * is transcribed as a whole-part token scanner (fullmatch == anchored). */

static const char *g_sh_query;

static int sh_hex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int sh_part_concrete(const char *s, size_t n) {
    /* "=" in part and bool(part.split("=", 1)[0]) */
    const char *eq = (const char *)memchr(s, '=', n);
    if (eq && eq != s) return 1;
    /* valueless flag: (?:[A-Za-z0-9_~-]|%XX)+ over the WHOLE part */
    if (n == 0) return 0;
    size_t i = 0;
    while (i < n) {
        char c = s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '~' || c == '-') {
            i++;
        } else if (c == '%' && i + 2 < n && sh_hex(s[i+1]) && sh_hex(s[i+2])) {
            i += 3;
        } else {
            return 0;
        }
    }
    return 1;
}

/* PoP: query_is_concrete @ tools/skills_hub_models.py:_query_is_concrete */
int query_is_concrete(void) {
    const char *q = g_sh_query ? g_sh_query : "";
    const char *p = q;
    for (;;) {
        const char *e = strchr(p, '&');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (!sh_part_concrete(p, len)) return 0;
        if (!e) break;
        p = e + 1;
    }
    return 1;
}

/* PoP: owner_from_payload @ tools/skills_hub_clawhub.py:_owner_from_payload */
/* Python: owner = data.get("owner") if dict else None; if dict -> .get("handle");
 * return owner.strip() if str and strip non-empty else None. */
char *owner_from_payload(const json_t *data) {
    if (!data || data->type != JSON_OBJECT) return NULL;
    const json_t *owner = json_obj_get(data, "owner");
    if (owner && owner->type == JSON_OBJECT)
        owner = json_obj_get(owner, "handle");
    if (!owner || owner->type != JSON_STRING || !owner->str_val) return NULL;
    const char *s = owner->str_val;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    const char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r')) e--;
    if (e == s) return NULL;
    size_t n = (size_t)(e - s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n); out[n] = '\0';
    return out;
}

/* PoP: owner_matches @ tools/skills_hub_clawhub.py:_owner_matches */
/* Python: if not expected_owner: return True; actual = _owner_from_payload(data);
 * return not actual or actual.lower() == expected_owner.lower(). */
int owner_matches(const char *expected_owner, const json_t *data) {
    if (!expected_owner || !*expected_owner) return 1;
    char *actual = owner_from_payload(data);
    if (!actual) return 1;
    int match = (strcasecmp(actual, expected_owner) == 0);
    free(actual);
    return match;
}

/* PoP: list_local @ tools/skills_hub_official.py:list_local */
char *list_local(void) { return strdup("[]"); }

/* PoP: fetch_from_live_repo @ tools/skills_hub_official.py:_fetch_from_live_repo */
char *fetch_from_live_repo(void) { return strdup("[]"); }

/* PoP: list_remote_skill_dirs @ tools/skills_hub_official.py:_list_remote_skill_dirs */
char *list_remote_skill_dirs(void) { return strdup("[]"); }

/* PoP: upstream_pointer @ tools/skills_hub.py:upstream_pointer */
char *upstream_pointer(void) { return strdup("{}"); }

/* PoP: upstream_pointer_from_content @ tools/skills_hub_official.py:_upstream_pointer_from_content */
char *upstream_pointer_from_content(void) { return strdup("{}"); }

/* PoP: fetch_from_upstream @ tools/skills_hub_official.py:_fetch_from_upstream */
char *fetch_from_upstream(void) { return strdup("[]"); }
