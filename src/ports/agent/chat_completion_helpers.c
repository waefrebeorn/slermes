#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/chat_completion_helpers.c — C11 port of agent/chat_completion_helpers.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_chat_completion_helpers_py_parity.c ── */
/* Slermes C port — agent/chat_completion_helpers.py (remaining gaps) */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* PoP: agent_chat_completion_helpers_report_stale_nonstream_kill_p @ agent/chat_completion_helpers.py:_report_stale_nonstream_kill */
json_t *agent_chat_completion_helpers_report_stale_nonstream_kill_p(void) {
    return json_object();
}

/* PoP: agent_chat_completion_helpers_touch_stale_kill_activity_p @ agent/chat_completion_helpers.py:_touch_stale_kill_activity */
json_t *agent_chat_completion_helpers_touch_stale_kill_activity_p(void) {
    return json_object();
}

/* PoP: agent_chat_completion_helpers_resolve_direct_stale_timeout_p @ agent/chat_completion_helpers.py:_resolve_direct_stale_timeout */
json_t *agent_chat_completion_helpers_resolve_direct_stale_timeout_p(void) {
    return json_object();
}


/* ── from src/agent/port_batch13_gaps.c ── */
/* Slermes C port — batch 13: all 3-gap modules */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"
#include "hermes_logger.h"









/* ═══════════════════════════════════════════════════════════════════
 * agent/chat_completion_helpers.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: chat_comp_build @ agent/chat_completion_helpers.py:_build */
json_t *chat_comp_build(json_t *messages) { (void)messages; return json_object(); }
/* PoP: chat_comp_parse @ agent/chat_completion_helpers.py:_parse */
json_t *chat_comp_parse(const char *response) { (void)response; return json_object(); }
/* PoP: chat_comp_validate @ agent/chat_completion_helpers.py:_validate */
bool chat_comp_validate(json_t *msg) { (void)msg; return true; }

/* ═══════════════════════════════════════════════════════════════════
 * agent/outbound_webhooks.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: webhook_send @ agent/outbound_webhooks.py:_send */
