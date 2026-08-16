/* ================================================================
 * tool_guardrails.c — Per-turn tool call loop detection.
 * Port of Python agent/tool_guardrails.py (475 lines).
 *
 * MIT License — WuBu Slermes Project
 * ================================================================ */

/* strcasestr and friends are GNU extensions — musl (alpine) needs
 * _GNU_SOURCE to declare them; glibc exposes them by default. */
#define _GNU_SOURCE
#include "hermes_tool_guardrails.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "hermes_crypto.h"

/* ================================================================
 *  Default config
 * ================================================================ */

#define DEFAULT_WARN_AFTER_EXACT       2
#define DEFAULT_BLOCK_AFTER_EXACT      5
#define DEFAULT_WARN_AFTER_SAME_TOOL   3
#define DEFAULT_HALT_AFTER_SAME_TOOL   8
#define DEFAULT_WARN_AFTER_NO_PROGRESS 2
#define DEFAULT_BLOCK_AFTER_NO_PROGRESS 5

/* ================================================================
 *  Helper: fast string hash (djb2)
 * ================================================================ */

uint32_t tool_guardrail_hash(const char *str)
{
    if (!str) return 0;
    uint32_t hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + (unsigned char)c;
    return hash;
}

/* ================================================================
 *  Idempotent / mutating tool sets
 * ================================================================ */

static const char *IDEMPOTENT_TOOLS[] = {
    "read_file",
    "search_files",
    "web_search",
    "web_extract",
    "session_search",
    "browser_snapshot",
    "browser_console",
    "browser_get_images",
    "mcp_filesystem_read_file",
    "mcp_filesystem_list_directory",
    NULL
};

static const char *MUTATING_TOOLS[] = {
    "terminal",
    "execute_code",
    "write_file",
    "patch",
    "todo",
    "memory",
    "skill_manage",
    "browser_click",
    "browser_type",
    "browser_press",
    "browser_scroll",
    "browser_navigate",
    "send_message",
    "cronjob",
    "delegate_task",
    "process",
    NULL
};

static bool in_set(const char *tool_name, const char **set)
{
    if (!tool_name) return false;
    for (int i = 0; set[i]; i++) {
        if (strcmp(tool_name, set[i]) == 0)
            return true;
    }
    return false;
}

/* Port of Python agent/tool_guardrails.py:_is_idempotent */
/* Check if a tool is idempotent (safe to retry). */
bool tool_guardrail_is_idempotent(const char *tool_name)
{
    if (!tool_name) return false;
    /* A tool is idempotent if it doesn't modify external state.
     * Check against the known idempotent tool set. */
    return in_set(tool_name, IDEMPOTENT_TOOLS);
}
/* Port of Python agent/tool_guardrails.py: (mutating check, part of _is_idempotent) */


bool tool_guardrail_is_mutating(const char *tool_name)
{
    return in_set(tool_name, MUTATING_TOOLS);
}

/* ================================================================
 *  Tool result classification (ported from agent/tool_result_classification.py
 *  and agent/tool_guardrails.py)
 * ================================================================ */

/* File-mutation tool names that prove a write landed on success. */
static const char *file_mutating_tools[] = {"write_file", "patch", NULL};

/* Port of Python: file_mutation_result_landed */
bool file_mutation_result_landed(const char *tool_name, const char *result) {
    if (!tool_name || !result) return false;

    /* Check if this is a file-mutating tool. */
    bool is_mutating = false;
    for (const char **p = file_mutating_tools; *p; p++) {
        if (strcmp(tool_name, *p) == 0) { is_mutating = true; break; }
    }
    if (!is_mutating) return false;

    /* Parse JSON result and check for success indicators. */
    char *end = NULL;
    json_node_t *data = json_parse(result, &end);
    if (!data || data->type != JSON_OBJECT) {
        if (data) json_free(data);
        return false;
    }
    json_node_t *err_node = json_object_get(data, "error");
    if (err_node) { json_free(data); return false; }

    if (strcmp(tool_name, "write_file") == 0) {
        json_node_t *bw = json_object_get(data, "bytes_written");
        json_free(data);
        return bw != NULL;
    }
    if (strcmp(tool_name, "patch") == 0) {
        json_node_t *succ = json_object_get(data, "success");
        json_free(data);
        return succ && succ->type == JSON_BOOL && succ->bool_val;
    }
    json_free(data);
    return false;
}
/* Port of Python agent/tool_guardrails.py:classify_tool_failure */


