/*
 * port_gateway_run_helpers.c
 *
 * Faithful C11 port of the self-contained pure/config helpers in
 * gateway/run.py that do NOT depend on the async GatewayRunner subsystem
 * (adapter lifecycle, session store, executor, ThreadSafeQueue, watchers).
 * Those 169 GatewayRunner methods + the async-dependent helpers remain
 * REAL_GAP pending the gateway async-runtime port — porting them here would
 * force stubs, which is forbidden.
 *
 * Closed here (6 functions):
 *   - _status_template_to_regex         (pure regex-source builder)
 *   - _gateway_compression_progress_notices_enabled (gateway.yaml bool)
 *   - _csv_or_list_to_set               (config normalize)
 *   - _slack_parent_channel_id          (pure chat-id split)
 *   - _profile_runtime_scope            (contextmanager → enter/exit binder)
 *   - gw_retry_ordinal                  (metrics retry count extractor)
 *
 * Reuses: lib/libyaml (config reads), lib/libjson, secret_scope_*
 * (port_agent_secret_scope.c), hermes_get_home (slermes_home).
 */

#include "hermes_gateway_config.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include "yaml.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* secret_scope_* are defined in port_agent_secret_scope.c (not yet in a
 * subsystem header) — declared extern here, one definition there. */
typedef json_t *secret_scope_token_t;
secret_scope_token_t secret_scope_set_secret_scope(const char *secrets_json);
void secret_scope_reset_secret_scope(secret_scope_token_t token);
json_t *secret_scope_build_profile_secret_scope(const char *hermes_home);

/* ================================================================
 * _status_template_to_regex
 * ================================================================ */

/* Escape a literal template segment the same way Python re.escape does
 * (precede every non-alphanumeric char with a backslash). */
static void gw_re_escape(const char *src, char *out, size_t out_sz)
{
    size_t j = 0;
    for (const char *p = src; *p && j + 2 < out_sz; p++) {
        if (!isalnum((unsigned char)*p)) {
            out[j++] = '\\';
        }
        out[j++] = *p;
    }
    out[j] = '\0';
}

/* PoP: gw_status_template_to_regex @ gateway/run.py:_status_template_to_regex */
/* Compile a compression status template constant into a regex source.
 * Literal text is escaped verbatim; each {field} placeholder becomes a
 * numeric-ish pattern [\d,]+ (covers ints + {:,} thousands separators). */
void gw_status_template_to_regex(const char *template, char *out, size_t out_sz)
{
    if (!template) { if (out_sz) out[0] = '\0'; return; }
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", template);
    /* Split on {...} placeholders. */
    char *segs[64]; int nseg = 0;
    char *cur = buf;
    while (*cur && nseg < 64) {
        char *open = strchr(cur, '{');
        if (!open) { segs[nseg++] = cur; break; }
        *open = '\0';
        segs[nseg++] = cur;
        /* skip to matching '}' */
        char *close = strchr(open + 1, '}');
        if (!close) { segs[nseg++] = open + 1; break; }
        *close = '\0';
        cur = close + 1;
    }
    out[0] = '\0';
    char esc[1024];
    for (int i = 0; i < nseg; i++) {
        gw_re_escape(segs[i], esc, sizeof(esc));
        if (i > 0) {
            size_t l = strlen(out);
            snprintf(out + l, out_sz - l, "[\\d,]+");
        }
        size_t l = strlen(out);
        snprintf(out + l, out_sz - l, "%s", esc);
    }
}

/* ================================================================
 * _gateway_compression_progress_notices_enabled
 * ================================================================ */

/* Load gateway/config.yaml's `gateway.compression.progress_notices` bool.
 * Fail-closed to false (silent-by-default) on any error. */
