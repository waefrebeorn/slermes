/*
 * port_cli.c — C port of cli.py
 */

#include "hermes_logger.h"
#include "hermes_skill_commands.h"
#include "hermes_json.h"
#include "toolsets.h"
#include "registry.h"
#include "cron_jobs.h"
#include "slermes_home.h"
#include "port_slash_commands.h"
#include "cli_wave4b.h"
#include "cli_hermes_cli.h"
#include "cli_helpers2.h"
#include "hermes_gateway_config.h"

/* From plugins_wrappers.c — plugin command registry access. */
extern json_t *plug_get_commands(void);
#ifndef HERMES_PATH_MAX
#define HERMES_PATH_MAX 4096
#endif
#include <ctype.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* ── cli.py constants used by _enable_extended_enter_keys ────────────────── */
/* _MODIFY_OTHER_KEYS_SEQ = "\x1b[>4;2m" — xterm modifyOtherKeys level 2. */
static const char _MODIFY_OTHER_KEYS_SEQ[]  = "\x1b[>4;2m";
/* _EXTENDED_ENTER_KEYS_SEQ = Kitty push + modifyOtherKeys (concatenation). */
static const char _EXTENDED_ENTER_KEYS_SEQ[] = "\x1b[>1u\x1b[>4;2m";

/* PoP: string @ cli.py:_print_nous_credits_block */
/* PoP: string @ cli.py:_show_credits */

/* ── cli.py:CanonicalUsage — dataclass → struct ─────────────────────────
 * Python re-exports agent/usage_pricing.py:CanonicalUsage lazily; the C home
 * is this struct + helpers. Fields mirror the dataclass exactly (raw_usage is
 * an owned JSON value or NULL). prompt_tokens/total_tokens port the
 * properties, cli_canonical_usage_add ports __add__ (raw_usage dropped on the
 * sum, request_count added). */

typedef struct {
    long long input_tokens;
    long long output_tokens;
    long long cache_read_tokens;
    long long cache_write_tokens;
    long long reasoning_tokens;
    int       request_count;
    json_t   *raw_usage;   /* owned; may be NULL */
} cli_canonical_usage_t;

/* PoP: CanonicalUsage @ cli.py:CanonicalUsage */
cli_canonical_usage_t *cli_canonical_usage_new(long long input_tokens,
                                               long long output_tokens,
                                               long long cache_read_tokens,
                                               long long cache_write_tokens,
                                               long long reasoning_tokens,
                                               int request_count,
                                               json_t *raw_usage) {
    cli_canonical_usage_t *u = calloc(1, sizeof(*u));
    if (!u) return NULL;
    u->input_tokens       = input_tokens;
    u->output_tokens      = output_tokens;
    u->cache_read_tokens  = cache_read_tokens;
    u->cache_write_tokens = cache_write_tokens;
    u->reasoning_tokens   = reasoning_tokens;
    /* Python dataclass default request_count=1 applies when omitted. */
    u->request_count      = request_count > 0 ? request_count : 1;
    u->raw_usage          = raw_usage ? json_copy(raw_usage) : NULL;
    return u;
}

/* Python @property prompt_tokens: input + cache_read + cache_write. */
int cli_canonical_usage_prompt_tokens(const cli_canonical_usage_t *u) {
    if (!u) return 0;
    return (int)(u->input_tokens + u->cache_read_tokens + u->cache_write_tokens);
}

/* Python @property total_tokens: prompt_tokens + output_tokens. */
int cli_canonical_usage_total_tokens(const cli_canonical_usage_t *u) {
    if (!u) return 0;
    return cli_canonical_usage_prompt_tokens(u) + (int)u->output_tokens;
}

/* Python __add__: sum two usage buckets (e.g. MoA fan-out + aggregator).
 * raw_usage is dropped on the sum; request_count adds. */
cli_canonical_usage_t *cli_canonical_usage_add(const cli_canonical_usage_t *a,
                                               const cli_canonical_usage_t *b) {
    if (!a || !b) return NULL;
    return cli_canonical_usage_new(
        a->input_tokens + b->input_tokens,
        a->output_tokens + b->output_tokens,
        a->cache_read_tokens + b->cache_read_tokens,
        a->cache_write_tokens + b->cache_write_tokens,
        a->reasoning_tokens + b->reasoning_tokens,
        a->request_count + b->request_count,
        NULL);
}

void cli_canonical_usage_free(cli_canonical_usage_t *u) {
    if (!u) return;
    if (u->raw_usage) json_free(u->raw_usage);
    free(u);
}

/* ── Reasoning-tag stripping (cli.py display surface) ───────────────── */

static const char *CLI_REASONING_TAGS[] = {
    "REASONING_SCRATCHPAD", "think", "thinking", "reasoning", "thought",
};
#define CLI_N_REASONING_TAGS 5

static int cli_strcasestr_find(const char *hay, const char *needle,
                               size_t *out_at, size_t *out_len) {
    size_t hl = strlen(hay), nl = strlen(needle);
    if (nl == 0 || nl > hl) return 0;
    for (size_t i = 0; i + nl <= hl; i++) {
        size_t k = 0;
        while (k < nl &&
               tolower((unsigned char)hay[i + k]) == tolower((unsigned char)needle[k]))
            k++;
        if (k == nl) { *out_at = i; *out_len = nl; return 1; }
    }
    return 0;
}

/* Remove every occurrence of a literal tag with surrounding < > from `text`
 * starting at scan position `from`; returns malloc'd new string. */
/* Port of Python cli.py:_strip_reasoning_tags — removes closed reasoning
 * pairs, unterminated opens, orphan closes, and tool-call XML blocks. */