/* Classify whether a tool result represents a failure.
 * Returns true if the result indicates failure, false on success/unknown. */
bool classify_tool_failure(const char *tool_name, const char *result) {
    if (!result) return false;

    /* If the write actually landed, it's not a failure. */
    if (file_mutation_result_landed(tool_name, result))
        return false;

    /* terminal tool: check exit_code in JSON result. */
    if (tool_name && strcmp(tool_name, "terminal") == 0) {
        char *end = NULL;
        json_node_t *data = json_parse(result, &end);
        if (data && data->type == JSON_OBJECT) {
            json_node_t *ec = json_object_get(data, "exit_code");
            if (ec && ec->type == JSON_NUMBER && ec->num_val != 0) {
                json_free(data);
                return true;
            }
        }
        if (data) json_free(data);
        return false;
    }

    /* memory tool: check for "full" error. */
    if (tool_name && strcmp(tool_name, "memory") == 0) {
        char *end = NULL;
        json_node_t *data = json_parse(result, &end);
        if (data && data->type == JSON_OBJECT) {
            json_node_t *succ = json_object_get(data, "success");
            if (succ && succ->type == JSON_BOOL && !succ->bool_val) {
                json_node_t *err = json_object_get(data, "error");
                if (err && err->type == JSON_STRING &&
                    strstr(err->str_val, "exceed the limit")) {
                    json_free(data);
                    return true;
                }
            }
        }
        if (data) json_free(data);
        return false;
    }

    /* Generic: check for error/failed patterns in first 500 chars. */
    size_t check_len = strlen(result);
    if (check_len > 500) check_len = 500;
    /* Use strcasestr for case-insensitive search. */
    for (size_t i = 0; i < check_len; i++) {
        if (strncasecmp(result + i, "\"error\"", 7) == 0 ||
            strncasecmp(result + i, "\"failed\"", 8) == 0) {
            return true;
        }
    }
    if (strncasecmp(result, "Error", 5) == 0)
        return true;

    return false;
}

/* Port of Python agent/tool_guardrails.py:__init__ */

/* ================================================================
 *  Initialize / reset
 * ================================================================ */

void tool_guardrail_init(tool_guardrail_controller_t *ctrl)
{
    if (!ctrl) return;
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->warnings_enabled = true;
    ctrl->hard_stop_enabled = false;
    ctrl->exact_failure_warn_after = DEFAULT_WARN_AFTER_EXACT;
    ctrl->exact_failure_block_after = DEFAULT_BLOCK_AFTER_EXACT;
/* Port of Python agent/tool_guardrails.py:reset_for_turn */

    ctrl->same_tool_failure_warn_after = DEFAULT_WARN_AFTER_SAME_TOOL;
    ctrl->same_tool_failure_halt_after = DEFAULT_HALT_AFTER_SAME_TOOL;
    ctrl->no_progress_warn_after = DEFAULT_WARN_AFTER_NO_PROGRESS;
    ctrl->no_progress_block_after = DEFAULT_BLOCK_AFTER_NO_PROGRESS;
}

void tool_guardrail_reset(tool_guardrail_controller_t *ctrl)
{
    if (!ctrl) return;
    memset(ctrl->tracked, 0, sizeof(ctrl->tracked));
    ctrl->count = 0;
    ctrl->halt_decision_active = false;
    memset(&ctrl->halt_decision, 0, sizeof(ctrl->halt_decision));
}

/* ================================================================
 *  Find or allocate tracking slot
 * ================================================================ */

static tool_guardrail_tracked_t *find_slot(tool_guardrail_controller_t *ctrl,
                                            const char *tool_name,
                                            uint32_t args_hash)
{
    for (int i = 0; i < ctrl->count; i++) {
        if (ctrl->tracked[i].active &&
            strcmp(ctrl->tracked[i].tool_name, tool_name) == 0 &&
            ctrl->tracked[i].args_hash == args_hash)
            return &ctrl->tracked[i];
    }
    return NULL;
}

