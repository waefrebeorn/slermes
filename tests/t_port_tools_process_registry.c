/* Oracle harness for the tools/process_registry.py pure-function gap ports
 * (src/tools/process_registry_gaps.c). Emits one JSON line per case:
 *   {"func":"<c_name>","in":{...},"out":<C result>}
 * The paired sta_oracle_tools_process_registry.py recomputes "out" from the
 * LIVE quarry defs and compares. Cases are hardcoded inputs; edges cover
 * Python slice semantics (n==0 / n<0 / clamp), code-point counting, ANSI
 * stripping, dict-merge override order and the fail-closed routing verdicts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libjson/json.h"
#include "process_registry.h"

typedef struct { const char *func; const char *in; } pr_case_t;

static const pr_case_t CASES[] = {
    /* _not_found */
    {"process_registry_not_found", "{\"session_id\":\"proc_abc123\"}"},
    {"process_registry_not_found", "{\"session_id\":\"\"}"},
    {"process_registry_not_found", "{\"session_id\":\"proc_\xc3\xa9\"}"},
    {"process_registry_not_found", "{\"session_id\":\"weird \\\"id\\\" \\\\ space\"}"},
    /* _output_tail: last-n code points, ANSI stripped; n==0 keeps all,
     * n<0 drops the head, |n|>len clamps. */
    {"process_registry_output_tail", "{\"output_buffer\":\"hello world\",\"n\":5}"},
    {"process_registry_output_tail", "{\"output_buffer\":\"hello\",\"n\":0}"},
    {"process_registry_output_tail", "{\"output_buffer\":\"hello\",\"n\":-2}"},
    {"process_registry_output_tail", "{\"output_buffer\":\"abc\",\"n\":100}"},
    {"process_registry_output_tail", "{\"output_buffer\":\"\\u001b[32mOK\\u001b[0m done\",\"n\":12}"},
    {"process_registry_output_tail", "{\"output_buffer\":\"h\xc3\xa9llo w\xc3\xb6rld\",\"n\":5}"},
    /* _completion_output: 0 → COMPLETION_OUTPUT_CHARS (2000); negative
     * override kept verbatim; output_cut only when cut > 0. */
    {"process_registry_completion_output", "{\"session\":{\"output_buffer\":\"hi\",\"completion_output_chars\":0}}"},
    {"process_registry_completion_output", "{\"session\":{\"output_buffer\":\"0123456789\",\"completion_output_chars\":4}}"},
    {"process_registry_completion_output", "{\"session\":{\"output_buffer\":\"abc\",\"completion_output_chars\":-1}}"},
    {"process_registry_completion_output", "{\"session\":{\"output_buffer\":\"\",\"completion_output_chars\":0}}"},
    {"process_registry_completion_output", "{\"session\":{\"output_buffer\":\"abcde\",\"completion_output_chars\":5}}"},
    {"process_registry_completion_output", "{\"session\":{\"output_buffer\":\"\\u001b[31mabcdef\",\"completion_output_chars\":5}}"},
    /* _exit_fields: exit_code None rides as JSON null. */
    {"process_registry_exit_fields", "{\"session\":{\"exit_code\":0,\"completion_reason\":\"exited\",\"termination_source\":\"\"}}"},
    {"process_registry_exit_fields", "{\"session\":{\"exit_code\":null,\"completion_reason\":\"exited\",\"termination_source\":\"\"}}"},
    {"process_registry_exit_fields", "{\"session\":{\"exit_code\":137,\"completion_reason\":\"killed\",\"termination_source\":\"process.kill\"}}"},
    {"process_registry_exit_fields", "{\"session\":{}}"},
    /* _status_head */
    {"process_registry_status_head", "{\"session\":{\"id\":\"proc_1\",\"command\":\"sleep 5\",\"exited\":false}}"},
    {"process_registry_status_head", "{\"session\":{\"id\":\"proc_2\",\"command\":\"make build\",\"exited\":true}}"},
    {"process_registry_status_head", "{\"session\":{}}"},
    {"process_registry_status_head", "{\"session\":{\"id\":\"proc_3\",\"command\":\"echo \\\"hi\\\"\\nls\",\"exited\":true}}"},
    /* _exit_snapshot: status + command + exit fields + completion output. */
    {"process_registry_exit_snapshot", "{\"session\":{\"command\":\"sleep 1\",\"exit_code\":0,\"completion_reason\":\"exited\",\"termination_source\":\"\",\"output_buffer\":\"done\\n\",\"completion_output_chars\":0},\"status\":\"exited\"}"},
    {"process_registry_exit_snapshot", "{\"session\":{\"command\":\"srv\",\"exit_code\":null,\"completion_reason\":\"lost\",\"termination_source\":\"backend_lost\",\"output_buffer\":\"0123456789\",\"completion_output_chars\":3},\"status\":\"already_exited\"}"},
    {"process_registry_exit_snapshot", "{\"session\":{},\"status\":\"exited\"}"},
    {"process_registry_exit_snapshot", "{\"session\":{\"command\":\"ls\",\"exit_code\":2,\"completion_reason\":\"exited\",\"termination_source\":\"\",\"output_buffer\":\"\\u001b[31merr\\u001b[0m\",\"completion_output_chars\":0},\"status\":\"exited\"}"},
    /* _global_watch_event: **extra overrides the leading fixed keys but
     * never message / the trailing routing keys. */
    {"process_registry_global_watch_event", "{\"type\":\"watch_overflow_tripped\",\"message\":\"flood\",\"extra\":{}}"},
    {"process_registry_global_watch_event", "{\"type\":\"watch_overflow_released\",\"message\":\"resumed\",\"extra\":{\"suppressed\":7}}"},
    {"process_registry_global_watch_event", "{\"type\":\"watch_overflow_tripped\",\"message\":\"m\",\"extra\":{\"session_id\":\"proc_x\",\"type\":\"other\",\"suppressed\":3}}"},
    {"process_registry_global_watch_event", "{\"type\":\"t\",\"message\":\"msg \\\"quoted\\\" \xe2\x80\x94 \xc3\xbc""n\xc3\xaf""code\",\"extra\":{\"platform\":\"telegram\",\"chat_id\":\"9\"}}"},
    /* _watch_event_base: event_id (uuid4 hex) injected; hosted producer
     * carries sandbox_key, local one does not. */
    {"process_registry_watch_event_base", "{\"session\":{\"id\":\"proc_a\",\"command\":\"run\",\"task_id\":\"sa-1\",\"owner_task_id\":\"sa-1\",\"session_key\":\"sk\",\"sandbox_key\":\"sandbox:t/a\",\"parent_session_id\":\"sess-1\",\"started_at\":1700000000.5,\"watcher_platform\":\"telegram\",\"watcher_chat_id\":\"42\",\"watcher_user_id\":\"u\",\"watcher_user_name\":\"\xc3\x9cser\",\"watcher_thread_id\":\"th\",\"watcher_message_id\":\"m\"},\"event_id\":\"deadbeef\"}"},
    {"process_registry_watch_event_base", "{\"session\":{\"id\":\"proc_b\",\"command\":\"local\",\"task_id\":\"t2\",\"owner_task_id\":\"t2\",\"sandbox_key\":\"\"},\"event_id\":\"0123456789abcdef0123456789abcdef\"}"},
    {"process_registry_watch_event_base", "{\"session\":{},\"event_id\":\"abc\"}"},
    {"process_registry_watch_event_base", "{\"session\":{\"id\":\"proc_c\",\"command\":\"c\",\"task_id\":\"sa-child\",\"owner_task_id\":\"sa-parent\",\"started_at\":0},\"event_id\":\"ff\"}"},
    /* ProcessSession.__post_init__: empty/missing owner defaults to task_id. */
    {"process_registry_session_post_init", "{\"session\":{\"task_id\":\"sa-1\",\"owner_task_id\":\"\"}}"},
    {"process_registry_session_post_init", "{\"session\":{\"task_id\":\"sa-1\",\"owner_task_id\":\"sa-parent\"}}"},
    {"process_registry_session_post_init", "{\"session\":{\"task_id\":\"sa-1\"}}"},
    {"process_registry_session_post_init", "{\"session\":{}}"},
    {"process_registry_session_post_init", "{\"session\":{\"task_id\":\"\",\"owner_task_id\":\"\"}}"},
    /* _owns_event: visible/owns_verdict inject the live scope check and
     * callback; "error" models a raised check (fail closed). */
    {"process_registry_owns_event", "{\"evt\":{\"session_key\":\"k1\",\"type\":\"completion\"},\"session_key\":\"k1\",\"is_async_delegation\":false,\"visible\":true,\"owns_verdict\":null}"},
    {"process_registry_owns_event", "{\"evt\":{\"sandbox_key\":\"sandbox:t/a\",\"session_key\":\"k1\"},\"session_key\":\"k1\",\"is_async_delegation\":false,\"visible\":false,\"owns_verdict\":null}"},
    {"process_registry_owns_event", "{\"evt\":{\"session_key\":\"k1\"},\"session_key\":\"\",\"is_async_delegation\":false,\"visible\":true,\"owns_verdict\":true}"},
    {"process_registry_owns_event", "{\"evt\":{\"session_key\":\"k1\"},\"session_key\":\"k1\",\"is_async_delegation\":false,\"visible\":true,\"owns_verdict\":\"error\"}"},
    {"process_registry_owns_event", "{\"evt\":{\"restored\":true},\"session_key\":\"\",\"is_async_delegation\":true,\"visible\":true,\"owns_verdict\":null}"},
    {"process_registry_owns_event", "{\"evt\":{\"session_key\":5},\"session_key\":\"5\",\"is_async_delegation\":false,\"visible\":true,\"owns_verdict\":null}"},
};

