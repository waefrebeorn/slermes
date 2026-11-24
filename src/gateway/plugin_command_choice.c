/*
 * plugin_command_choice.c — real C11 port of upstream Hermes
 * gateway/plugin_command_choice.py (gate w17_plugincomm_d95180).
 *
 * PoP: w17_plugincomm_d95180_ask_viewers @ gateway/plugin_command_choice.py:_ask_viewers
 * PoP: w17_plugincomm_d95180_chat_choices @ gateway/plugin_command_choice.py:chat_choices
 *
 * get_clarify_timeout / ask_viewer_result / asker_bound / bind_asker / Platform
 * are deterministic injected collaborators. The port reproduces the clarify
 * request payload and the ((result or {}).get("answers") or {}).get("q0")
 * answer projection with its isinstance(str) guard, plus chat_choices' bind
 * decision (already bound or LOCAL source -> nullcontext()).
 */
#include "noc_w17_plugincomm_d95180.h"

#include <stdlib.h>
#include <string.h>

json_t *w17_plugincomm_d95180_ask_viewers(const char *session_id, const char *question,
                                          json_t *labels, int timeout, json_t *result) {
    json_t *out = json_object();
    json_t *request = json_object();
    json_t *payload = json_object();
    json_t *questions = json_array();
    json_t *q = json_object();
    json_t *qids = json_array();
    json_t *answers, *q0, *ans;
    json_t *base = (result && result->type == JSON_OBJECT) ? result : NULL;

    json_set(q, "qid", json_string("q0"));
    json_set(q, "question", json_string(question ? question : ""));
    json_set(q, "choices", (labels && labels->type == JSON_ARRAY) ? json_copy(labels) : json_array());
    json_set(q, "multi_select", json_bool(false));
    json_append(questions, q);
    json_set(payload, "questions", questions);

    json_set(request, "session_id", json_string(session_id ? session_id : ""));
    json_set(request, "op", json_string("clarify"));
    json_set(request, "payload", payload);
    json_set(request, "timeout", timeout > 0 ? json_number((double)timeout) : json_null());
    json_append(qids, json_string("q0"));
    json_set(request, "qids", qids);

    answers = base ? json_obj_get(base, "answers") : NULL;
    answers = (answers && answers->type == JSON_OBJECT) ? answers : NULL;
    q0 = answers ? json_obj_get(answers, "q0") : NULL;
    if (q0 && q0->type == JSON_STRING)
        ans = json_string(q0->str_val ? q0->str_val : "");
    else
        ans = json_null();

    json_set(out, "answer", ans);
    json_set(out, "request", request);
    return out;
}

const char *w17_plugincomm_d95180_chat_choices(int asker_bound, int is_local) {
    if (asker_bound || is_local) return "none";
    return "bound";
}