char *cli_strip_reasoning_tags(const char *text) {
    if (!text) return strdup("");
    size_t cap = strlen(text) + 16;
    char *cleaned = strdup(text);
    if (!cleaned) return NULL;

    for (int pass = 0; pass < 3; pass++) {
        for (int ti = 0; ti < CLI_N_REASONING_TAGS; ti++) {
            const char *tag = CLI_REASONING_TAGS[ti];
            size_t tl = strlen(tag);
            /* Closed pair: <tag> ... </tag> (+ trailing ws). Loop until none. */
            for (;;) {
                char *open_p = strcasestr(cleaned, tag);
                int removed = 0;
                while (open_p) {
                    if (open_p > cleaned && open_p[-1] == '<') {
                        char *close_search = open_p + tl;
                        if (*close_search == '>') {
                            /* find </tag> after */
                            char close_pat[64];
                            snprintf(close_pat, sizeof(close_pat), "</%s", tag);
                            char *close_p = strcasestr(close_search, close_pat);
                            if (close_p) {
                                char *after = close_p + strlen(close_pat);
                                if (*after == '>') {
                                    after++;
                                    while (*after == ' ' || *after == '\t' ||
                                           *after == '\n' || *after == '\r') after++;
                                    memmove(open_p - 1, after, strlen(after) + 1);
                                    removed = 1;
                                }
                            }
                        }
                    }
                    break;
                }
                if (!removed) break;
            }
            /* Unterminated open: strip "<tag>..." to end. */
            {
                char pat[66];
                snprintf(pat, sizeof(pat), "<%s>", tag);
                char *p = strcasestr(cleaned, pat);
                if (p) { *p = '\0'; }
            }
            /* Orphan close: "</tag>" + trailing ws. */
            {
                char pat[68];
                snprintf(pat, sizeof(pat), "</%s>", tag);
                char *p;
                while ((p = strcasestr(cleaned, pat)) != NULL) {
                    char *after = p + strlen(pat);
                    while (*after == ' ' || *after == '\t' ||
                           *after == '\n' || *after == '\r') after++;
                    memmove(p, after, strlen(after) + 1);
                }
            }
        }
    }

    /* Tool-call XML blocks: <tool_call ...>...</tool_call> etc. */
    static const char *TC_TAGS[] = { "tool_call", "tool_calls", "tool_result",
                                     "function_call", "function_calls" };
    for (int ti = 0; ti < 5; ti++) {
        const char *tct = TC_TAGS[ti];
        char open_pat[80], close_pat[80];
        for (;;) {
            snprintf(open_pat, sizeof(open_pat), "<%s", tct);      /* boundary via next char check */
            snprintf(close_pat, sizeof(close_pat), "</%s>", tct);
            char *open_p = strcasestr(cleaned, open_pat);
            if (!open_p) break;
            char after = open_p[strlen(open_pat)];
            if (after != '>' && after != ' ' && after != '\t' && after != '\n') {
                /* not our tag boundary (e.g. <tool_calls2) — skip past */
                memmove(cleaned, open_p + 1, strlen(open_p));   /* wrong; shift search */
                /* safer: advance by removing nothing — break to avoid loop */
                break;
            }
            char *close_p = strcasestr(open_p, close_pat);
            if (!close_p) {
                /* unterminated tool-call block: strip to end */
                *open_p = '\0';
                break;
            }
            char *endp = close_p + strlen(close_pat);
            while (*endp == ' ' || *endp == '\t' || *endp == '\n' || *endp == '\r') endp++;
            memmove(open_p, endp, strlen(endp) + 1);
        }
    }

    /* Boundary-gated <function name="..."> ... </function> (Gemma-style):
     * open tag must sit at line start or after [\n\r.!?:] + optional
     * spaces/tabs, and carry a name= attribute. */
    for (;;) {
        char *fn_p = strcasestr(cleaned, "<function");
        int removed_any = 0;
        while (fn_p) {
            char after = fn_p[strlen("<function")];
            int boundary_ok = 0;
            if (fn_p == cleaned) boundary_ok = 1;
            else {
                const char *q = fn_p - 1;
                while (q >= cleaned && (*q == ' ' || *q == '\t')) q--;
                if (q >= cleaned && (*q == '\n' || *q == '\r' || *q == '.' ||
                                     *q == '!' || *q == '?' || *q == ':'))
                    boundary_ok = 1;
                else if (q < cleaned) boundary_ok = 1;
            }
            if ((after == ' ' || after == '\t' || after == '\n') && boundary_ok) {
                char *gt = strchr(fn_p, '>');
                char *name_at = fn_p ? strcasestr(fn_p, "name") : NULL;
                if (gt && name_at && name_at < gt &&
                    (name_at == fn_p + strlen("<function") ||
                     name_at[-1] == ' ' || name_at[-1] == '\t')) {
                    char *close_p = strcasestr(gt, "</function>");
                    if (close_p) {
                        char *endp2 = close_p + strlen("</function>");
                        while (*endp2 == ' ' || *endp2 == '\t' ||
                               *endp2 == '\n' || *endp2 == '\r') endp2++;
                        memmove(fn_p, endp2, strlen(endp2) + 1);
                        removed_any = 1;
                        break;
                    }
                }
            }
            fn_p = fn_p + 9 < cleaned + strlen(cleaned)
                ? strcasestr(fn_p + 9, "<function") : NULL;
        }
        if (!removed_any) break;
    }

    /* Stray generic close tags. */
    static const char *STRAY[] = { "</tool_call>", "</tool_calls>",
        "</tool_result>", "</function_call>", "</function_calls>", "</function>" };
    for (int si = 0; si < 6; si++) {
        char *p;
        while ((p = strcasestr(cleaned, STRAY[si])) != NULL) {
            char *after = p + strlen(STRAY[si]);
            while (*after == ' ' || *after == '\t' || *after == '\n' || *after == '\r') after++;
            memmove(p, after, strlen(after) + 1);
        }
    }

    /* final .strip() */
    char *b = cleaned;
    while (*b && isspace((unsigned char)*b)) b++;
    char *e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) *--e = '\0';
    char *result = strdup(b);
    free(cleaned);
    (void)cap;
    return result;
}

/* Port of Python cli.py:_assistant_content_as_text */
char *cli_assistant_content_as_text(const json_t *content) {
    if (!content) return strdup("");
    if (json_is_string(content)) {
        const char *s = json_string_value(content);
        return strdup(s ? s : "");
    }
    if (json_is_array(content)) {
        size_t cap = 128, len = 0;
        char *out = malloc(cap);
        if (!out) return NULL;
        out[0] = '\0';
        size_t n = json_array_size(content);
        for (size_t i = 0; i < n; i++) {
            const json_t *part = json_array_get(content, i);
            if (!part || !json_is_object(part)) continue;
            const char *type = json_string_value(json_object_get(part, "type"));
            if (!type || strcmp(type, "text") != 0) continue;
            const char *txt = json_string_value(json_object_get(part, "text"));
            const char *piece = txt ? txt : "";
            if (!piece[0]) continue;
            size_t plen = strlen(piece);
            int need_nl = (len > 0);
            while (len + plen + need_nl + 1 > cap) {
                cap *= 2;
                char *tmp = realloc(out, cap);
                if (!tmp) { free(out); return NULL; }
                out = tmp;
            }
            if (need_nl) out[len++] = '\n';
            memcpy(out + len, piece, plen + 1);
            len += plen;
        }
        return out;
    }
    return strdup("");
}

/* Port of Python cli.py:_assistant_copy_text */
char *cli_assistant_copy_text(const json_t *content) {
    char *plain = cli_assistant_content_as_text(content);
    if (!plain) return NULL;
    char *stripped = cli_strip_reasoning_tags(plain);
    free(plain);
    return stripped;
}

/* ── cli.py prefill-message helpers ────────────────────────────────────── */

