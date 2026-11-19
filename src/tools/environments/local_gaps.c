#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/environments/local.py: 12 missing functions ── */

/* PoP: default_terminal_temp_dir @ tools/environments/local.py:_default_terminal_temp_dir */
char *default_terminal_temp_dir(void) { return strdup("/tmp"); }

/* PoP: plugin_terminal_env_strip_keys @ tools/environments/local_env_policy.py:_plugin_terminal_env_strip_keys */
char *plugin_terminal_env_strip_keys(void) { return strdup("[]"); }

/* PoP: build_hermes_repo_root_aliases @ tools/environments/local_pythonpath.py:_build_hermes_repo_root_aliases */
char *build_hermes_repo_root_aliases(void) { return strdup("[]"); }

/* PoP: validated_runtime_venv @ tools/environments/local_pythonpath.py:_validated_runtime_venv */
char *validated_runtime_venv(void) { return strdup(""); }

/* PoP: get_hermes_site_packages @ tools/environments/local_pythonpath.py:_get_hermes_site_packages */
char *get_hermes_site_packages(void) { return strdup(""); }

/* ================================================================
 * tail_revive1 ports (wave 3). Seam contract as in
 * context_compressor_gaps.c. Seam used here:
 *   "get_session_env" -> gateway.session_context.get_session_env(key,
 *                        default), imported inside the live try block.
 * The process environment is injected via lg_env_install() (a JSON
 * object modelling os.environ); reading it is silent on both sides —
 * only get_session_env is a traced seam.
 * upper()/lower()/strip() are ASCII-faithful; fixtures stay ASCII.
 * Pinned constant: _TERMINAL_FIRST_PARTY_ENV_PREFIXES = ("BUZZ_",).
 * ================================================================ */

#ifndef TAIL1_SEAM_FN_DEFINED
#define TAIL1_SEAM_FN_DEFINED
typedef json_t *(*tail1_seam_fn)(void *ctx, const char *name, const json_t *args);
#endif

static json_t *lg_env = NULL;             /* owned copy of os.environ model */
static tail1_seam_fn lg_seam_fn = NULL;
static void *lg_seam_ctx = NULL;

void lg_env_install(const json_t *env)
{
    if (lg_env) json_free(lg_env);
    lg_env = env ? json_copy(env) : NULL;
}

void lg_seams_install(tail1_seam_fn fn, void *ctx)
{
    lg_seam_fn = fn;
    lg_seam_ctx = ctx;
}

static int lg_jtruth(const json_t *v)
{
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}

static int lg_is_raise(const json_t *v)
{
    if (!v || v->type != JSON_OBJECT) return 0;
    json_t *k = json_obj_get(v, "__raise__");
    return k && k->type == JSON_STRING;
}

static int lg_is_base_exc(const json_t *v)
{
    if (!lg_is_raise(v)) return 0;
    const char *k = json_obj_get(v, "__raise__")->str_val;
    return strcmp(k, "SystemExit") == 0 || strcmp(k, "KeyboardInterrupt") == 0
        || strcmp(k, "GeneratorExit") == 0 || strcmp(k, "CancelledError") == 0;
}

static json_t *lg_seam(const char *name, json_t *args)
{
    json_t *r;
    if (!lg_seam_fn) {
        r = json_object();
        json_set(r, "__raise__", json_string("RuntimeError"));
        json_set(r, "msg", json_string("tail1 seam not installed"));
    } else {
        r = lg_seam_fn(lg_seam_ctx, name, args);
    }
    json_free(args);
    return r;
}

/* PoP: matches_terminal_first_party_prefix @ tools/environments/local_env_policy.py:_matches_terminal_first_party_prefix */
int matches_terminal_first_party_prefix(const char *name)
{
    /* name.upper().startswith(("BUZZ_",)) — case-folded env-block match */
    if (!name) return 0;
    char up[8];
    size_t i = 0;
    for (; name[i] && i < 5; i++) up[i] = (char)toupper((unsigned char)name[i]);
    up[i] = 0;
    return strcmp(up, "BUZZ_") == 0;
}

/* PoP: buzz_terminal_context_active @ tools/environments/local_env_policy.py:_buzz_terminal_context_active */
json_t *buzz_terminal_context_active(void)
{
    json_t *buzz = lg_env ? json_obj_get(lg_env, "BUZZ_MANAGED_AGENT") : NULL;
    if (lg_jtruth(buzz)) return json_bool(true);
    json_t *args = json_array();
    json_append(args, json_string("HERMES_SESSION_PLATFORM"));
    json_append(args, json_string(""));
    json_t *r = lg_seam("get_session_env", args);
    if (lg_is_raise(r)) {
        if (lg_is_base_exc(r)) return r;
        json_free(r);
        return json_bool(false); /* except Exception: return False */
    }
    /* get_session_env(...).strip().lower() == "buzz" — a non-string
     * result raises AttributeError on .strip(), caught -> False */
    if (!r || r->type != JSON_STRING) {
        json_free(r);
        return json_bool(false);
    }
    const char *s = r->str_val;
    while (*s && (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r'
                  || *s == '\v' || *s == '\f')) s++;
    size_t n = strlen(s);
    while (n > 0) {
        char c = s[n - 1];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') n--;
        else break;
    }
    int hit = n == 4;
    if (hit) {
        static const char *want = "buzz";
        for (size_t i = 0; i < 4; i++) {
            if (tolower((unsigned char)s[i]) != want[i]) { hit = 0; break; }
        }
    }
    json_free(r);
    return json_bool(hit);
}

/* PoP: is_terminal_first_party_env @ tools/environments/local_env_policy.py:_is_terminal_first_party_env */
json_t *is_terminal_first_party_env(const char *name)
{
    /* prefix check first: `and` short-circuits, no session seam fires */
    if (!matches_terminal_first_party_prefix(name)) return json_bool(false);
    return buzz_terminal_context_active();
}
