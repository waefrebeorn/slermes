/*
 * context_compressor_window.c - real C11 port of upstream Hermes
 *   agent/context_compressor_window.py window planning helpers.
 * Collaborators that read the wider compressor (the token walk, boundary alignment,
 * the progress floor, the restart-probe constant) are decision inputs here; the
 * head/middle/tail arithmetic and the summary splice are ported exactly.
 * PoP: w19_contextcom_bbabd0_summary_view @ agent/context_compressor_window.py:summary_view
 * PoP: w19_contextcom_bbabd0_restart_probe_bounds @ agent/context_compressor_window.py:CompactionWindowMixin._restart_handoff_probe_bounds
 * PoP: w19_contextcom_bbabd0_effective_protect_first_n @ agent/context_compressor_window.py:CompactionWindowMixin._effective_protect_first_n
 * PoP: w19_contextcom_bbabd0_protect_head_size @ agent/context_compressor_window.py:CompactionWindowMixin._protect_head_size
 * PoP: w19_contextcom_bbabd0_compress_window @ agent/context_compressor_window.py:CompactionWindowMixin._compress_window
 */
#include "hermes_json.h"
#include "noc_w19_contextcom_bbabd0.h"

static size_t list_len(json_t *a) { return (a && a->type == JSON_ARRAY) ? json_len(a) : 0; }

static const char *msg_role(json_t *m) {
    if (!m || m->type != JSON_OBJECT) return NULL;
    json_t *r = json_obj_get(m, "role");
    return (r && r->type == JSON_STRING) ? r->str_val : NULL;
}

/* Python slice normalisation of list[a:b]. */
static void slice_bounds(size_t n, int start, int end, int *s, int *e) {
    long long lo = start, hi = end, len = (long long)n;
    if (lo < 0) lo += len;
    if (lo < 0) lo = 0;
    if (lo > len) lo = len;
    if (hi < 0) hi += len;
    if (hi < 0) hi = 0;
    if (hi > len) hi = len;
    *s = (int)lo;
    *e = (int)hi;
}

/* _tool_calls_by_id: tool_call_id -> (tool_name, raw_arguments) over assistant tool calls.
 * Only the name is needed here; returns 1 and stores the name when the id maps to a call. */
static int call_name_for(json_t *messages, const char *cid, const char **name) {
    size_t n = list_len(messages);
    for (size_t i = 0; i < n; i++) {
        json_t *m = json_get(messages, (size_t)i);
        const char *role = msg_role(m);
        if (!role || strcmp(role, "assistant")) continue;
        json_t *tcs = json_obj_get(m, "tool_calls");
        if (!tcs || tcs->type != JSON_ARRAY) continue;
        for (size_t j = 0; j < json_len(tcs); j++) {
            json_t *tc = json_get(tcs, j);
            if (!tc || tc->type != JSON_OBJECT) continue;
            json_t *id = json_obj_get(tc, "id");
            const char *idv = (id && id->type == JSON_STRING) ? id->str_val : "";
            if (strcmp(idv, cid)) continue;
            json_t *fn = json_obj_get(tc, "function");
            const char *nm = NULL;
            if (fn && fn->type == JSON_OBJECT) {
                json_t *nv = json_obj_get(fn, "name");
                if (nv && nv->type == JSON_STRING) nm = nv->str_val;
            }
            *name = nm;   /* None when the key is absent or null: never "clarify" */
            return 1;
        }
    }
    return 0;
}

json_t *w19_contextcom_bbabd0_summary_view(json_t *plan, json_t *originals, int start, int end) {
    size_t plen = list_len(plan), olen = list_len(originals);
    if (olen != plen) return plan ? json_copy(plan) : json_array();

    int s = 0, e = 0;
    slice_bounds(plen, start, end, &s, &e);

    json_t *out = json_array();
    for (int i = 0; i < s; i++) json_append(out, json_copy(json_get(plan, (size_t)i)));
    for (int i = s; i < e; i++) {
        json_t *pruned = json_get(plan, (size_t)i);
        json_t *original = json_get(originals, (size_t)i);
        json_t *keep = pruned;
        if (original && original->type == JSON_OBJECT) {
            const char *role = msg_role(original);
            if (role && !strcmp(role, "tool")) {
                json_t *tcid = json_obj_get(original, "tool_call_id");
                const char *cid = (tcid && tcid->type == JSON_STRING) ? tcid->str_val : "";
                int lookup = 1;
                /* A non-string truthy id is a dict key the string map cannot hold: default. */
                if (tcid && tcid->type == JSON_NUMBER && tcid->num_val != 0.0) lookup = 0;
                if (tcid && tcid->type == JSON_BOOL && tcid->bool_val) lookup = 0;
                const char *nm = NULL;
                int found = lookup ? call_name_for(originals, cid, &nm) : 0;
                int keeps_pruned = found && nm && !strcmp(nm, "clarify");
                if (!keeps_pruned) keep = original;
            }
        }
        json_append(out, json_copy(keep));
    }
    for (int i = e; i < (int)plen; i++) json_append(out, json_copy(json_get(plan, (size_t)i)));
    return out;
}