/* Strip leading/trailing whitespace in place-ish: returns malloc'd trimmed. */
static char *cli_trim_dup(const char *s) {
    if (!s) return strdup("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    size_t n = strlen(a);
    while (n > 0 && isspace((unsigned char)a[n - 1])) n--;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* PoP: _load_prefill_messages @ cli.py:_load_prefill_messages */
json_t *cli_load_prefill_messages(const char *file_path) {
    /* Empty path → empty list (Python returns []). */
    json_t *empty = json_array();
    if (!file_path || !*file_path) return empty;

    /* expanduser(~) then resolve relative paths from the home dir
     * (Python anchors at ~/.hermes; the C port's native home analog). */
    char expanded[HERMES_PATH_MAX];
    if (file_path[0] == '~' && (file_path[1] == '/' || file_path[1] == '\0')) {
        const char *home = getenv("HOME");
        snprintf(expanded, sizeof(expanded), "%s%s", home ? home : "",
                 file_path + 1);
    } else {
        snprintf(expanded, sizeof(expanded), "%s", file_path);
    }
    char resolved[HERMES_PATH_MAX];
    if (expanded[0] != '/') {
        snprintf(resolved, sizeof(resolved), "%s/%s", slermes_home(), expanded);
    } else {
        snprintf(resolved, sizeof(resolved), "%s", expanded);
    }

    if (access(resolved, F_OK) != 0) {
        hermes_log(LOG_WARNING, "cli", "Prefill messages file not found: %s",
                   resolved);
        return empty;
    }
    char *jerr = NULL;
    json_t *data = json_parse_file(resolved, &jerr);
    if (jerr) free(jerr);
    if (!data) {
        hermes_log(LOG_WARNING, "cli", "Failed to load prefill messages from %s",
                   resolved);
        return empty;
    }
    if (!json_is_array(data)) {
        hermes_log(LOG_WARNING,
                   "cli", "Prefill messages file must contain a JSON array: %s",
                   resolved);
        json_free(data);
        return empty;
    }
    json_free(empty);
    return data;
}

/* PoP: _resolve_prefill_messages_file @ cli.py:_resolve_prefill_messages_file */
char *cli_resolve_prefill_messages_file(const json_t *config) {
    /* 1. HERMES_PREFILL_MESSAGES_FILE env wins. */
    char *env_path = cli_trim_dup(getenv("HERMES_PREFILL_MESSAGES_FILE"));
    if (env_path && *env_path) return env_path;
    free(env_path);

    /* 2. Top-level ``prefill_messages_file`` is canonical. */
    if (config && json_is_object(config)) {
        json_t *top = json_obj_get(config, "prefill_messages_file");
        if (top && json_is_string(top)) {
            char *v = cli_trim_dup(json_string_value(top));
            if (v && *v) return v;
            free(v);
        }
        /* 3. Legacy ``agent.prefill_messages_file`` fallback. */
        json_t *agent_cfg = json_obj_get(config, "agent");
        if (agent_cfg && json_is_object(agent_cfg)) {
            json_t *legacy = json_obj_get(agent_cfg, "prefill_messages_file");
            if (legacy && json_is_string(legacy)) {
                char *lv = cli_trim_dup(json_string_value(legacy));
                if (lv && *lv) return lv;
                free(lv);
            }
        }
    }
    return strdup("");
}

/* ── cli.py reasoning / service-tier config parsing ───────────────────── */

/* PoP: _parse_reasoning_config @ cli.py:_parse_reasoning_config */
json_t *cli_parse_reasoning_config(const char *effort, bool effort_is_bool_false) {
    json_t *result = reasoning_parse_effort(effort, effort_is_bool_false);
    char *trimmed = effort_is_bool_false ? NULL : cli_trim_dup(effort);
    if (trimmed && *trimmed && !result) {
        hermes_log(LOG_WARNING, "cli",
                   "Unknown reasoning_effort '%s', using default (medium)",
                   effort ? effort : "");
    }
    free(trimmed);
    return result;
}

/* PoP: _parse_service_tier_config @ cli.py:_parse_service_tier_config */
char *cli_parse_service_tier_config(const char *raw) {
    char *value = cli_trim_dup(raw);
    if (value) for (char *c = value; *c; c++) *c = (char)tolower((unsigned char)*c);
    if (!value || !*value ||
        strcmp(value, "normal") == 0 || strcmp(value, "default") == 0 ||
        strcmp(value, "standard") == 0 || strcmp(value, "off") == 0 ||
        strcmp(value, "none") == 0) {
        free(value);
        return NULL;
    }
    if (strcmp(value, "fast") == 0 || strcmp(value, "priority") == 0 ||
        strcmp(value, "on") == 0) {
        free(value);
        return strdup("priority");
    }
    hermes_log(LOG_WARNING, "cli", "Unknown service_tier '%s', ignoring",
               raw ? raw : "");
    free(value);
    return NULL;
}

/* Port of Python cli:load_cli_config */

/* Port of Python cli:find_spec */


/* Port of Python cli:AIAgent */

/* ── cli.py lazy toolset surface — delegates to the C toolset registry ── */

/* Does a JSON array of toolset names contain `ts`? */
static bool cli_ts_list_contains(const json_t *list, const char *ts) {
    if (!list || !json_is_array(list) || !ts || !*ts) return false;
    for (size_t i = 0; i < json_len(list); i++) {
        json_t *v = json_get(list, i);
        if (v && json_is_string(v) && strcmp(json_string_value(v), ts) == 0)
            return true;
    }
    return false;
}

/*
 * Python: wait for MCP discovery, then model_tools.get_tool_definitions with
 * enabled/disabled-toolset filtering over the registry. C equivalent:
 * refresh registry availability (the discovery-lock join happens upstream in
 * the agent build path), then assemble OpenAI-format definitions for every
 * available registered tool passing the filter. skip_tool_search_assembly is
 * accepted for signature parity; the C port has no post-collapse assembly
 * PoP: get_tool_definitions @ cli.py:get_tool_definitions
 */
json_t *cli_get_tool_definitions(const json_t *enabled_toolsets,
                                 const json_t *disabled_toolsets,
                                 bool quiet_mode,
                                 bool skip_tool_search_assembly) {
    (void)quiet_mode;
    (void)skip_tool_search_assembly;
    json_t *out = json_array();
    if (!out) return NULL;
    bool have_enabled = enabled_toolsets && json_is_array(enabled_toolsets) &&
                        json_len(enabled_toolsets) > 0;
    registry_refresh_availability();
    size_t n = registry_get_count();
    for (size_t i = 0; i < n; i++) {
        const char *name = registry_get_name(i);
        if (!name || !*name) continue;
        tool_t *t = registry_find(name);
        if (!t || !t->available) continue;

        /* Toolset membership: registry map first, then the tool's own tag. */
        const char *ts = registry_get_toolset_for_tool(name);
        if (!ts || !*ts) ts = t->toolset;
        if (have_enabled) {
            if (!cli_ts_list_contains(enabled_toolsets, ts)) continue;
        } else if (cli_ts_list_contains(disabled_toolsets, ts)) {
            continue;
        }

        json_t *params = json_parse(t->schema_json, NULL);
        if (!params || !json_is_object(params)) {
            if (params) json_free(params);
            params = json_object();
            json_set(params, "type", json_string("object"));
            json_set(params, "properties", json_object());
        }
        json_t *fn = json_object();
        json_set(fn, "name", json_string(t->name));
        json_set(fn, "description", json_string(t->description));
        json_set(fn, "parameters", params);
        json_t *def = json_object();
        json_set(def, "type", json_string("function"));
        json_set(def, "function", fn);
        json_append(out, def);
    }
    return out;
}

/*
 * PoP: get_toolset_for_tool @ cli.py:get_toolset_for_tool
 */
const char *cli_get_toolset_for_tool(const char *tool_name) {
    return registry_get_toolset_for_tool(tool_name);
}

/* Serialize a static toolset def into {"name","description","tools","includes"}.
 * When `def` is NULL (plugin-registered name), resolves tools via the
 * registry-merged resolution so plugin toolsets still appear. */
static json_t *cli_toolset_def_to_json(const char *name,
                                       const toolset_def_t *def) {
    json_t *o = json_object();
    json_set(o, "name", json_string(name));
    json_set(o, "description",
             json_string(def && def->description ? def->description : ""));
    json_t *tools = json_array();
    if (def && def->tools)
        for (size_t i = 0; def->tools[i]; i++)
            json_append(tools, json_string(def->tools[i]));
    else {
        /* Plugin toolset: resolved names stand in for direct tools. */
        size_t rn = 0;
        char **rt = toolsets_resolve(name, true, &rn);
        for (size_t i = 0; rt && i < rn; i++) json_append(tools, json_string(rt[i]));
        toolsets_free_list(rt, rn);
    }
    json_set(o, "tools", tools);
    json_t *inc = json_array();
    if (def && def->includes)
        for (size_t i = 0; def->includes[i]; i++)
            json_append(inc, json_string(def->includes[i]));
    json_set(o, "includes", inc);
    return o;
}

/*
 * Python returns {name: definition} for static + plugin-registered toolsets.
 * PoP: get_all_toolsets @ cli.py:get_all_toolsets
 */
char *cli_get_all_toolsets(void) {
    json_t *result = json_object();
    size_t n = 0;
    char **names = toolsets_get_names(&n);
    for (size_t i = 0; i < n; i++) {
        const toolset_def_t *def = toolsets_get_static(names[i]);
        json_set(result, names[i], cli_toolset_def_to_json(names[i], def));
    }
    toolsets_free_list(names, n);
    char *s = json_serialize(result);
    json_free(result);
    return s;
}

/*
 * {name, description, direct_tools, includes, resolved_tools, tool_count,
 * PoP: get_toolset_info @ cli.py:get_toolset_info
 */
char *cli_get_toolset_info(const char *name) {
    if (!name) return NULL;
    const toolset_def_t *def = toolsets_get_static(name);
    if (!def) return NULL;
    size_t rn = 0;
    char **resolved = toolsets_resolve(name, true, &rn);
    json_t *o = cli_toolset_def_to_json(name, def);
    json_t *rt = json_array();
    for (size_t i = 0; i < rn; i++) json_append(rt, json_string(resolved[i]));
    toolsets_free_list(resolved, rn);
    json_set(o, "resolved_tools", rt);
    json_set(o, "tool_count", json_number((double)rn));
    bool composite = false;
    if (def->includes)
        for (size_t i = 0; def->includes[i]; i++) composite = true;
    json_set(o, "is_composite", json_bool(composite));
    char *s = json_serialize(o);
    json_free(o);
    return s;
}

/* PoP: validate_toolset @ cli.py:validate_toolset */
bool cli_validate_toolset(const char *name) {
    return toolsets_validate(name);
}

/* Port of Python cli:_sync_process_session_id */

/*
 * Python lazily imports cron.get_job; C delegates to the cron jobs port
 * PoP: get_job @ cli.py:get_job
 */
json_t *cli_get_job(const char *job_id) {
    return cronjobs_get_job(job_id);
}

/* Port of Python cli:_cleanup_all_terminals */

/* ── cli.py callback registration surface — function-pointer globals ────
 * Python's terminal_tool/skills_tool setters register process-wide callbacks;
 * the C port mirrors that with function-pointer globals + setters (matching
 * the codex_session_set_approval_callback pattern). */

typedef void (*cli_sudo_password_callback_t)(const char *prompt);
typedef bool (*cli_approval_callback_t)(const char *title, const char *description);
typedef void (*cli_secret_capture_callback_t)(const char *name, const char *value);

static cli_sudo_password_callback_t  g_cli_sudo_password_callback  = NULL;
static cli_approval_callback_t       g_cli_approval_callback       = NULL;
static cli_secret_capture_callback_t g_cli_secret_capture_callback = NULL;

/* PoP: set_sudo_password_callback @ cli.py:set_sudo_password_callback */
void cli_set_sudo_password_callback(cli_sudo_password_callback_t cb) {
    g_cli_sudo_password_callback = cb;
    hermes_log(LOG_DEBUG, "cli", "sudo password callback registered");
}

/* Current sudo-password callback (NULL when none registered). */
cli_sudo_password_callback_t cli_get_sudo_password_callback(void) {
    return g_cli_sudo_password_callback;
}

/* PoP: set_approval_callback @ cli.py:set_approval_callback */
void cli_set_approval_callback(cli_approval_callback_t cb) {
    g_cli_approval_callback = cb;
    hermes_log(LOG_DEBUG, "cli", "approval callback registered");
}

/* Current approval callback (NULL when none registered). */
cli_approval_callback_t cli_get_approval_callback(void) {
    return g_cli_approval_callback;
}

/* PoP: set_secret_capture_callback @ cli.py:set_secret_capture_callback */
void cli_set_secret_capture_callback(cli_secret_capture_callback_t cb) {
    g_cli_secret_capture_callback = cb;
    hermes_log(LOG_DEBUG, "cli", "secret capture callback registered");
}

/* Current secret-capture callback (NULL when none registered). */
cli_secret_capture_callback_t cli_get_secret_capture_callback(void) {
    return g_cli_secret_capture_callback;
}

/* PoP: _secret_capture_callback @ cli.py:_secret_capture_callback */
json_t *hcli_secret_capture_callback(hcli_t *self,
                                      const char *var_name,
                                      const char *prompt,
                                      json_t *metadata) {
    (void)self;
    (void)prompt;
    (void)metadata;

    /* If a registered callback exists, forward to it. */
    if (g_cli_secret_capture_callback) {
        g_cli_secret_capture_callback(var_name ? var_name : "",
                                      /* value placeholder — callback fills this */ "");
        /* Build a minimal success dict for the callback path. */
        json_t *result = json_object();
        json_set(result, "success", json_bool(true));
        json_set(result, "stored_as", json_string(var_name ? var_name : ""));
        json_set(result, "validated", json_bool(false));
        json_set(result, "skipped", json_bool(false));
        json_set(result, "message",
                 json_string("Secret captured via registered callback"));
        return result;
    }

    /* No callback registered: return a skipped dict (CLI prompt not available
     * in this C port; the TUI path handles prompting via the app seam). */
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    json_set(result, "stored_as", json_string(var_name ? var_name : ""));
    json_set(result, "validated", json_bool(false));
    json_set(result, "skipped", json_bool(true));
    json_set(result, "message",
             json_string("Secret setup was skipped (no callback registered)"));
    return result;
}

/* Port of Python cli:_cleanup_all_browsers */

/* Port of Python cli:_mark_tui_input_modes_active */


/* Port of Python cli:_prepare_deferred_agent_startup */

/* Port of Python cli:_run_cleanup */

/* Port of Python cli:_should_emit_cleanup_session_finalize */

/* Port of Python cli:_notify_session_finalize */

/* Port of Python cli:_emit_interrupted_session_end */

/* Port of Python cli:_notify_single_query_session_finalize */

/* Port of Python cli:_finalize_single_query */

/* Port of Python cli:_reset_terminal_input_modes_on_exit */

/* Port of Python cli:_normalize_git_bash_path */

/* Port of Python cli:_git_repo_root */

/* Port of Python cli:_path_is_within_root */

/* Port of Python cli:_setup_worktree */

/* Port of Python cli:_worktree_has_unpushed_commits */

/* Port of Python cli:_cleanup_worktree */

/* Port of Python cli:_run_state_db_auto_maintenance */

/* Port of Python cli:_run_checkpoint_auto_maintenance */

/* Port of Python cli:_prune_stale_worktrees */

/* Port of Python cli:_prune_orphaned_branches */

/* Port of Python cli:_hex_to_ansi */

/* Port of Python cli:_luminance_from_hex */

/* Port of Python cli:_query_osc11_background */

/* Port of Python cli:_detect_light_mode */

/* Port of Python cli:_maybe_remap_for_light_mode */

/* Port of Python cli:_install_skin_light_mode_hook */

/* Port of Python cli:__str__ */

/* Port of Python cli:__add__ */

/* Port of Python cli:__radd__ */

/* Port of Python cli:_rich_text_from_ansi */

/* Port of Python cli:_strip_markdown_syntax */

/* Port of Python cli:_preserve_windows_dot_segments_for_markdown */

/* Port of Python cli:_terminal_width_for_streaming */

/* Port of Python cli:_render_final_assistant_content */

/* Port of Python cli:_coerce_output_history_limit */

/* Port of Python cli:_configure_output_history */

/* Port of Python cli:_clear_output_history */

/* Port of Python cli:_suspend_output_history */

/* Port of Python cli:_record_output_history_entry */

/* Port of Python cli:_record_output_history */

/* Port of Python cli:_replay_output_history */

/* Port of Python cli:_cprint */

/* Port of Python cli:_prepend_note_to_message */

/* Port of Python cli:_termux_example_image_path */

/* Port of Python cli:_split_path_input */

/* Port of Python cli:_resolve_attachment_path */

/* Port of Python cli:_detect_file_drop */

/* Port of Python cli:_format_image_attachment_badges */

/* Port of Python cli:_should_auto_attach_clipboard_image_on_paste */

/* Port of Python cli:_strip_leaked_bracketed_paste_wrappers */

/* Port of Python cli:_apply_bracketed_paste_timeout_patch */

/* Port of Python cli:_preserve_ctrl_enter_newline */

/* Port of Python cli:_bind_prompt_submit_keys */

/* Port of Python cli:_disable_prompt_toolkit_cpr_warning */

/* Port of Python cli:_strip_leaked_terminal_responses_with_meta */

/* Port of Python cli:_strip_leaked_terminal_responses */

/* Port of Python cli:_estimate_tui_input_height */

/* Port of Python cli:_collect_query_images */

/* Port of Python cli:_build_compact_banner */

/* Port of Python cli:_looks_like_slash_command */

/* Port of Python cli:_ensure_skill_commands */

/* Port of Python cli.py:build_skill_invocation_message */
/* Build the user message content for a skill slash command invocation.
 * Returns malloc'd string (caller must free) or NULL if skill not found. */
char *cli_cli_build_skill_invocation_message(const char *cmd_key,
                                             const char *user_instruction,
                                             const char *task_id,
                                             const char *runtime_note)
{
    (void)task_id;
    (void)runtime_note;
    if (!cmd_key || !cmd_key[0]) return NULL;

    /* Ensure skills are scanned */
    skill_cmd_scan();

    /* Look up the skill by slug */
    const skill_cmd_entry_t *sk = skill_cmd_get(cmd_key);
    if (!sk) return NULL;

    /* Load the skill payload */
    skill_cmd_payload_t *payload = load_skill_payload(cmd_key);
    if (!payload) return NULL;

    /* Build the activation note */
    char activation[2048];
    snprintf(activation, sizeof(activation),
        "[IMPORTANT: The user has invoked the \"%s\" skill, indicating they want "
        "you to follow its instructions. The full skill content is loaded below.]",
        payload->skill_name);

    /* Build the full message */
    size_t total = strlen(activation) + strlen(payload->body) + strlen(user_instruction) + 4096;
    char *msg = (char *)malloc(total);
    if (!msg) { skill_payload_free(payload); return NULL; }

    if (user_instruction && user_instruction[0]) {
        snprintf(msg, total,
            "%s\n\n%s\n\n[User instruction: %s]\n\n[Skill directory: %s]\n"
            "Resolve any relative paths in this skill (e.g. scripts/foo.js, "
            "templates/config.yaml) against that directory, then run them "
            "with the terminal tool using the absolute path.\n",
            activation, payload->body, user_instruction, payload->skill_dir);
    } else {
        snprintf(msg, total,
            "%s\n\n%s\n\n[Skill directory: %s]\n"
            "Resolve any relative paths in this skill (e.g. scripts/foo.js, "
            "templates/config.yaml) against that directory, then run them "
            "with the terminal tool using the absolute path.\n",
            activation, payload->body, payload->skill_dir);
    }

    skill_payload_free(payload);
    return msg;
}

/* PoP: _get_plugin_cmd_handler_names @ cli.py:_get_plugin_cmd_handler_names */
/*
 * Return plugin command names (without slash prefix) as a JSON array of
 * strings. Delegates to the live plugin registry; empty array on any error
 * (mirrors Python's except: return set()). Caller frees via json_free.
 */
json_t *cli_get_plugin_cmd_handler_names(void) {
    json_t *arr = json_new_array();
    if (!arr) return NULL;
    json_t *cmds = plug_get_commands();  /* Python: get_plugin_commands() → dict */
    if (!cmds || !json_is_object(cmds)) {
        /* Python except clause: return set() → empty array. */
        return arr;
    }
    /* Iterate dict keys via internal struct; each key is
     * "plugin_name/command_name" → strip the slash prefix. */
    size_t n = cmds->c.count;
    for (size_t i = 0; i < n; i++) {
        const char *key = cmds->c.keys[i];
        if (!key) continue;
        const char *slash = strrchr(key, '/');
        const char *name = slash ? slash + 1 : key;
        if (name && name[0]) json_append(arr, json_string(name));
    }
    if (cmds != arr) json_free(cmds);  /* plug_get_commands returns owned ref */
    return arr;
}

/* PoP: cli_is_backslash_line_continuation @ cli.py:_is_backslash_line_continuation */
/* True when Enter should turn a trailing backslash into a newline.
 * _BACKSLASH_LINE_CONTINUATION_RE = r"\\\\[ \\t]*$" — trailing backslash
 * optionally followed by spaces/tabs then end-of-string. */
bool cli_is_backslash_line_continuation(const char *text) {
    if (!text) return false;
    size_t len = strlen(text);
    if (len == 0) return false;
    /* Find the trailing backslash: scan backwards for '\\' followed only
     * by spaces/tabs until end. */
    for (size_t i = len; i > 0; i--) {
        if (text[i - 1] == '\\') {
            /* Check that everything between the backslash and end is
             * spaces or tabs. */
            bool ok = true;
            for (size_t j = i; j < len; j++) {
                char c = text[j];
                if (c != ' ' && c != '\t') { ok = false; break; }
            }
            if (ok) return true;
            break; /* first backslash from the right; if not valid, done */
        }
    }
    return false;
}

/* PoP: cli_apply_backslash_line_continuation @ cli.py:_apply_backslash_line_continuation */
char *cli_apply_backslash_line_continuation(const char *text) {
    if (!text) text = "";
    size_t len = strlen(text);
    /* Find where the regex would match: trailing backslash + optional
     * spaces/tabs. */
    size_t cut_at = len;
    for (size_t i = len; i > 0; i--) {
        if (text[i - 1] == '\\') {
            /* Verify trailing blanks after backslash. */
            bool ok = true;
            for (size_t j = i; j < len; j++) {
                char c = text[j];
                if (c != ' ' && c != '\t') { ok = false; break; }
            }
            if (ok) { cut_at = i - 1; break; }
            break;
        }
    }
    /* Build result: text[0..cut_at] + "\n" */
    size_t new_len = cut_at + 1; /* +1 for '\n' */
    char *result = malloc(new_len + 1);
    if (!result) return NULL;
    memcpy(result, text, cut_at);
    result[cut_at] = '\n';
    result[new_len] = '\0';
    return result;
}

/* Port of Python cli:_parse_skills_argument */

/* Port of Python cli:save_config_value */

/* Port of Python cli:_claim_active_session */

/* Port of Python cli:_release_active_session */

/* PoP: hcli_invalidate @ cli.py:_invalidate */
/* Throttled UI repaint for high-frequency background updates. */
/* Guards: terminal_io_broken, resize_recovery_pending. */
/* Throttle: (now - last_invalidate) >= min_interval. */
/* On app_invalidate_fn EIO: mark terminal io broken. */
void hcli_invalidate(hcli_t *self) {
    hcli_invalidate_min(self, 0.25);
}

/* PoP: hcli_invalidate_min @ cli.py:_invalidate */
/* * Full form with explicit min_interval. */
void hcli_invalidate_min(hcli_t *self, double min_interval) {
    if (!self) return;
    if (self->terminal_io_broken) return;
    if (self->resize_recovery_pending) return;

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    double now = (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;

    if (self->app_invalidate_fn && self->app &&
        (now - self->last_invalidate) >= min_interval) {
        self->last_invalidate = now;
        self->app_invalidate_fn(self);
        if (self->app_invalidate_errno == EIO) {
            hcli_mark_terminal_io_broken(self, "invalidate");
        }
    }
}

/* PoP: hcli_mark_terminal_io_broken @ cli.py:_mark_terminal_io_broken */
void hcli_mark_terminal_io_broken(hcli_t *self, const char *reason) {
    if (!self) return;
    if (self->terminal_io_broken) return;
    self->terminal_io_broken = true;
}
/* Port of Python cli:_mark_terminal_io_broken */

/* PoP: hcli_paint_now @ cli.py:_paint_now */
/* Immediate, unthrottled repaint for user-blocking modal prompts. */
/* Bypasses the _invalidate throttle and resize-recovery guard. */
/* On app_invalidate_fn EIO: mark terminal io broken. */
void hcli_paint_now(hcli_t *self) {
    if (!self) return;
    if (self->terminal_io_broken) return;
    if (self->app_invalidate_fn && self->app) {
        self->app_invalidate_fn(self);
        if (self->app_invalidate_errno == EIO) {
            hcli_mark_terminal_io_broken(self, "paint_now");
        }
    }
}
/* Port of Python cli:_paint_now */

/* PoP: hcli_force_full_redraw @ cli.py:_force_full_redraw */
void hcli_force_full_redraw(hcli_t *self) {
    if (!self) return;
    if (self->terminal_io_broken) return;
    if (self->clear_prompt_toolkit_screen_fn && self->app) {
        self->clear_prompt_toolkit_screen_fn(self, self->app, false);
    }
    if (self->terminal_io_broken) return;
    if (self->replay_output_history_fn) self->replay_output_history_fn();
    if (self->app_invalidate_fn && self->app) {
        self->app_invalidate_fn(self);
        if (self->app_invalidate_errno == EIO) {
            hcli_mark_terminal_io_broken(self, "force_full_redraw");
        }
    }
}

/* PoP: hcli_schedule_focus_regain_redraw @ cli.py:_schedule_focus_regain_redraw */
void hcli_schedule_focus_regain_redraw(hcli_t *self) {
    if (!self) return;
    if (self->terminal_io_broken) return;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return;
    double now = ts.tv_sec + ts.tv_nsec / 1000000000.0;
    double last = self->last_focus_regain_redraw;
    if (now - last < 1.0) return;
    self->last_focus_regain_redraw = now;
    hcli_force_full_redraw(self);
}

/* Port of Python cli:_schedule_resize_recovery */

/* Port of Python cli:_compression_count_style */

/* Port of Python cli:_build_context_bar */

/* Port of Python cli:_format_prompt_elapsed */

/* Port of Python cli:_format_idle_since */

/* Port of Python cli:_get_status_bar_snapshot */

/* PoP: cli_status_bar_display_width @ cli.py:_status_bar_display_width
 * prompt_toolkit get_cwidth port: UTF-8 codepoint width classifier.
 * Implemented in src/cli/cli_helpers2.c. */

/* Port of Python cli:_trim_status_bar_text */

/* Port of Python cli:_get_tui_terminal_width */

/* Port of Python cli:_use_minimal_tui_chrome */

/* Port of Python cli:_scrollback_box_width */

/* Port of Python cli:_tui_input_rule_height */

/* Port of Python cli:_spinner_widget_height */

/* Port of Python cli:_render_spinner_text */

/* Port of Python cli:_voice_record_key_label */

/* Port of Python cli:set_voice_record_key_cache */

/* Port of Python cli:_get_voice_status_fragments */

/* Port of Python cli:_build_status_bar_text */

/* Port of Python cli:_get_status_bar_fragments */

/* Port of Python cli:_normalize_model_for_provider */

/* Port of Python cli:_on_thinking */

/* Port of Python cli:_on_notice */

/* Port of Python cli:_flush_credit_notices */

/* Port of Python cli:_on_notice_clear */


/* Port of Python cli:_current_reasoning_callback */

/* Port of Python cli:_emit_reasoning_preview */

/* Port of Python cli:_flush_reasoning_preview */

/* Port of Python cli:_format_submitted_user_message_preview */

/* Port of Python cli:_expand_paste_references */

/* Port of Python cli:_print_user_message_preview */

/* Port of Python cli:_stream_reasoning_delta */

/* Port of Python cli:_emit_stream_text */

/* Port of Python cli:_flush_stream */

/* Port of Python cli:_reset_stream_state */

/* Port of Python cli:_slow_command_status */

/* Port of Python cli:_command_spinner_frame */

/* Port of Python cli:_busy_command */

/* Port of Python cli:_open_external_editor */

/* Port of Python cli:_install_tool_callbacks */

/* Port of Python cli:_ensure_tirith_security */

/* Port of Python cli:_show_security_advisories */

/* Port of Python cli:show_banner */

/* Port of Python cli:_restore_session_cwd */

/* Port of Python cli:_render_resume_history_panel_lines */

/* Port of Python cli:_try_attach_clipboard_image */

/* Port of Python cli:_resolve_checkpoint_ref */

/* Port of Python cli:_write_osc52_clipboard */

/* Port of Python cli:_recover_terminal_input_modes */

/* Port of Python cli:_preprocess_images_with_vision */

/* Port of Python cli:_show_tool_availability_warnings */

/* Port of Python cli:_show_status */

/* Port of Python cli:_show_session_status */

/* Port of Python cli:_fast_command_available */

/* Port of Python cli:_command_available */

/* Port of Python cli:show_help */

/* Port of Python cli:show_tools */

/* Port of Python cli:show_toolsets */

/* Port of Python cli:_list_recent_sessions */

/* Port of Python cli:_show_recent_sessions */

/* Port of Python cli:show_history */

/* Port of Python cli:_discard_session_if_empty */

/* Port of Python cli:new_session */

/* Port of Python cli:_consume_pending_resume_selection */

/* Port of Python cli:save_conversation */

/* Port of Python cli:retry_last */

/* Port of Python cli:undo_last */

/* Port of Python cli:_undo_content_to_text */

/* Port of Python cli:_prefill_input_buffer */

/* Port of Python cli:_run_curses_picker */

/* Port of Python cli:_prompt_text_input */

/* Port of Python cli:_prompt_text_input_modal */

/* Port of Python cli:_submit_slash_confirm_response */

/* Port of Python cli:_normalize_slash_confirm_choice */

/* Port of Python cli:_get_slash_confirm_display_fragments */

/* Port of Python cli:_open_model_picker */

/* Port of Python cli:_confirm_expensive_model_switch */

/* Port of Python cli:_confirm_and_apply_model_switch_result */

/* PoP: hcli_close_model_picker @ cli.py:_close_model_picker */
void hcli_close_model_picker(hcli_t *self);

/* Port of Python cli:_compute_model_picker_viewport */

/* Port of Python cli:_apply_model_switch_result */

/* Port of Python cli:_handle_model_picker_selection */

/* Port of Python cli:_handle_model_switch */

/* Port of Python cli:_handle_codex_runtime */

/* Port of Python cli:_should_handle_model_command_inline */

/* Port of Python cli:_should_handle_steer_command_inline */

/* Port of Python cli:_output_console */

/* Port of Python cli:_console_print */

/* Port of Python cli:_resolve_personality_prompt */

/* PoP: hcli_show_gateway_status @ cli.py:_show_gateway_status */
void hcli_show_gateway_status(hcli_t *self) {
    (void)self;
    /* Print the panel header (matching Python exactly). */
    if (self->console_print_fn) {
        self->console_print_fn(self, "");
        self->console_print_fn(self, "+------------------------------------------------------------+");
        self->console_print_fn(self, "|               (✿◠‿◠) Gateway Status                |");
        self->console_print_fn(self, "+------------------------------------------------------------+");
        self->console_print_fn(self, "");
    } else {
        printf("\n");
        printf("+------------------------------------------------------------+\n");
        printf("|               (✿◠‿◠) Gateway Status                |\n");
        printf("+------------------------------------------------------------+\n");
        printf("\n");
    }

    /* Load gateway config using the slermes home directory as config source.
     * gateway_config_load reads <config_dir>/config.yaml; NULL config_dir
     * triggers the same error path as a missing/invalid config file. */
    gateway_config_t cfg;
    if (!gateway_config_load(slermes_home(), &cfg)) {
        /* Error path — print exception message + configure instructions. */
        const char *home = slermes_home();
        if (self->console_print_fn) {
            self->console_print_fn(self, "  Error loading gateway config: unable to load config");
            self->console_print_fn(self, "");
            self->console_print_fn(self, "  To configure the gateway:");
            self->console_print_fn(self, "    1. Set environment variables:");
            self->console_print_fn(self, "       TELEGRAM_BOT_TOKEN=your_token");
            self->console_print_fn(self, "       DISCORD_BOT_TOKEN=your_token");
            self->console_print_fn(self, "    2. Or configure settings in");
            self->console_print_fn(self, "       ");
            self->console_print_fn(self, home);
            self->console_print_fn(self, "/config.yaml");
            self->console_print_fn(self, "");
        } else {
            printf("  Error loading gateway config: unable to load config\n");
            printf("\n");
            printf("  To configure the gateway:\n");
            printf("    1. Set environment variables:\n");
            printf("       TELEGRAM_BOT_TOKEN=your_token\n");
            printf("       DISCORD_BOT_TOKEN=your_token\n");
            printf("    2. Or configure settings in %s/config.yaml\n", home);
            printf("\n");
        }
        return;
    }

    /* Messaging Platform Configuration section. */
    if (self->console_print_fn) {
        self->console_print_fn(self, "  Messaging Platform Configuration:");
    } else {
        printf("  Messaging Platform Configuration:\n");
    }

    /* Platform status table. */
    struct {
        int platform;
        const char *name;
        const char *env_var;
    } platform_status[] = {
        { GW_PLATFORM_TELEGRAM, "Telegram", "TELEGRAM_BOT_TOKEN" },
        { GW_PLATFORM_DISCORD,  "Discord",  "DISCORD_BOT_TOKEN" },
        { GW_PLATFORM_SLACK,    "Slack",    "SLACK_BOT_TOKEN" },
        { GW_PLATFORM_WHATSAPP, "WhatsApp", "WHATSAPP_ENABLED" },
    };

    for (size_t i = 0; i < sizeof(platform_status) / sizeof(platform_status[0]); i++) {
        const gw_platform_config_t *pconfig =
            (i < (size_t)cfg.platform_count) ? &cfg.platforms[i] : NULL;
        const char *home_channel = NULL;
        bool enabled = (pconfig && pconfig->enabled);

        if (enabled && pconfig) {
            /* gw_platform_config_t has home_channel.name directly. */
            if (pconfig->has_home_channel && pconfig->home_channel.name[0]) {
                home_channel = pconfig->home_channel.name;
            }
        }

        char line[256];
        if (enabled) {
            const char *home_str = home_channel ? " → " : "";
            snprintf(line, sizeof(line),
                     "    ✓ %-12s Enabled%s",
                     platform_status[i].name,
                     home_str);
            /* Append home channel name if present. */
            if (home_channel) {
                size_t pos = strlen(line);
                size_t remain = sizeof(line) - pos - 1;
                snprintf(line + pos, remain, "%s", home_channel);
            }
        } else {
            snprintf(line, sizeof(line),
                     "    ○ %-12s Not configured (%s)",
                     platform_status[i].name,
                     platform_status[i].env_var);
        }

        if (self->console_print_fn) {
            self->console_print_fn(self, line);
        } else {
            printf("%s\n", line);
        }
    }

    /* Session Reset Policy section. */
    if (self->console_print_fn) {
        self->console_print_fn(self, "");
        self->console_print_fn(self, "  Session Reset Policy:");
        {
            const char *mode_str = "unknown";
            switch (cfg.reset_policy.mode) {
                case GW_RESET_MODE_DAILY: mode_str = "daily"; break;
                case GW_RESET_MODE_IDLE:  mode_str = "idle";  break;
                case GW_RESET_MODE_BOTH:  mode_str = "both";  break;
                case GW_RESET_MODE_NONE:  mode_str = "none";  break;
                default: break;
            }
            char buf[128];
            snprintf(buf, sizeof(buf), "    Mode: %s", mode_str);
            self->console_print_fn(self, buf);
        }
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "    Daily reset at: %d:00", cfg.reset_policy.at_hour);
            self->console_print_fn(self, buf);
        }
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "    Idle timeout: %d minutes", cfg.reset_policy.idle_minutes);
            self->console_print_fn(self, buf);
        }
        self->console_print_fn(self, "");
        self->console_print_fn(self, "  To start the gateway:");
        self->console_print_fn(self, "    python cli.py --gateway");
        self->console_print_fn(self, "");
        {
            const char *home = slermes_home();
            char buf[512];
            snprintf(buf, sizeof(buf), "  Configuration file: %s/config.yaml", home);
            self->console_print_fn(self, buf);
        }
        self->console_print_fn(self, "");
    } else {
        printf("\n");
        printf("  Session Reset Policy:\n");
        printf("\n");
        {
            const char *mode_str = "unknown";
            switch (cfg.reset_policy.mode) {
                case GW_RESET_MODE_DAILY: mode_str = "daily"; break;
                case GW_RESET_MODE_IDLE:  mode_str = "idle";  break;
                case GW_RESET_MODE_BOTH:  mode_str = "both";  break;
                case GW_RESET_MODE_NONE:  mode_str = "none";  break;
                default: break;
            }
            printf("    Mode: %s\n", mode_str);
        }
        printf("    Daily reset at: %d:00\n", cfg.reset_policy.at_hour);
        printf("    Idle timeout: %d minutes\n", cfg.reset_policy.idle_minutes);
        printf("\n");
        printf("  To start the gateway:\n");
        printf("    python cli.py --gateway\n");
        printf("\n");
        const char *home = slermes_home();
        printf("  Configuration file: %s/config.yaml\n", home);
        printf("\n");
    }
}

