/*
 * turn_loop_errors.c — C port of upstream Hermes agent/turn_loop_errors.py.
 *
 * PoP: turnlooper_handle_outer_loop_error @ agent/turn_loop_errors.py:handle_outer_loop_error
 *
 * Phase-aware outer-loop classification: interpreter shutdown, deterministic
 * local post-processing bug vs API-path failure, unanswered tool_call results,
 * and the per-turn error cap.  Loop-internal collaborators are the fixture
 * seams registered identically by tests/oracle_w8_turnlooper_2ee1aa.py.
 */
#include "noc_w8_turnlooper_2ee1aa.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *xstrdup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s ? s : "", n);
    return p;
}
static char *xfmt(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap); va_end(ap);
    if (n < 0) return xstrdup("");
    char *b = (char *)malloc((size_t)n + 1);
    if (!b) return xstrdup("");
    va_start(ap, fmt); vsnprintf(b, (size_t)n + 1, fmt, ap); va_end(ap);
    return b;
}
static char *py_strip(const char *s) {
    if (!s) return xstrdup("");
    const char *a = s, *b = s + strlen(s);
    while (a < b && isspace((unsigned char)*a)) a++;
    while (b > a && isspace((unsigned char)*(b - 1))) b--;
    char *r = (char *)malloc((size_t)(b - a) + 1);
    memcpy(r, a, (size_t)(b - a)); r[b - a] = '\0';
    return r;
}
/* os.path.splitext(os.path.basename(filename))[0] */
static char *tb_stem(const char *path) {
    if (!path) return xstrdup("");
    const char *base = path;
    for (const char *p = path; *p; p++) if (*p == '/' || *p == '\\') base = p + 1;
    const char *dot = NULL;
    for (const char *p = base; *p; p++) if (*p == '.') dot = p;
    size_t n = (dot && dot > base) ? (size_t)(dot - base) : strlen(base);
    char *r = (char *)malloc(n + 1);
    memcpy(r, base, n); r[n] = '\0';
    return r;
}
static const char *LOCAL_MODS[] = {"message_repair", "sanitize", "local_helpers", NULL};
static const char *API_MODS[] = {"api_call", "provider_call", "openai_client", NULL};
static int in_set(const char *s, const char *const *set) {
    if (!s) return 0;
    for (int i = 0; set[i]; i++) if (!strcmp(s, set[i])) return 1;
    return 0;
}
/* site_copy stub: key + "|" + sorted(kwargs) as k=v */
static char *site_copy1(const char *key, const char *k, const char *v) {
    return xfmt("%s|%s=%s", key, k, v ? v : "");
}
static char *first80(const char *s) {
    if (!s) return xstrdup("");
    size_t n = strlen(s); if (n > 80) n = 80;
    char *r = (char *)malloc(n + 1); memcpy(r, s, n); r[n] = '\0';
    return r;
}
static int json_str_eq(json_t *v, const char *s) {
    if (!v || v->type != JSON_STRING || !s) return 0;
    return !strcmp(v->str_val, s);
}