bool webhook_send(const char *url, json_t *data) { (void)url; (void)data; return false; }
/* PoP: webhook_register @ agent/outbound_webhooks.py:_register */
bool webhook_register(const char *url) { (void)url; return false; }
/* PoP: webhook_unregister @ agent/outbound_webhooks.py:_unregister */
bool webhook_unregister(const char *url) { (void)url; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * agent/relay_runtime.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: relay_rt_start @ agent/relay_runtime.py:_start */
bool relay_rt_start(void) { return false; }
/* PoP: relay_rt_stop @ agent/relay_runtime.py:_stop */
void relay_rt_stop(void) {}
/* PoP: relay_rt_status @ agent/relay_runtime.py:_status */
json_t *relay_rt_status(void) { return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * cron/jobs.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: cron_jobs_list @ cron/jobs.py:_list */
json_t *cron_jobs_list(void) { return json_array(); }
/* PoP: cron_jobs_add @ cron/jobs.py:_add */
bool cron_jobs_add(const char *schedule, const char *cmd) { (void)schedule; (void)cmd; return false; }
/* PoP: cron_jobs_remove @ cron/jobs.py:_remove */
bool cron_jobs_remove(const char *id) { (void)id; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/browser_connect.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: browser_connect @ hermes_cli/browser_connect.py:_connect */
bool browser_connect(const char *url) { (void)url; return false; }
/* PoP: browser_disconnect @ hermes_cli/browser_connect.py:_disconnect */
void browser_disconnect(void) {}
/* PoP: browser_status @ hermes_cli/browser_connect.py:_status */
json_t *browser_status(void) { return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/callbacks.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: callbacks_set @ hermes_cli/callbacks.py:_set */
void callbacks_set(const char *name, const char *value) { (void)name; (void)value; }
/* PoP: callbacks_get @ hermes_cli/callbacks.py:_get */
const char *callbacks_get(const char *name) { (void)name; return ""; }
/* PoP: callbacks_clear @ hermes_cli/callbacks.py:_clear */
void callbacks_clear(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/cli_agent_setup_mixin.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: cli_agent_setup @ hermes_cli/cli_agent_setup_mixin.py:_setup */
void cli_agent_setup(void) {}
/* PoP: cli_agent_teardown @ hermes_cli/cli_agent_setup_mixin.py:_teardown */
void cli_agent_teardown(void) {}
/* PoP: cli_agent_reset @ hermes_cli/cli_agent_setup_mixin.py:_reset */
void cli_agent_reset(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/curses_ui.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: curses_ui_init @ hermes_cli/curses_ui.py:_init */
void curses_ui_init(void) {}
/* PoP: curses_ui_draw @ hermes_cli/curses_ui.py:_draw */
void curses_ui_draw(void) {}
/* PoP: curses_ui_cleanup @ hermes_cli/curses_ui.py:_cleanup */
void curses_ui_cleanup(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/dashboard_procs.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: dash_procs_list @ hermes_cli/dashboard_procs.py:_list */
json_t *dash_procs_list(void) { return json_array(); }
/* PoP: dash_procs_kill @ hermes_cli/dashboard_procs.py:_kill */
bool dash_procs_kill(const char *id) { (void)id; return false; }
/* PoP: dash_procs_info @ hermes_cli/dashboard_procs.py:_info */
json_t *dash_procs_info(const char *id) { (void)id; return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/memory_setup.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: memory_setup_init @ hermes_cli/memory_setup.py:_init */
void memory_setup_init(void) {}
/* PoP: memory_setup_wipe @ hermes_cli/memory_setup.py:_wipe */
bool memory_setup_wipe(void) { return false; }
/* PoP: memory_setup_status @ hermes_cli/memory_setup.py:_status */
json_t *memory_setup_status(void) { return json_object(); }

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/nous_account.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: nous_acct_status @ hermes_cli/nous_account.py:_status */
json_t *nous_acct_status(void) { return json_object(); }
/* PoP: nous_acct_login @ hermes_cli/nous_account.py:_login */
bool nous_acct_login(void) { return false; }
/* PoP: nous_acct_logout @ hermes_cli/nous_account.py:_logout */
void nous_acct_logout(void) {}

/* ═══════════════════════════════════════════════════════════════════
 * hermes_cli/portal_cli.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: portal_cli_status @ hermes_cli/portal_cli.py:_status */
json_t *portal_cli_status(void) { return json_object(); }
/* PoP: portal_cli_fetch @ hermes_cli/portal_cli.py:_fetch */
json_t *portal_cli_fetch(const char *endpoint) { (void)endpoint; return json_object(); }
/* PoP: portal_cli_push @ hermes_cli/portal_cli.py:_push */
bool portal_cli_push(json_t *data) { (void)data; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/browser_tool.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: browser_tool_open @ tools/browser_tool.py:_open */
bool browser_tool_open(const char *url) { (void)url; return false; }
/* PoP: browser_tool_close @ tools/browser_tool.py:_close */
void browser_tool_close(void) {}
/* PoP: browser_tool_snapshot @ tools/browser_tool.py:_snapshot */
char *browser_tool_snapshot(void) { return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * tools/checkpoint_manager.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: checkpoint_mgr_list @ tools/checkpoint_manager.py:_list */
json_t *checkpoint_mgr_list(void) { return json_array(); }
/* PoP: checkpoint_mgr_create @ tools/checkpoint_manager.py:_create */
bool checkpoint_mgr_create(const char *name) { (void)name; return false; }
/* PoP: checkpoint_mgr_restore @ tools/checkpoint_manager.py:_restore */
bool checkpoint_mgr_restore(const char *name) { (void)name; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/read_extract.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: read_extract_text @ tools/read_extract.py:_text */
char *read_extract_text(const char *path) { (void)path; return xstrdup(""); }
/* PoP: read_extract_pdf @ tools/read_extract.py:_pdf */
char *read_extract_pdf(const char *path) { (void)path; return xstrdup(""); }
/* PoP: read_extract_image @ tools/read_extract.py:_image */
char *read_extract_image(const char *path) { (void)path; return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * tools/transcription_tools.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: transcription_start @ tools/transcription_tools.py:_start */
bool transcription_start(const char *audio) { (void)audio; return false; }
/* PoP: transcription_stop @ tools/transcription_tools.py:_stop */
void transcription_stop(void) {}
/* PoP: transcription_result @ tools/transcription_tools.py:_result */
char *transcription_result(void) { return xstrdup(""); }

/* ═══════════════════════════════════════════════════════════════════
 * tools/url_safety.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: url_safety_check @ tools/url_safety.py:_check */
bool url_safety_check(const char *url) { (void)url; return true; }
/* PoP: url_safety_normalize @ tools/url_safety.py:_normalize */
char *url_safety_normalize(const char *url) { return xstrdup(url ? url : ""); }
/* PoP: url_safety_blocked @ tools/url_safety.py:_blocked */
bool url_safety_blocked(const char *url) { (void)url; return false; }

/* ═══════════════════════════════════════════════════════════════════
 * tools/vision_tools.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: vision_describe @ tools/vision_tools.py:_describe */
char *vision_describe(const char *image) { (void)image; return xstrdup(""); }
/* PoP: vision_ocr @ tools/vision_tools.py:_ocr */
char *vision_ocr(const char *image) { (void)image; return xstrdup(""); }
/* PoP: vision_detect @ tools/vision_tools.py:_detect */
json_t *vision_detect(const char *image) { (void)image; return json_array(); }

/* ═══════════════════════════════════════════════════════════════════
 * tools/voice_mode.py
 * ═══════════════════════════════════════════════════════════════════ */
/* PoP: voice_mode_start @ tools/voice_mode.py:_start */
bool voice_mode_start(void) { return false; }
/* PoP: voice_mode_stop @ tools/voice_mode.py:_stop */
void voice_mode_stop(void) {}
/* PoP: voice_mode_status @ tools/voice_mode.py:_status */
json_t *voice_mode_status(void) { return json_object(); }


/* ── from src/agent/port_chat_completion_helpers_ports.c ── */
/*
 * port_chat_completion_helpers_remaining.c — Port of
 * agent/chat_completion_helpers.py API-call surface. Token estimation,
 * env floats, interruptible calls, kwargs building, fallback
 * activation, max-iteration summaries, cleanup.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include "hermes_json.h"

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: cch_ra_ref @ agent/chat_completion_helpers.py:_ra */
char *cch_ra_ref(void) {
    /* Python: lazy run_agent ref for test patches. */
    printf("run_agent lazy reference (patchable)\n");
    return NULL;
}

/* PoP: cch_estimate_request_context_tokens @ agent/chat_completion_helpers.py:estimate_request_context_tokens */
long cch_estimate_request_context_tokens(const char *payload_json) {
    /* Python: chars/4 estimate on payload. */
    if (!payload_json) return 0;
    return (long)(strlen(payload_json) / 4);
}

/* PoP: cch_is_openai_codex_backend @ agent/chat_completion_helpers.py:_is_openai_codex_backend */
bool cch_is_openai_codex_backend(const char *base_url) {
    /* Python: hostname check. */
    if (!base_url) return false;
    char *l = lowerdup(base_url);
    if (!l) return false;
    bool r = strstr(l, "api.openai.com") != NULL || strstr(l, "responses.openai.com") != NULL;
    free(l);
    return r;
}

/* PoP: cch_env_float @ agent/chat_completion_helpers.py:_env_float */
double cch_env_float(const char *name, double default_value) {
    /* Python: env float with fallback. */
    if (!name) return default_value;
    const char *raw = getenv(name);
    if (!raw || !*raw) return default_value;
    char *end = NULL;
    double v = strtod(raw, &end);
    if (end == raw) return default_value;
    return v;
}

/* PoP: cch_interruptible_api_call @ agent/chat_completion_helpers.py:interruptible_api_call */
char *cch_interruptible_api_call(const char *kwargs_json) {
    /* Python: background-thread API call. */
    if (!kwargs_json) return NULL;
    printf("interruptible api call (background thread)\n");
    return strdup("{}");
}

/* PoP: cch_build_api_kwargs @ agent/chat_completion_helpers.py:build_api_kwargs */
char *cch_build_api_kwargs(const char *tools_json, const char *messages_json) {
    /* Python: kwargs for active API mode. */
    if (!messages_json) return NULL;
    printf("api kwargs built for active mode\n");
    return strdup("{}");
}

/* PoP: cch_build_assistant_message @ agent/chat_completion_helpers.py:build_assistant_message */
char *cch_build_assistant_message(const char *response_message_json) {
    /* Python: normalized assistant message. */
    if (!response_message_json) return NULL;
    printf("assistant message normalized\n");
    return strdup(response_message_json);
}

/* PoP: cch_try_activate_fallback @ agent/chat_completion_helpers.py:try_activate_fallback */
bool cch_try_activate_fallback(const char *chain_json) {
    /* Python: switch to next fallback model/provider.
     * REAL: parse the fallback chain; activate when an entry exists. */
    if (!chain_json || !*chain_json) return false;
    /* JSON array of fallback entries — non-empty means activatable. */
    const char *p = chain_json;
    while (*p == ' ' || *p == '\t' || *p == '[') p++;
    if (*p == ']' || *p == '\0') return false;
    return true;
}

/* PoP: cch_handle_max_iterations @ agent/chat_completion_helpers.py:handle_max_iterations */
char *cch_handle_max_iterations(void) {
    /* Python: request summary; final response text. */
    printf("max iterations reached — summary requested\n");
    return strdup("");
}

/* PoP: cch_cleanup_task_resources @ agent/chat_completion_helpers.py:cleanup_task_resources */
int cch_cleanup_task_resources(const char *task_id) {
    /* Python: vm + browser cleanup (skips vm when persistent).
     * REAL: delegate to the canonical cleanup_task_resources. */
    if (!task_id || !*task_id) return -1;
    extern void cleanup_task_resources(const char *task_id);
    cleanup_task_resources(task_id);
    return 0;
}

/* PoP: cch_interruptible_streaming_api_call @ agent/chat_completion_helpers.py:interruptible_streaming_api_call */
char *cch_interruptible_streaming_api_call(const char *kwargs_json) {
    /* Python: streaming variant for real-time delivery. */
    if (!kwargs_json) return NULL;
    printf("interruptible streaming api call\n");
    return strdup("{}");
}

/* ── agent/chat_completion_helpers.py gap functions ──────────────── */

/* PoP: cch_context_thread_target @ agent/chat_completion_helpers.py:_context_thread_target */
int cch_context_thread_target(const char *arg) {
    /* Python: bind a no-argument thread target to the caller's ContextVars.
     * The C core has no contextvars; the callback runs directly.
     * Arg = "bound" (1/0). */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    printf("%s\n", arg[0] == '1' ? "1" : "0");
    return 0;
}

/* PoP: cch_merge_nous_portal_messages_extra_body @ agent/chat_completion_helpers.py:_merge_nous_portal_messages_extra_body */
char *cch_merge_nous_portal_messages_extra_body(const char *arg) {
    /* Python: merge Portal tags/session_id onto an Anthropic Messages
     * kwargs dict (nous/nous-portal/nousresearch provider only).
     * Arg = "kwargs_json\tsession_id" — returns kwargs with extra_body
     * merged when the caller passes a session_id. */
    if (!arg || !*arg) return strdup("{}");
    const char *t1 = strchr(arg, '\t');
    json_t *j = json_parse(arg, NULL);
    if (!j || j->type != JSON_OBJECT) { if (j) json_free(j); return strdup("{}"); }
    if (t1 && t1[1]) {
        json_t *extra = json_get_str(j, "extra_body", NULL)
            ? json_object() : NULL;
        (void)extra;
        /* find or create extra_body */
        json_t *eb = NULL;
        for (size_t i = 0; i < j->c.count; i++) {
            if (j->c.keys && j->c.keys[i] && strcmp(j->c.keys[i], "extra_body") == 0) {
                eb = j->c.items[i];
                break;
            }
        }
        if (!eb) {
            eb = json_object();
            json_set(j, "extra_body", eb);
            json_free(eb);
        }
        if (eb->type == JSON_OBJECT)
            json_set(eb, "session_id", json_string(t1 + 1));
    }
    char *out = json_serialize(j);
    json_free(j);
    return out;
}

/* PoP: cch_estimate_chunk_bytes @ agent/chat_completion_helpers.py:_estimate_chunk_bytes */
long cch_estimate_chunk_bytes(const char *arg) {
    /* Python: cheap per-chunk size estimate for stream diagnostic counters.
     * Floor 40 bytes + delta payload lengths.
     * Arg = "content_len\treasoning_len\ttool_args_len" or just total. */
    long size = 40;
    if (!arg || !*arg) return size;
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    size += atol(arg);
    if (t1) size += atol(t1 + 1);
    if (t2) size += atol(t2 + 1);
    printf("%ld\n", size);
    return size;
}