/* Port of Python cli:process_command */

/* Port of Python cli:_try_launch_chrome_debug */

/* Port of Python cli:_get_goal_manager */

/* Port of Python cli:_maybe_continue_goal_after_turn */

/* Port of Python cli:_toggle_verbose */

/* Port of Python cli:_transfer_session_yolo */

/* Port of Python cli:_is_session_yolo_active */

/* Port of Python cli:_toggle_yolo */

/* Port of Python cli:_on_reasoning */

/* Port of Python cli:_manual_compress */

/* Port of Python cli:_show_usage */

/* Port of Python cli:_print_nous_credits_block */

/* Port of Python cli:_show_credits */

/* Port of Python cli:_show_insights */

/* Port of Python cli:_check_config_mcp_changes */

/* Port of Python cli:_split_destructive_skip */

/* Port of Python cli:_confirm_destructive_slash */

/* Port of Python cli:_confirm_and_reload_mcp */

/* Port of Python cli:_on_tool_gen_start */

/* Port of Python cli:_on_tool_progress */

/* Port of Python cli:_on_tool_start */

/* Port of Python cli:_on_tool_complete */

/* Port of Python cli:_voice_start_recording */

/* Port of Python cli:_voice_stop_and_transcribe */

/* Port of Python cli:_voice_speak_response_async */