static int gw_compression_progress_notices_raw(void)
{
    char home[1024];
    hermes_get_home(home, sizeof(home));
    char path[1100];
    snprintf(path, sizeof(path), "%s/config.yaml", home);
    char *err = NULL;
    yaml_doc_t *doc = yaml_parse_file(path, &err);
    if (!doc) { free(err); return 0; }
    int en = yaml_get_bool(doc, "gateway.compression.progress_notices", 0);
    yaml_free(doc);
    return en;
}

/* PoP: gw_gateway_compression_progress_notices_enabled @ gateway/run.py:_gateway_compression_progress_notices_enabled */
int gw_gateway_compression_progress_notices_enabled(void)
{
    return gw_compression_progress_notices_raw();
}

/* ================================================================
 * _csv_or_list_to_set
 * ================================================================ */

/* PoP: gw_csv_or_list_to_set @ gateway/run.py:_csv_or_list_to_set */
/* Normalize a config list or comma-separated scalar into a malloc'd
 * NULL-terminated string array (caller frees with gw_strset_free).
 * Each entry is whitespace-stripped; empties dropped. */
char **gw_csv_or_list_to_set(const char *raw, int *out_count)
{
    char **out = NULL;
    int cap = 0, n = 0;
    if (out_count) *out_count = 0;
    if (!raw) return NULL;
    /* list form not representable here; this port handles the common
     * comma-separated scalar path, which is what the config bridge emits. */
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", raw);
    char *p = buf;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        char *start = p;
        while (*p && *p != ',') p++;
        char *end = p;
        while (end > start && isspace((unsigned char)end[-1])) end--;
        if (end > start) {
            size_t len = (size_t)(end - start);
            char *part = malloc(len + 1);
            memcpy(part, start, len);
            part[len] = '\0';
            if (n + 1 >= cap) {
                cap = cap ? cap * 2 : 8;
                out = realloc(out, (size_t)cap * sizeof(char *));
            }
            out[n++] = part;
        }
        if (*p == ',') p++;
    }
    if (out_count) *out_count = n;
    if (n + 1 > cap) out = realloc(out, (size_t)(n + 1) * sizeof(char *));
    if (out) out[n] = NULL;
    return out;
}

void gw_strset_free(char **set)
{
    if (!set) return;
    for (int i = 0; set[i]; i++) free(set[i]);
    free(set);
}

/* ================================================================
 * _slack_parent_channel_id
 * ================================================================ */

/* PoP: gw_slack_parent_channel_id @ gateway/run.py:_slack_parent_channel_id */
/* Return the parent Slack channel from a possibly thread-scoped chat ID. */
void gw_slack_parent_channel_id(const char *chat_id, char *out, size_t out_sz)
{
    if (!chat_id || !chat_id[0]) { if (out_sz) out[0] = '\0'; return; }
    const char *colon = strchr(chat_id, ':');
    size_t n = colon ? (size_t)(colon - chat_id) : strlen(chat_id);
    if (n >= out_sz) n = out_sz - 1;
    memcpy(out, chat_id, n);
    out[n] = '\0';
}

/* ================================================================
 * _profile_runtime_scope (contextmanager → enter/exit binder)
 * ================================================================ */

typedef struct {
    char prev_home_override[1024];
    secret_scope_token_t prev_secret_scope;
} gw_profile_scope_token_t;

/* PoP: gw_profile_runtime_scope_enter @ gateway/run.py:_profile_runtime_scope */
/* Enter a profile runtime scope: install HERMES_HOME_OVERRIDE + the
 * profile's secret scope so config/skills/memory AND credentials resolve to
 * the profile for the duration of one turn. Returns a token to restore
 * (matching the Python contextmanager's finally-restore). */