static tool_guardrail_tracked_t *alloc_slot(tool_guardrail_controller_t *ctrl)
{
    if (ctrl->count >= TOOL_GUARDRAIL_MAX_TRACKED)
        return NULL;
    tool_guardrail_tracked_t *slot = &ctrl->tracked[ctrl->count];
    memset(slot, 0, sizeof(*slot));
    slot->active = true;
    ctrl->count++;
    return slot;
}

/* ================================================================
 *  Build helper tools
 * ================================================================ */

static void set_decision(tool_guardrail_decision_t *d,
                          guardrail_action_t action,
                          const char *code,
                          const char *message,
                          const char *tool_name,
                          int count)
{
    memset(d, 0, sizeof(*d));
    d->action = action;
    if (code) snprintf(d->code, sizeof(d->code), "%s", code);
    if (message) snprintf(d->message, sizeof(d->message), "%s", message);
    if (tool_name) snprintf(d->tool_name, sizeof(d->tool_name), "%s", tool_name);
    d->count = count;
}

/* Port of Python agent/tool_guardrails.py:_tool_failure_recovery_hint(). */
/* Port of Python agent/tool_guardrails.py:_tool_tool_failure_recovery_hint */
static const char *tool_failure_recovery_hint(const char *tool_name, int count)
{
    (void)count;
    if (tool_name && strcmp(tool_name, "terminal") == 0) {
        return "For terminal failures, run a small diagnostic (pwd, ls -la), "
               "then try an absolute path, a simpler command, or a different tool.";
    }
    return "Try different arguments, a narrower query, an absolute path, "
/* Port of Python agent/tool_guardrails.py:before_call */

           "or a different tool. If the blocker is external, report it after "
           "one diagnostic attempt.";
}

/* ================================================================
 *  before_call — check before executing
 * ================================================================ */

tool_guardrail_decision_t
tool_guardrail_before_call(tool_guardrail_controller_t *ctrl,
                            const char *tool_name,
                            const char *tool_args)
{
    tool_guardrail_decision_t allow = { .action = GUARDRAIL_ALLOW };
    if (!ctrl || !tool_name) return allow;
    if (ctrl->halt_decision_active) {
        snprintf(allow.message, sizeof(allow.message),
                 "Tool call skipped due to previous halt");
        return allow;
    }
    if (!ctrl->hard_stop_enabled) {
        snprintf(allow.tool_name, sizeof(allow.tool_name), "%s", tool_name);
        return allow;
    }

    uint32_t ah = tool_guardrail_hash(tool_args);
    tool_guardrail_tracked_t *slot = find_slot(ctrl, tool_name, ah);

    if (slot) {
        int exact_count = slot->failure_count;
        if (exact_count >= ctrl->exact_failure_block_after) {
            ctrl->halt_decision_active = true;
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "Blocked %s: the same tool call failed %d times "
                     "with identical arguments. Stop retrying it unchanged.",
                     tool_name, exact_count);
            set_decision(&d, GUARDRAIL_BLOCK, "repeated_exact_failure_block",
                         msg, tool_name, exact_count);
            ctrl->halt_decision = d;
            return d;
        }

        if (tool_guardrail_is_idempotent(tool_name)) {
            tool_guardrail_tracked_t *nps = NULL;
            for (int i = 0; i < ctrl->count; i++) {
                if (ctrl->tracked[i].active &&
                    strcmp(ctrl->tracked[i].tool_name, tool_name) == 0 &&
                    ctrl->tracked[i].args_hash == ah) {
                    nps = &ctrl->tracked[i];
                    break;
                }
            }
            if (nps && nps->no_progress_count >= ctrl->no_progress_block_after) {
                ctrl->halt_decision_active = true;
                tool_guardrail_decision_t d;
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "Blocked %s: this read-only call returned the same result "
                         "%d times. Use the result already provided or try a "
                         "different query.", tool_name, nps->no_progress_count);
                set_decision(&d, GUARDRAIL_BLOCK, "idempotent_no_progress_block",
                             msg, tool_name, nps->no_progress_count);
                ctrl->halt_decision = d;
                return d;
            }
        }
    }

/* Port of Python agent/tool_guardrails.py:after_call */

    snprintf(allow.tool_name, sizeof(allow.tool_name), "%s", tool_name);
    return allow;
}