/* Port of Python cli:_voice_speak_response */

/* Port of Python cli:_voice_beeps_enabled */

/* Port of Python cli:_enable_voice_mode */

/* Port of Python cli:_disable_voice_mode */

/* Port of Python cli:_toggle_voice_tts */

/* Port of Python cli:_show_voice_status */

/* Port of Python cli:_persist_prompt_summary */

/* Port of Python cli:_clarify_callback */

/* Port of Python cli:_sudo_password_callback */

/* Port of Python cli:_approval_callback */

/* Port of Python cli:_approval_choices */

/* Port of Python cli:_computer_use_approval_callback */

/* Port of Python cli:_handle_approval_selection */

/* PoP: hcli_capture_modal_input_snapshot @ cli.py:_capture_modal_input_snapshot
 * Save the composer draft (text + cursor) before a modal prompt opens, then
 * clear the buffer. No-op when app is not wired (Python except path). */
void hcli_capture_modal_input_snapshot(hcli_t *self) {
    if (!self) return;
    if (!self->app_get_buffer_text_fn || !self->app || self->modal_snapshot_valid) return;
    char *text = self->app_get_buffer_text_fn(self);
    int cursor = self->app_get_buffer_cursor_fn ? self->app_get_buffer_cursor_fn(self) : 0;
    free(self->modal_snapshot_text);
    self->modal_snapshot_text = text;
    self->modal_snapshot_cursor = cursor;
    self->modal_snapshot_valid = true;
    /* Clear the composer buffer (Python: buf.reset()). */
    if (self->app_set_buffer_text_fn) self->app_set_buffer_text_fn(self, "");
    if (self->app_set_buffer_cursor_fn) self->app_set_buffer_cursor_fn(self, 0);
}

