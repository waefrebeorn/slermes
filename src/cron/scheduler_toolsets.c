/*
 * port_cron_scheduler_toolsets.c — faithful C11 port of cron/scheduler.py's
 * toolset-resolution surface:
 *   _resolve_cron_disabled_toolsets
 *   _merge_mcp_into_per_job_toolsets
 *   _resolve_cron_enabled_toolsets
 *
 * Reuses the existing platform_tools C plumbing (hermes_cli/tools_config.py
 * port) — enabled_mcp_server_names and _get_platform_tools — via the
 * platform_tools.h subsystem header. No god headers.
 */

#include "cron_scheduler_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "platform_tools.h"

/* ── small list helpers ─────────────────────────────────────────────── */

static bool list_contains(char *const *list, size_t n, const char *s)
{
    for (size_t i = 0; i < n; i++)
        if (list[i] && strcmp(list[i], s) == 0) return true;
    return false;
}

static char **list_finish(char **list, size_t n, size_t *out_n)
{
    if (list) list[n] = NULL;
    if (out_n) *out_n = n;
    return list;
}

void scheduler_free_string_list(char **list, size_t n)
{
    if (!list) return;
    if (n == 0) { for (size_t i = 0; list[i]; i++) free(list[i]); }
    else        { for (size_t i = 0; i < n; i++) free(list[i]); }
    free(list);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* strip leading/trailing ASCII whitespace into a fresh string */
static char *strip_dup(const char *s)
{
    if (!s) return strdup("");
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' ||
                       s[len-1] == '\n' || s[len-1] == '\r')) len--;
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* ── _resolve_cron_disabled_toolsets ────────────────────────────────── */

/* PoP: scheduler_resolve_cron_disabled_toolsets @ cron/scheduler.py:_resolve_cron_disabled_toolsets */
char **scheduler_resolve_cron_disabled_toolsets(const json_t *cfg,
                                                size_t *out_n)
{
    /* Three protected toolsets always disabled in cron context. */
    static const char *protected_ts[] = { "cronjob", "messaging", "clarify" };

    const json_t *agent_cfg = cfg ? json_object_get((json_t *)cfg, "agent") : NULL;
    const json_t *user_disabled =
        agent_cfg ? json_object_get((json_t *)agent_cfg, "disabled_toolsets") : NULL;
    size_t extra = (user_disabled && user_disabled->type == JSON_ARRAY)
                       ? json_array_size(user_disabled) : 0;

    char **out = calloc(3 + extra + 1, sizeof(char *));
    if (!out) { if (out_n) *out_n = 0; return NULL; }
    size_t n = 0;
    for (size_t i = 0; i < 3; i++)
        out[n++] = strdup(protected_ts[i]);

    for (size_t i = 0; i < extra; i++) {
        const json_t *item = json_array_get(user_disabled, i);
        const char *raw = item ? json_string_value_safe(item) : NULL;
        char buf[128];
        if (!raw) {
            /* str(name) for non-strings: numbers/bools stringify */
            if (item && item->type == JSON_NUMBER) {
                snprintf(buf, sizeof(buf), "%g", item->num_val);
                raw = buf;
            } else if (item && item->type == JSON_BOOL) {
                raw = item->bool_val ? "True" : "False";
            } else {
                continue;
            }
        }
        char *name = strip_dup(raw);
        if (!name) continue;
        if (name[0] && !list_contains(out, n, name))
            out[n++] = name;
        else
            free(name);
    }
    return list_finish(out, n, out_n);
}

/* ── _merge_mcp_into_per_job_toolsets ───────────────────────────────── */

/* PoP: scheduler_merge_mcp_into_per_job_toolsets @ cron/scheduler.py:_merge_mcp_into_per_job_toolsets */
char **scheduler_merge_mcp_into_per_job_toolsets(const char *const *per_job,
                                                 size_t n_per_job,
                                                 const json_t *cfg,
                                                 size_t *out_n)
{
    /* result = [t for t in per_job if t != "no_mcp"] */
    bool had_no_mcp = false;
    size_t n_mcp = 0;
    char **mcp = platform_tools_enabled_mcp_server_names(cfg, &n_mcp);

    char **out = calloc(n_per_job + n_mcp + 1, sizeof(char *));
    if (!out) {
        platform_tools_free_list(mcp, n_mcp);
        if (out_n) *out_n = 0;
        return NULL;
    }
    size_t n = 0;
    for (size_t i = 0; i < n_per_job; i++) {
        if (!per_job[i]) continue;
        if (strcmp(per_job[i], "no_mcp") == 0) { had_no_mcp = true; continue; }
        out[n++] = strdup(per_job[i]);
    }
    if (had_no_mcp) {
        platform_tools_free_list(mcp, n_mcp);
        return list_finish(out, n, out_n);
    }

    /* if set(result) & enabled_mcp: return result (user-named allowlist) */
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n_mcp; j++) {
            if (mcp[j] && strcmp(out[i], mcp[j]) == 0) {
                platform_tools_free_list(mcp, n_mcp);
                return list_finish(out, n, out_n);
            }
        }
    }

    /* union in every globally-enabled MCP server, sorted order.
     * platform_tools_enabled_mcp_server_names already returns a sorted
     * unique list, matching Python's sorted(enabled_mcp). */
    if (n_mcp > 1)
        qsort(mcp, n_mcp, sizeof(char *), cmp_str);
    for (size_t j = 0; j < n_mcp; j++) {
        if (mcp[j] && !list_contains(out, n, mcp[j]))
            out[n++] = strdup(mcp[j]);
    }
    platform_tools_free_list(mcp, n_mcp);
    return list_finish(out, n, out_n);
}

/* ── _resolve_cron_enabled_toolsets ─────────────────────────────────── */

/* PoP: scheduler_resolve_cron_enabled_toolsets @ cron/scheduler.py:_resolve_cron_enabled_toolsets */
char **scheduler_resolve_cron_enabled_toolsets(const json_t *job,
                                               const json_t *cfg,
                                               size_t *out_n)
{
    if (out_n) *out_n = 0;

    /* 1. Per-job enabled_toolsets wins (truthiness: non-empty list). */
    const json_t *per_job =
        job ? json_object_get((json_t *)job, "enabled_toolsets") : NULL;
    if (per_job && per_job->type == JSON_ARRAY && json_array_size(per_job) > 0) {
        size_t pn = json_array_size(per_job);
        const char **vec = calloc(pn + 1, sizeof(char *));
        if (!vec) return NULL;
        size_t vn = 0;
        for (size_t i = 0; i < pn; i++) {
            const char *s = json_string_value_safe(json_array_get(per_job, i));
            if (s) vec[vn++] = s;
        }
        char **out = scheduler_merge_mcp_into_per_job_toolsets(
            vec, vn, cfg, out_n);
        free(vec);
        return out;
    }

    /* 2. Per-platform "cron" toolset config (mirrors _get_platform_tools),
     *    sorted. platform_tools_get returns a sorted unique list already. */
    size_t n = 0;
    char **out = platform_tools_get(cfg, "cron", true, &n);
    if (!out) return NULL;   /* 3. NULL => caller loads the full default set */
    if (n > 1) qsort(out, n, sizeof(char *), cmp_str);
    if (out_n) *out_n = n;
    return out;
}
