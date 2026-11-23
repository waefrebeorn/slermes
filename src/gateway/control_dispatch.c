/*
 * control_dispatch.c — faithful C11 port of upstream gateway/control_dispatch.py.
 *
 * The module separates POSIX control workers along the hosted producer's
 * callback dependency. The only data-dependent decision in dispatch() is which
 * executor receives the handler: a JSON object whose "verb" is
 * "hosted-producer" is scheduled on the producers pool, everything else (and
 * any parse failure) on the controls pool. close() shuts producers down before
 * controls, with wait=False / cancel_futures=True — not a pure value, so it is
 * documented here rather than exposed as a PoP export.
 *
 * PoP: ctrldisp_is_producer @ gateway/control_dispatch.py:ControlRequestDispatcher.dispatch
 * PoP: ctrldisp_select      @ gateway/control_dispatch.py:ControlRequestDispatcher.dispatch
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_ctrldisp.h"

/* json.loads(raw) except (ValueError, RecursionError) -> None; then
 * isinstance(request, dict) and request.get('verb') == 'hosted-producer'. */
static int ctrl_is_producer(const char *raw) {
    if (!raw) return 0;
    json_t *request = json_parse(raw, NULL); /* NULL on ValueError/RecursionError */
    int producer = 0;
    if (request && request->type == JSON_OBJECT) {
        json_t *verb = json_obj_get(request, "verb");
        if (verb && verb->type == JSON_STRING && verb->str_val &&
            !strcmp(verb->str_val, "hosted-producer"))
            producer = 1;
    }
    json_free(request);
    return producer;
}

struct json_t *ctrldisp_is_producer(const char *raw) {
    return json_bool(ctrl_is_producer(raw) ? true : false);
}

struct json_t *ctrldisp_select(const char *raw) {
    return json_string(ctrl_is_producer(raw) ? "producers" : "controls");
}