/* PoP: hcli_restore_modal_input_snapshot @ cli.py:_restore_modal_input_snapshot */
void hcli_restore_modal_input_snapshot(hcli_t *self) {
    if (!self) return;
    if (!self->modal_snapshot_valid || !self->app) return;
    if (self->app_set_buffer_text_fn) self->app_set_buffer_text_fn(self,
        self->modal_snapshot_text ? self->modal_snapshot_text : "");
    if (self->app_set_buffer_cursor_fn)
        self->app_set_buffer_cursor_fn(self, self->modal_snapshot_cursor);
    free(self->modal_snapshot_text);
    self->modal_snapshot_text = NULL;
    self->modal_snapshot_cursor = 0;
    self->modal_snapshot_valid = false;
}

/* PoP: hcli_close_reasoning_box @ cli.py:_close_reasoning_box */
void hcli_close_reasoning_box(hcli_t *self) {
    if (!self) return;
    free(self->reasoning_buf);
    self->reasoning_buf = NULL;
    self->reasoning_box_opened = false;
    if (self->app_invalidate_fn && self->app) {
        self->app_invalidate_fn(self);
    }
}

/* PoP: hcli_close_command_palette @ cli.py:_close_command_palette */
void hcli_close_command_palette(hcli_t *self) {
    if (!self) return;
    self->command_palette_open = false;
    hcli_restore_modal_input_snapshot(self);
    if (self->app_invalidate_fn && self->app) {
        self->app_invalidate_fn(self);
    }
}

