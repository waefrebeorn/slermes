/* t_port_ac3_native_fallback.c — proves the flow3 seam dispatcher reaches the
 * native C port when no script is installed. Reads a JSON event, calls
 * ac3_call(ctx, "codex_event_has_content", {event}) with an EMPTY io map, and
 * emits {"event":...,"out":0|1}. Gate with oracle_codex_event_content.py.
 */
#include "hermes_json.h"
#include "agent/auxiliary_client_flow3.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    char line[8192];
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        ac3_ctx ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.io = json_object();        /* NO scripts: native fallback must fire */
        ctx.trace = json_array();
        ctx.counters = json_object();
        json_t *args = json_object();
        json_t *ev = json_obj_get(in, "event");
        if (ev) json_set(args, "event", json_copy(ev));
        json_t *r = ac3_call(&ctx, "codex_event_has_content", args);
        int out = (r && r->type == JSON_BOOL && r->bool_val) ? 1 : 0;
        printf("{\"event\":");
        char *s = json_dumps(ev ? ev : json_null(), 0);
        if (s) { fputs(s, stdout); free(s); } else fputs("null", stdout);
        printf(",\"out\":%d}\n", out);
        /* ac3_call stores `args` in the trace event; do not free it here. */
        json_free(r);
        json_free(ctx.counters); json_free(ctx.trace); json_free(ctx.io);
        json_free(in);
    }
    return 0;
}