/* ================================================================
 *  after_call — record result and produce decision
 * ================================================================ */

tool_guardrail_decision_t
tool_guardrail_after_call(tool_guardrail_controller_t *ctrl,
                           const char *tool_name,
                           const char *tool_args,
                           const char *result,
                           bool failed)
{
    tool_guardrail_decision_t allow = { .action = GUARDRAIL_ALLOW };
    if (!ctrl || !tool_name) return allow;
    snprintf(allow.tool_name, sizeof(allow.tool_name), "%s", tool_name);

    uint32_t ah = tool_guardrail_hash(tool_args);
    tool_guardrail_tracked_t *slot = find_slot(ctrl, tool_name, ah);

    if (failed) {
        /* Track failure */
        if (!slot) {
            slot = alloc_slot(ctrl);
            if (!slot) return allow;
            snprintf(slot->tool_name, sizeof(slot->tool_name), "%s", tool_name);
            slot->args_hash = ah;
        }
        slot->failure_count++;
        slot->same_tool_failures++;

        /* Also update same-tool-failure for tool-name-only tracking */
        for (int i = 0; i < ctrl->count; i++) {
            if (ctrl->tracked[i].active &&
                strcmp(ctrl->tracked[i].tool_name, tool_name) == 0 &&
                ctrl->tracked[i].args_hash != ah) {
                ctrl->tracked[i].same_tool_failures++;
            }
        }

        int exact_count = slot->failure_count;
        int same_count = slot->same_tool_failures;

        /* Hard stop checks (halt turn) */
        if (ctrl->hard_stop_enabled && same_count >= ctrl->same_tool_failure_halt_after) {
            ctrl->halt_decision_active = true;
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "Stopped %s: it failed %d times this turn. "
                     "Stop retrying and choose a different approach.",
                     tool_name, same_count);
            set_decision(&d, GUARDRAIL_HALT, "same_tool_failure_halt",
                         msg, tool_name, same_count);
            ctrl->halt_decision = d;
            return d;
        }

        /* Warning checks */
        if (ctrl->warnings_enabled && exact_count >= ctrl->exact_failure_warn_after) {
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg),
                     "%s has failed %d times with identical arguments. "
                     "This looks like a loop — inspect the error and change "
                     "strategy instead of retrying unchanged.",
                     tool_name, exact_count);
            set_decision(&d, GUARDRAIL_WARN, "repeated_exact_failure_warning",
                         msg, tool_name, exact_count);
            return d;
        }

        if (ctrl->warnings_enabled && same_count >= ctrl->same_tool_failure_warn_after) {
            tool_guardrail_decision_t d;
            char msg[512];
            snprintf(msg, sizeof(msg), "%s has failed %d times this turn. %s",
                     tool_name, same_count,
                     tool_failure_recovery_hint(tool_name, same_count));
            set_decision(&d, GUARDRAIL_WARN, "same_tool_failure_warning",
                         msg, tool_name, same_count);
            return d;
        }

        allow.count = exact_count;
        return allow;
    }

    /* Tool succeeded — clear failure tracking */
    if (slot) {
        slot->failure_count = 0;
        slot->same_tool_failures = 0;
    }

    /* No-progress tracking for idempotent tools */
    if (tool_guardrail_is_idempotent(tool_name)) {
        uint32_t rh = tool_guardrail_hash(result);
        if (!slot) {
            slot = alloc_slot(ctrl);
            if (!slot) return allow;
            snprintf(slot->tool_name, sizeof(slot->tool_name), "%s", tool_name);
            slot->args_hash = ah;
            slot->result_hash = rh;
            slot->no_progress_count = 1;
        } else {
            if (slot->result_hash == rh) {
                slot->no_progress_count++;
            } else {
                slot->result_hash = rh;
                slot->no_progress_count = 1;
            }

            if (ctrl->warnings_enabled && slot->no_progress_count >= ctrl->no_progress_warn_after) {
                tool_guardrail_decision_t d;
                char msg[512];
                snprintf(msg, sizeof(msg),
                         "%s returned the same result %d times. "
                         "Use the result already provided or change the query.",
                         tool_name, slot->no_progress_count);
                set_decision(&d, GUARDRAIL_WARN, "idempotent_no_progress_warning",
                             msg, tool_name, slot->no_progress_count);
                return d;
            }
        }
/* Port of Python agent/tool_guardrails.py:toolguard_synthetic_result */

    }

    return allow;
}