json_t *turnlooper_handle_outer_loop_error(
    int outer_error_count, int api_call_count, int max_iterations,
    int max_outer_errors, int is_finalizing, int shutdown_error,
    const char *e_text, json_t *tb_files, const char *session_id,
    int suppress_status_output, json_t *messages,
    const char *turn_exit_reason, int failed, json_t *final_response) {
    (void)suppress_status_output; /* logging-only in the live function */
    if (!e_text) e_text = "";
    json_t *res = json_object();
    int n = outer_error_count + 1;
    json_t *msgs = (messages && messages->type == JSON_ARRAY) ? json_copy(messages) : json_array();

    char *action = NULL, *treason = NULL, *fresp = NULL;
    int out_failed = failed ? 1 : 0;
    char *error_msg = NULL;
    int shutdown = is_finalizing || shutdown_error;

    if (shutdown) {
        error_msg = xfmt("Interpreter is shutting down — cannot continue (API call #%d): %s",
                         api_call_count, e_text);
        out_failed = 1;
        treason = xstrdup("interpreter_shutdown");
        if (session_id && session_id[0])
            fresp = xfmt("interpreter_shutdown|resume= (CLI: `hermes --resume %s`)", session_id);
        else
            fresp = xstrdup("interpreter_shutdown|resume=");
        action = xstrdup("break");
    } else {
        int has_local = 0, has_api = 0;
        size_t ntb = tb_files ? json_len(tb_files) : 0;
        for (size_t i = 0; i < ntb; i++) {
            json_t *fj = json_get(tb_files, i);
            const char *fn = (fj && fj->type == JSON_STRING) ? fj->str_val : NULL;
            char *stem = tb_stem(fn);
            if (in_set(stem, LOCAL_MODS)) has_local = 1;
            if (in_set(stem, API_MODS)) has_api = 1;
            free(stem);
        }
        int is_local = has_local && !has_api;
        error_msg = is_local
            ? xfmt("Error during local message processing after API call #%d: %s", api_call_count, e_text)
            : xfmt("Error during API call #%d: %s", api_call_count, e_text);

        /* unanswered assistant tool_calls -> tool error results */
        size_t mlen = json_len(msgs);
        for (int idx = (int)mlen - 1; idx >= 0; idx--) {
            json_t *msg = json_get(msgs, (size_t)idx);
            if (!msg || msg->type != JSON_OBJECT) break;
            const char *role = json_get_str(msg, "role", "");
            if (!strcmp(role, "tool")) continue;
            if (!strcmp(role, "assistant")) {
                json_t *tcs = json_obj_get(msg, "tool_calls");
                if (tcs && tcs->type == JSON_ARRAY && json_len(tcs) > 0) {
                    for (size_t k = 0; k < json_len(tcs); k++) {
                        json_t *tc = json_get(tcs, k);
                        if (!tc || tc->type != JSON_OBJECT) continue;
                        const char *id = json_get_str(tc, "id", NULL);
                        if (!id || !id[0]) continue;
                        int answered = 0;
                        for (size_t j = (size_t)idx + 1; j < json_len(msgs); j++) {
                            json_t *m2 = json_get(msgs, j);
                            if (m2 && m2->type == JSON_OBJECT &&
                                !strcmp(json_get_str(m2, "role", ""), "tool") &&
                                json_str_eq(json_obj_get(m2, "tool_call_id"), id)) {
                                answered = 1; break;
                            }
                        }
                        if (answered) continue;
                        json_t *tool = json_object();
                        json_set(tool, "role", json_string("tool"));
                        json_set(tool, "name", json_string(json_get_str(tc, "name", "")));
                        json_set(tool, "tool_call_id", json_string(id));
                        char *content = xfmt("Error executing tool: %s", error_msg);
                        json_set(tool, "content", json_string(content));
                        free(content);
                        json_append(msgs, tool);
                    }
                }
            }
            break;
        }

        int floor = max_iterations > 1 ? max_iterations : 1;
        int cap = max_outer_errors < floor ? max_outer_errors : floor;
        int near_max = api_call_count >= max_iterations - 1;
        if (is_local || near_max || n >= cap) {
            char *detail = xstrdup(e_text);
            char *err80 = first80(error_msg);
            if (is_local) {
                /* failed kept as it was for deterministic local bugs */
                treason = xfmt("local_processing_error(%s)", err80);
                fresp = site_copy1("local_processing_error", "detail", detail);
            } else if (n >= cap) {
                out_failed = 1;
                treason = xfmt("repeated_outer_errors(%s)", err80);
                fresp = site_copy1("loop_error", "detail", detail);
            } else {
                treason = xfmt("error_near_max_iterations(%s)", err80);
                fresp = site_copy1("loop_error", "detail", detail);
            }
            free(err80); free(detail);
            action = xstrdup("break");
        } else {
            action = xstrdup("fallthrough");
        }
    }

    json_set(res, "action", json_string(action));
    json_set(res, "_outer_error_count", json_number((double)n));
    json_set(res, "_turn_exit_reason",
             treason ? json_string(treason)
                     : (turn_exit_reason ? json_string(turn_exit_reason) : json_null()));
    json_set(res, "failed", json_bool(out_failed));
    json_set(res, "final_response",
             fresp ? json_string(fresp)
                   : ((final_response && final_response->type != JSON_NULL)
                      ? json_copy(final_response) : json_null()));
    json_set(res, "messages", msgs);
    free(action); free(treason); free(fresp); free(error_msg);
    return res;
}