json_t *w19_contextcom_bbabd0_restart_probe_bounds(json_t *messages, int protect_first_n,
                                                  int extra) {
    size_t n = list_len(messages);
    int start = 0, end = 0;
    if (n > 0 && protect_first_n > 0) {
        const char *first = msg_role(json_get(messages, 0));
        int first_non_system = (first && !strcmp(first, "system")) ? 1 : 0;
        long long stop = (long long)first_non_system + (long long)protect_first_n + (long long)extra;
        if (stop > (long long)n) stop = (long long)n;
        start = first_non_system;
        end = (int)stop;
    }
    json_t *out = json_array();
    json_append(out, json_number((double)start));
    json_append(out, json_number((double)end));
    return out;
}

/* any(map(self._is_context_summary_message, messages[probe_start:probe_end])) */
static int has_summary_flag(json_t *messages, int start, int end) {
    size_t n = list_len(messages);
    int s = start, e = end;
    if (s < 0) s = 0;
    if (e > (int)n) e = (int)n;
    for (int i = s; i < e; i++) {
        json_t *m = json_get(messages, (size_t)i);
        if (m && m->type == JSON_OBJECT) {
            json_t *f = json_obj_get(m, "is_summary");
            if (f && (f->type == JSON_BOOL ? f->bool_val : 0)) return 1;
        }
    }
    return 0;
}

int w19_contextcom_bbabd0_effective_protect_first_n(int pin_task_turn, int compression_count,
                                                    int previous_summary, int protect_first_n,
                                                    json_t *messages, int extra) {
    if (pin_task_turn) return 1;
    if (compression_count >= 1 || previous_summary) return 0;
    size_t n = list_len(messages);
    if (n > 0 && protect_first_n > 0) {
        json_t *bounds = w19_contextcom_bbabd0_restart_probe_bounds(messages, protect_first_n, extra);
        int ps = (int)(json_get(bounds, 0) ? json_get(bounds, 0)->num_val : 0.0);
        int pe = (int)(json_get(bounds, 1) ? json_get(bounds, 1)->num_val : 0.0);
        int found = has_summary_flag(messages, ps, pe);
        json_free(bounds);
        if (found) return 0;
    }
    return protect_first_n;
}

int w19_contextcom_bbabd0_protect_head_size(int pin_task_turn, int compression_count,
                                            int previous_summary, int protect_first_n,
                                            json_t *messages, int extra) {
    size_t n = list_len(messages);
    const char *first = n > 0 ? msg_role(json_get(messages, 0)) : NULL;
    int head = (first && !strcmp(first, "system")) ? 1 : 0;
    return head + w19_contextcom_bbabd0_effective_protect_first_n(
        pin_task_turn, compression_count, previous_summary, protect_first_n, messages, extra);
}

/* self._align_boundary_forward(messages, idx): push past any orphan tool results. */
static int align_forward(json_t *messages, int idx) {
    size_t n = list_len(messages);
    while (idx >= 0 && idx < (int)n) {
        const char *role = msg_role(json_get(messages, (size_t)idx));
        if (!role || strcmp(role, "tool")) break;
        idx++;
    }
    return idx;
}

json_t *w19_contextcom_bbabd0_compress_window(json_t *messages, int pin_task_turn,
                                              int compression_count, int previous_summary,
                                              int protect_first_n, int extra, int tail_cut,
                                              int last_user_idx, int aligned_backward,
                                              int floor) {
    size_t n = list_len(messages);
    int head = w19_contextcom_bbabd0_protect_head_size(pin_task_turn, compression_count,
                                                       previous_summary, protect_first_n,
                                                       messages, extra);
    int compress_start = align_forward(messages, head);
    int compress_end = tail_cut;
    if (compress_end == last_user_idx) {
        int bridge_idx = last_user_idx - 1;
        const char *bridge_role = (bridge_idx >= 0 && bridge_idx < (int)n)
                                ? msg_role(json_get(messages, (size_t)bridge_idx)) : NULL;
        if (bridge_role && !strcmp(bridge_role, "tool")) {
            bridge_idx = aligned_backward;
        } else if (!bridge_role || strcmp(bridge_role, "assistant")) {
            bridge_idx = -1;
        }
        /* The bridge never moves the cut below the progress floor. */
        if (bridge_idx >= floor) compress_end = bridge_idx;
    }
    json_t *out = json_array();
    json_append(out, json_number((double)compress_start));
    json_append(out, json_number((double)compress_end));
    return out;
}