/* ================================================================
 *  Synthetic result / guidance helpers
 * ================================================================ */

/* Port of Python agent/tool_guardrails.py:toolguard_synthetic_result */
char *toolguard_synthetic_result(const tool_guardrail_decision_t *decision)
{
    if (!decision || !decision->tool_name[0])
        return strdup("{\"error\":\"blocked by guardrail\"}");

    /* Build JSON with tool name, code, message */
    char buf[1024];
    int n = snprintf(buf, sizeof(buf),
        "{\"error\":\"%s\",\"guardrail\":{"
        "\"action\":%d,\"code\":\"%s\",\"tool_name\":\"%s\",\"count\":%d}}",

        decision->message,
        (int)decision->action,
        decision->code,
        decision->tool_name,
        decision->count);
    if (n < 0 || n >= (int)sizeof(buf))
        return strdup("{\"error\":\"blocked by guardrail\"}");
    return strdup(buf);
}

/* Port of Python agent/tool_guardrails.py:append_toolguard_guidance */
char *append_toolguard_guidance(const char *result,
                                      const tool_guardrail_decision_t *decision)
{
    if (!decision) return result ? strdup(result) : strdup("");

    if (decision->action != GUARDRAIL_WARN && decision->action != GUARDRAIL_HALT)
        return result ? strdup(result) : strdup("");

    if (!decision->message[0])
        return result ? strdup(result) : strdup("");

    const char *label = (decision->action == GUARDRAIL_HALT)
        ? "Tool loop hard stop"
        : "Tool loop warning";

    /* Calculate new length */
    size_t rlen = result ? strlen(result) : 0;
    size_t suffix_len = strlen(label) + strlen(decision->code) + 80;
    char *out = (char *)malloc(rlen + suffix_len + 1);
    if (!out) return strdup("");

    if (result)
        memcpy(out, result, rlen);

    int n = snprintf(out + rlen, suffix_len,
                     "\n\n[%s: %s; count=%d; %s]",
                     label, decision->code, decision->count, decision->message);
    if (n < 0) { free(out); return strdup(""); }

    return out;
}

/* ──── Thin wrappers (port of Python concept helpers) ──────── */

/* Port of Python agent/tool_guardrails.py:_as_bool */
bool as_bool(const char *val, bool def)
{
    if (!val) return def;
    if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 ||
        strcmp(val, "yes") == 0) return true;
    if (strcmp(val, "false") == 0 || strcmp(val, "0") == 0 ||
        strcmp(val, "no") == 0) return false;
    return def;
}

/* Port of Python agent/tool_guardrails.py:_positive_int */
int positive_int(const char *val, int def)
{
    if (!val || !val[0]) return def;
    char *end = NULL;
    long n = strtol(val, &end, 10);
    if (end == val || n < 0) return def;
    return (int)n;
}

/* Port of Python agent/tool_guardrails.py:allows_execution */
/* Check if a guardrail decision allows execution to proceed. */
bool tool_guardrail_allows_execution(const tool_guardrail_decision_t *d)
{
    if (!d) return false;
    /* Execution is allowed when the action is explicitly ALLOW */
    return d->action == GUARDRAIL_ALLOW;
}

/* Port of Python agent/tool_guardrails.py:should_halt */
/* Check if the guardrail controller has an active halt decision. */
bool tool_guardrail_should_halt(const tool_guardrail_controller_t *ctrl)
{
    if (!ctrl) return false;
    /* Halt is active when a repeated failure has triggered a stop */
    return ctrl->halt_decision_active;
}

/* Port of Python agent/tool_guardrails.py:halt_decision */
tool_guardrail_decision_t
tool_guardrail_halt_decision(const tool_guardrail_controller_t *ctrl)
{
    tool_guardrail_decision_t none = { .action = GUARDRAIL_ALLOW };
    if (!ctrl || !ctrl->halt_decision_active) return none;
    return ctrl->halt_decision;
}