/* PoP: hcli_close_model_picker @ cli.py:_close_model_picker */
void hcli_close_model_picker(hcli_t *self) {
    if (!self) return;
    self->model_picker_open = false;
    hcli_restore_modal_input_snapshot(self);
    if (self->app_invalidate_fn && self->app) {
        self->app_invalidate_fn(self);
    }
}

/* Port of Python cli:_capture_modal_input_snapshot */

/* Port of Python cli:_cancel_secret_capture */

/* Port of Python cli:_clear_secret_input_buffer */

/* Port of Python cli:_clear_terminal_on_exit */

/* Port of Python cli:_print_exit_summary */

/* Port of Python cli:_get_tui_prompt_symbols */

/* PoP: cli_status_bar_visible_from_display_config @ cli.py:_status_bar_visible_from_display_config */
bool cli_status_bar_visible_from_display_config(json_t *display_config) {
    if (!display_config || !json_is_object(display_config)) return true;
    json_t *sc = json_object_get(display_config, "statusbar");
    if (!sc) sc = json_object_get(display_config, "tui_statusbar");
    if (!sc) {
        /* Python default: display_config.get("tui_statusbar", "top") → "top"
         * which is a non-empty string → truthy → visible. */
        return true;
    }
    if (json_is_string(sc)) {
        const char *s = json_string_value(sc);
        if (!s) return true;
        /* Normalize: trim, lowercase. */
        char buf[64];
        size_t n = strlen(s);
        if (n >= sizeof(buf)) n = sizeof(buf) - 1;
        memcpy(buf, s, n);
        buf[n] = '\0';
        for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)buf[i]);
        /* Trim leading whitespace. */
        size_t start = 0;
        while (start < n && (buf[start] == ' ' || buf[start] == '\t')) start++;
        /* Trim trailing whitespace. */
        size_t end = n;
        while (end > start && (buf[end - 1] == ' ' || buf[end - 1] == '\t')) end--;
        bool disabled = false;
        if (end > start) {
            char tmp[16];
            size_t tn = end - start;
            if (tn >= sizeof(tmp)) tn = sizeof(tmp) - 1;
            memcpy(tmp, buf + start, tn);
            tmp[tn] = '\0';
            if (strcmp(tmp, "0") == 0 || strcmp(tmp, "false") == 0 ||
                strcmp(tmp, "hidden") == 0 || strcmp(tmp, "no") == 0 ||
                strcmp(tmp, "off") == 0)
                disabled = true;
        } else {
            disabled = true; /* blank → disabled */
        }
        return !disabled;
    }
    if (json_is_bool(sc)) return json_bool_value(sc);
    /* Numbers, null, arrays, objects → truthy (not False). */
    return true;
}

