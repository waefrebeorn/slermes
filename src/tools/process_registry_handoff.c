/*
 * process_registry_handoff.c -- real C11 port of upstream Hermes
 *   tools/process_registry_handoff.py.
 *
 * PoP: w20_processreg_37cba9_handoff @ tools/process_registry_handoff.py:handoff_process
 * PoP: w20_processreg_37cba9_max_handoffs_per_child @ tools/process_registry_handoff.py:_MAX_HANDOFFS_PER_CHILD
 *
 * The three live seams the Python reaches for inside the function --
 * process_registry.transfer_ownership, delegate_tool_registry._active_subagents
 * and terminal_tool._resolve_container_task_id -- are the `world` argument here;
 * every validation and every message is the module's own.
 */
#include "hermes_json.h"
#include "noc_w20_processreg_37cba9.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PRHB_MAX_HANDOFFS_PER_CHILD 3

static int prhb_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

static int prhb_is_py_int(double d) {
    if (!isfinite(d)) return 0;
    if (floor(d) != d) return 0;
    return d >= -9.007199254740992e15 && d <= 9.007199254740992e15;
}

/* str(v or "").strip() for the scalar shapes args["data"] can carry. */
static char *prhb_str_strip(const json_t *v) {
    char buf[64];
    const char *raw = "";
    size_t a = 0, b;
    if (prhb_truthy(v)) {
        switch (v->type) {
        case JSON_STRING: raw = v->str_val ? v->str_val : ""; break;
        case JSON_BOOL:   raw = v->bool_val ? "True" : "False"; break;
        case JSON_NUMBER:
            if (prhb_is_py_int(v->num_val)) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
            else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
            raw = buf;
            break;
        default: raw = ""; break;
        }
    }
    b = strlen(raw);
    while (a < b && (raw[a] == ' ' || raw[a] == '\t' || raw[a] == '\n' || raw[a] == '\r' ||
                     raw[a] == '\f' || raw[a] == '\v')) a++;
    while (b > a && (raw[b - 1] == ' ' || raw[b - 1] == '\t' || raw[b - 1] == '\n' ||
                     raw[b - 1] == '\r' || raw[b - 1] == '\f' || raw[b - 1] == '\v')) b--;
    {
        char *out = (char *)malloc(b - a + 1);
        if (!out) return NULL;
        memcpy(out, raw + a, b - a);
        out[b - a] = '\0';
        return out;
    }
}

/* str(<expr> or "") for the two owner attributes. */
static const char *prhb_attr_or_empty(const json_t *obj, const char *key) {
    const json_t *v = json_obj_get(obj, key);
    if (!prhb_truthy(v)) return NULL;
    return json_get_str(obj, key, NULL);
}

static json_t *prhb_error(const char *msg) {
    json_t *out = json_object();
    json_set(out, "error", json_string(msg ? msg : ""));
    return out;
}

int w20_processreg_37cba9_max_handoffs_per_child(void) {
    return PRHB_MAX_HANDOFFS_PER_CHILD;
}

json_t *w20_processreg_37cba9_handoff(const char *session_id, const json_t *args,
                                      const char *task_id, const json_t *world) {
    const char *owner = NULL;
    const char *current;
    const char *fallback;
    char *note;
    json_t *out;
    (void)task_id;

    if (!world || world->type != JSON_OBJECT) return prhb_error(
        "handoff is only available to a running subagent with a live parent; you are not one.");
    if (!json_get_bool(world, "has_child", 0) || !json_get_bool(world, "parent_live", 0)) {
        return prhb_error(
            "handoff is only available to a running subagent with a live parent; you are not one.");
    }
    current = prhb_attr_or_empty(world, "parent_current_task_id");
    fallback = prhb_attr_or_empty(world, "parent_session_id");
    owner = current ? current : (fallback ? fallback : "");
    if (!owner[0]) {
        return prhb_error(
            "parent has no process owner id yet; retry after the parent's turn has started.");
    }
    if ((int)json_get_num(world, "handed_before", 0.0) >= PRHB_MAX_HANDOFFS_PER_CHILD) {
        static const char *const FMT =
            "handoff cap reached (%d per subagent); wait on or kill the rest yourself.";
        char msg[160];
        snprintf(msg, sizeof(msg), FMT, PRHB_MAX_HANDOFFS_PER_CHILD);
        return prhb_error(msg);
    }
    {
        const json_t *data = args && args->type == JSON_OBJECT ? json_obj_get(args, "data") : NULL;
        note = prhb_str_strip(data);              /* str(args.get("data") or "").strip() */
    }
    if (!note || !note[0]) {
        free(note);
        return prhb_error(
            "handoff requires `data`: one sentence saying what the process is for and what the "
            "parent should do with its result.");
    }
    if (!json_get_bool(world, "transfer_ok", 0)) {
        const char *head = "cannot hand off ";
        const char *mid = ": not a running process you own (already exited? read its result with "
                          "poll/log and report it instead).";
        size_t len = strlen(head) + strlen(session_id ? session_id : "") + strlen(mid) + 1;
        char *msg = (char *)malloc(len);
        free(note);
        if (!msg) return prhb_error("");
        snprintf(msg, len, "%s%s%s", head, session_id ? session_id : "", mid);
        out = prhb_error(msg);
        free(msg);
        return out;
    }

    out = json_object();
    json_set(out, "status", json_string("handed_off"));
    json_set(out, "session_id", json_string(json_get_str(world, "transfer_session_id", "")));
    json_set(out, "command", json_string(json_get_str(world, "transfer_command", "")));
    json_set(out, "note", json_string(
        "Your parent now owns this process and will receive its completion; you will not. "
        "Mention the handoff in your final answer."));
    free(note);
    return out;
}