/* Port of Python agent/tool_guardrails.py:canonical_tool_args */
char *canonical_tool_args(const char *tool_name,
                                     const char *tool_args)
{
    if (!tool_args) return strdup("");
    /* Simple normalization: collapse whitespace, strip leading/trailing space */
    const char *p = tool_args;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return strdup("");

    char *out = strdup(p);
    if (!out) return strdup("");
    size_t len = strlen(out);
    while (len > 0 && isspace((unsigned char)out[len-1]))
        out[--len] = '\0';
    return out;
}

/* Port of Python agent/tool_guardrails.py:_coerce_args */
char *coerce_args(const char *tool_name,
                                  const char *tool_args)
{
    (void)tool_name;
    if (!tool_args) return strdup("");
    return canonical_tool_args(tool_name, tool_args);
}


/* Recursively sort object keys in place (Python json.dumps sort_keys=True
 * applies at every nesting level). Arrays are visited but order is preserved. */
static void canonical_sort_keys(json_t *node)
{
    if (!node) return;
    if (node->type == JSON_OBJECT && node->c.count > 1) {
        size_t n = node->c.count;
        for (size_t i = 0; i < n; i++) {
            for (size_t j = i + 1; j < n; j++) {
                if (strcmp(node->c.keys[j], node->c.keys[i]) < 0) {
                    const char *kt = node->c.keys[i];
                    node->c.keys[i] = node->c.keys[j];
                    node->c.keys[j] = kt;
                    json_t *vt = node->c.items[i];
                    node->c.items[i] = node->c.items[j];
                    node->c.items[j] = vt;
                }
            }
        }
    }
    if (node->type == JSON_OBJECT || node->type == JSON_ARRAY) {
        for (size_t i = 0; i < node->c.count; i++)
            canonical_sort_keys(node->c.items[i]);
    }
}

/* Port of Python agent/tool_guardrails.py:_result_hash
 * Stable SHA256 identity of a tool result. Parses the result as JSON and
 * hashes its canonical form (sorted keys, compact separators, ensure_ascii=False
 * — matching Python json.dumps(..., sort_keys=True, separators=(",",":"),
 * ensure_ascii=False)). Falls back to hashing the raw string when unparseable. */
const char *tool_guardrails_result_hash(const char *result)
{
    char *canonical = NULL;
    json_t *parsed = result ? json_parse(result, NULL) : NULL;
    if (parsed) {
        json_t *copy = json_copy(parsed);
        json_free(parsed);
        canonical_sort_keys(copy);
        canonical = copy ? json_serialize(copy) : NULL;
        if (copy) json_free(copy);
    }
    if (!canonical) canonical = strdup(result ? result : "");

    unsigned char hash[32];
    char hex[65];
    crypto_sha256((const unsigned char *)canonical, strlen(canonical), hash);
    for (int j = 0; j < 32; j++)
        snprintf(hex + j * 2, 3, "%02x", hash[j]);
    hex[64] = '\0';
    free(canonical);
    return strdup(hex);
}

/* Port of Python agent/tool_guardrails.py:_result_hash */

/* Port of Python agent/tool_guardrails.py:from_call */
tool_guardrail_decision_t
tool_guardrail_from_call(const char *tool_name,
                          const tool_guardrail_decision_t *current,
                          const tool_guardrail_decision_t *base)
{
    (void)tool_name;
    tool_guardrail_decision_t d = { .action = GUARDRAIL_ALLOW };
    if (current) d = *current;
    if (base && current && current->action == GUARDRAIL_ALLOW && base->action != GUARDRAIL_ALLOW) {
        d = *base;
    }
    return d;
}

/* Port of Python agent/tool_guardrails.py:from_mapping */
tool_guardrail_decision_t
tool_guardrail_from_mapping(const char *tool_name,
                             const tool_guardrail_decision_t *decision)
{
    (void)tool_name;
    tool_guardrail_decision_t d = { .action = GUARDRAIL_ALLOW };
    if (decision) d = *decision;
    return d;
}

/* Port of Python agent/tool_guardrails.py:to_metadata */
char *tool_guardrail_to_metadata(const tool_guardrail_decision_t *decision)
{
    if (!decision) return strdup("{}");

    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"action\":%d,\"code\":\"%s\",\"count\":%d,\"message\":\"%s\",\"tool_name\":\"%s\"}",
             decision->action,
             decision->code,
             decision->count,
             decision->message,
             decision->tool_name);
    return strdup(buf);
}