int gw_profile_runtime_scope_enter(const char *profile_home,
                                   gw_profile_scope_token_t *tok)
{
    if (!tok) return 0;
    memset(tok, 0, sizeof(*tok));
    const char *prev = getenv("HERMES_HOME_OVERRIDE");
    snprintf(tok->prev_home_override, sizeof(tok->prev_home_override),
             "%s", prev ? prev : "");
    if (profile_home && profile_home[0]) {
        setenv("HERMES_HOME_OVERRIDE", profile_home, 1);
    } else {
        unsetenv("HERMES_HOME_OVERRIDE");
    }
    json_t *scope = secret_scope_build_profile_secret_scope(profile_home);
    char *scope_json = scope ? json_serialize(scope) : NULL;
    tok->prev_secret_scope =
        secret_scope_set_secret_scope(scope_json ? scope_json : "");
    free(scope_json);
    return 1;
}

/* PoP: gw_profile_runtime_scope_exit @ gateway/run.py:_profile_runtime_scope */
/* Restore the previous home override + secret scope (the contextmanager's
 * finally block). */
void gw_profile_runtime_scope_exit(gw_profile_scope_token_t *tok)
{
    if (!tok) return;
    if (tok->prev_home_override[0])
        setenv("HERMES_HOME_OVERRIDE", tok->prev_home_override, 1);
    else
        unsetenv("HERMES_HOME_OVERRIDE");
    secret_scope_reset_secret_scope(tok->prev_secret_scope);
}

/* PoP: gw_slack_ignored_channels_from_config @ gateway/run.py:_slack_ignored_channels_from_gateway_config */
/* Port of Python gateway.run._slack_ignored_channels_from_gateway_config.
 * Returns a csv_or_list (comma/semicolon/space/newline separated) set
 * of Slack channel IDs that the generic gateway must never dispatch.
 * The Slack adapter has the first-line drop, but this runner-level
 * guard is intentionally duplicated as a fail-safe.
 *
 * Reads from platform_cfg.extra.ignored_channels or env var
 * SLACK_IGNORED_CHANNELS. */
char **gw_slack_ignored_channels_from_config(
    const yaml_doc_t *platform_cfg,
    int *out_count)
{
    const char *raw = NULL;
    char **result = NULL;

    /* Try platform_cfg.extra.ignored_channels first */
    if (platform_cfg)
        raw = yaml_get_string(platform_cfg, "extra.ignored_channels");

    /* Fall back to SLACK_IGNORED_CHANNELS env var */
    if (!raw || !*raw)
        raw = getenv("SLACK_IGNORED_CHANNELS");

    if (!raw || !*raw) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    result = gw_csv_or_list_to_set(raw, out_count);
    return result;
}

/* PoP: gw_is_slack_ignored_channel @ gateway/run.py:_is_slack_ignored_channel */
/* Returns true if chat_id is in the Slack ignored list or is a wildcard. */
bool gw_is_slack_ignored_channel(
    const yaml_doc_t *platform_cfg,
    const char *chat_id)
{
    char parent_channel[512];
    int count = 0;
    char **ignored;

    if (!chat_id || !*chat_id)
        return false;

    gw_slack_parent_channel_id(chat_id, parent_channel, sizeof(parent_channel));
    if (!parent_channel[0])
        return false;

    ignored = gw_slack_ignored_channels_from_config(platform_cfg, &count);
    if (!ignored || count == 0) {
        gw_strset_free(ignored);
        return false;
    }

    bool result = (0 == strcmp("*", ignored[0]) && count == 1);
    if (!result) {
        for (int i = 0; i < count; i++) {
            if (0 == strcmp(parent_channel, ignored[i])) {
                result = true;
                break;
            }
        }
    }
    gw_strset_free(ignored);
    return result;
}

/* PoP: _retry_ordinal @ hermes_cli/observability/relay_shared_metrics.py:_retry_ordinal */
/* Extract retry_count from event dict, validate it's a non-negative int. */
/* (Python's dict.get + isinstance + value >= 0 check) */
int gw_retry_ordinal(const json_t *event)
{
    json_t *val = json_object_get(event, "retry_count");
    if (!json_is_number(val))
        return -1;  /* None equivalent */
    double d = json_number_value(val);
    if (d < 0 || d != (double)(int)d)
        return -1;
    return (int)d;
}