static void emit_json(const char *func, const char *in, json_t *out) {
    char *s = out ? json_serialize(out) : NULL;
    printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n", func, in, s ? s : "null");
    free(s);
    json_free(out);
}

static void emit_str(const char *func, const char *in, char *out) {
    char *q = json_escape_string(out ? out : "");
    printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n", func, in, q ? q : "\"\"");
    free(q);
    free(out);
}

static void emit_bool(const char *func, const char *in, bool out) {
    printf("{\"func\":\"%s\",\"in\":%s,\"out\":%s}\n", func, in, out ? "true" : "false");
}

static void run_case(const pr_case_t *c) {
    json_t *in = json_parse(c->in, NULL);
    if (!in) {
        fprintf(stderr, "harness: bad case JSON for %s\n", c->func);
        return;
    }
    const char *fn = c->func;
    if (strcmp(fn, "process_registry_not_found") == 0) {
        emit_json(fn, c->in, process_registry_not_found(json_get_str(in, "session_id", "")));
    } else if (strcmp(fn, "process_registry_output_tail") == 0) {
        emit_str(fn, c->in, process_registry_output_tail(
            json_get_str(in, "output_buffer", ""), (long)json_get_num(in, "n", 0)));
    } else if (strcmp(fn, "process_registry_completion_output") == 0) {
        emit_json(fn, c->in, process_registry_completion_output(json_obj_get(in, "session")));
    } else if (strcmp(fn, "process_registry_exit_fields") == 0) {
        emit_json(fn, c->in, process_registry_exit_fields(json_obj_get(in, "session")));
    } else if (strcmp(fn, "process_registry_status_head") == 0) {
        emit_json(fn, c->in, process_registry_status_head(json_obj_get(in, "session")));
    } else if (strcmp(fn, "process_registry_exit_snapshot") == 0) {
        emit_json(fn, c->in, process_registry_exit_snapshot(
            json_obj_get(in, "session"), json_get_str(in, "status", "")));
    } else if (strcmp(fn, "process_registry_global_watch_event") == 0) {
        emit_json(fn, c->in, process_registry_global_watch_event(
            json_get_str(in, "type", ""), json_get_str(in, "message", ""),
            json_obj_get(in, "extra")));
    } else if (strcmp(fn, "process_registry_watch_event_base") == 0) {
        emit_json(fn, c->in, process_registry_watch_event_base(
            json_obj_get(in, "session"), json_get_str(in, "event_id", "")));
    } else if (strcmp(fn, "process_registry_session_post_init") == 0) {
        json_t *sess = json_copy(json_obj_get(in, "session"));
        process_registry_session_post_init(sess);
        emit_str(fn, c->in, strdup(json_get_str(sess, "owner_task_id", "")));
        json_free(sess);
    } else if (strcmp(fn, "process_registry_owns_event") == 0) {
        const json_t *v = json_obj_get(in, "owns_verdict");
        int verdict;
        const int *vp = NULL;
        if (v && v->type == JSON_BOOL) { verdict = v->bool_val ? 1 : 0; vp = &verdict; }
        else if (v && v->type == JSON_STRING && strcmp(v->str_val, "error") == 0) { verdict = -1; vp = &verdict; }
        emit_bool(fn, c->in, process_registry_owns_event(
            json_obj_get(in, "evt"), json_get_str(in, "session_key", ""),
            vp, json_get_bool(in, "is_async_delegation", false),
            json_get_bool(in, "visible", true)));
    } else {
        fprintf(stderr, "harness: unknown func %s\n", fn);
    }
    json_free(in);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t i = 0; i < sizeof(CASES) / sizeof(CASES[0]); i++)
        run_case(&CASES[i]);
    return 0;
}