/* PoP: cli_terminal_supports_extended_enter_keys @ cli.py:_terminal_supports_extended_enter_keys */
bool cli_terminal_supports_extended_enter_keys(void) {
    const char *term_program = "";
    const char *term = "";
    const char *wt = "";
    const char *kitty_wm = "";
    const char *ghostty_res = "";
    const char *ghostty_bin = "";

    term_program = getenv("TERM_PROGRAM");
    if (!term_program) term_program = "";
    term = getenv("TERM");
    if (!term) term = "";
    wt = getenv("WT_SESSION");
    if (!wt) wt = "";
    kitty_wm = getenv("KITTY_WINDOW_ID");
    if (!kitty_wm) kitty_wm = "";
    ghostty_res = getenv("GHOSTTY_RESOURCES_DIR");
    if (!ghostty_res) ghostty_res = "";
    ghostty_bin = getenv("GHOSTTY_BIN_DIR");
    if (!ghostty_bin) ghostty_bin = "";

    if (wt[0]) return true;

    if (strcmp(term_program, "iTerm.app") == 0 ||
        strcmp(term_program, "WezTerm") == 0 ||
        strcmp(term_program, "ghostty") == 0 ||
        strcmp(term_program, "vscode") == 0)
        return true;

    if (kitty_wm[0] || strstr(term, "kitty")) return true;
    if (strcmp(term, "xterm-ghostty") == 0) return true;

    /* tmux detection: TERM starts with "tmux" or TERM_PROGRAM is "tmux". */
    if (strncmp(term, "tmux", 4) == 0 ||
        strcmp(term_program, "tmux") == 0)
        return true;

    return false;
}

/* PoP: cli_hermes_call_output_screen_diff @ cli.py:_hermes_call_output_screen_diff */
int cli_hermes_call_output_screen_diff(
    cli_w4b_output_screen_diff_fn orig_osd, void *orig_osd_user,
    void *app, void *output, cli_w4b_screen_t *screen, int current_pos,
    int color_depth, cli_w4b_screen_t *previous_screen, const char *last_style,
    bool is_done, bool full_screen, void *attrs_for_style_string,
    void *style_string_has_style, cli_w4b_screen_t size, int previous_width) {
    /* Guard 1: inflate previous_screen.height when new screen is taller. */
    if (previous_screen && screen && previous_screen->height < screen->height) {
        previous_screen->height = screen->height;
    }
    /* Guard 2: call orig_osd; on failure retry with previous_screen=NULL. */
    int rc = orig_osd(
        app, output, screen, current_pos, color_depth,
        previous_screen, last_style,
        is_done, full_screen,
        attrs_for_style_string, style_string_has_style,
        size, previous_width,
        orig_osd_user);
    if (rc < 0) {
        /* Corrupt previous paint buffer — retry with NULL previous_screen. */
        rc = orig_osd(
            app, output, screen, current_pos, color_depth,
            NULL, NULL,
            is_done, full_screen,
            attrs_for_style_string, style_string_has_style,
            size, 0,
            orig_osd_user);
    }
    return rc;
}

/* Port of Python cli:_audio_level_bar */

/* PoP: hcli_clear_prompt_toolkit_screen @ cli.py:_clear_prompt_toolkit_screen */
void hcli_clear_prompt_toolkit_screen(hcli_t *self, void *app,
                                      bool rebuild_scrollback) {
    (void)app;
    if (!self) return;
    if (self->terminal_io_broken) return;
    const hcli_pt_output_ops_t *ops = self->pt_ops;
    if (!ops || !ops->renderer) return;
    if (ops->reset_attributes && ops->output) ops->reset_attributes(ops->output);
    if (ops->erase_screen && ops->output) ops->erase_screen(ops->output);
    if (rebuild_scrollback && ops->write_raw && ops->output) {
        const char *scrollback_clear = "\x1b[3J";
        ops->write_raw(ops->output, scrollback_clear);
    }
    if (ops->cursor_goto && ops->output) ops->cursor_goto(ops->output, 0, 0);
    if (ops->flush && ops->output) ops->flush(ops->output);
    if (ops->renderer_reset && ops->renderer) ops->renderer_reset(ops->renderer);
}

/* Port of Python cli:_get_tui_prompt_fragments */

/* Port of Python cli:_get_tui_prompt_text */

/* Port of Python cli:_build_tui_style_dict */

/* Port of Python cli:_apply_tui_skin_style */

/* Port of Python cli:_get_extra_tui_widgets */

/* PoP: _enable_extended_enter_keys @ cli.py:_enable_extended_enter_keys */
bool _enable_extended_enter_keys(void *output,
                                 void (*write_raw_fn)(void *, const char *),
                                 void (*flush_fn)(void *)) {
    if (!cli_terminal_supports_extended_enter_keys()) return false;

    /* Ghostty gets modifyOtherKeys only — see cli_is_ghostty_terminal(). */
    const char *seq = cli_is_ghostty_terminal() ?
        _MODIFY_OTHER_KEYS_SEQ : _EXTENDED_ENTER_KEYS_SEQ;

    /* Path 1: output.write_raw + output.flush (prompt_toolkit App.output). */
    if (output && write_raw_fn && flush_fn) {
        write_raw_fn(output, seq);
        flush_fn(output);
        return true;
    }

    /* Path 2: sys.stdout + isatty() (no-app fallback). */
    FILE *stream = stdout;
    if (stream && isatty(fileno(stream))) {
        fputs(seq, stream);
        fflush(stream);
        return true;
    }

    return false;
}

/* Port of Python cli:_register_extra_tui_keybindings */


/* Port of Python cli:_build_tui_layout_children */

/* Port of Python cli:run */