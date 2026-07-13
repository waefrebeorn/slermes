/* Smoke test: replay_cleanup strippers vs Python reference (embedded). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "port_agent_replay_cleanup.h"

static int failures = 0;

/* Each entry: input, expected strip_interrupted, expected strip_dangling,
 * expected sanitize (compact JSON strings). Generated from the Python
 * reference agent/replay_cleanup.py. */
static const char *CASES[][4] = {
  { /* clean */
    "[{\"content\":\"hi\",\"role\":\"user\"},{\"content\":\"ok\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"1\"}]},{\"content\":\"done\",\"role\":\"tool\",\"tool_call_id\":\"1\"}]",
    "[{\"content\":\"hi\",\"role\":\"user\"},{\"content\":\"ok\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"1\"}]},{\"content\":\"done\",\"role\":\"tool\",\"tool_call_id\":\"1\"}]",
    "[{\"content\":\"hi\",\"role\":\"user\"},{\"content\":\"ok\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"1\"}]},{\"content\":\"done\",\"role\":\"tool\",\"tool_call_id\":\"1\"}]",
    "[{\"content\":\"hi\",\"role\":\"user\"},{\"content\":\"ok\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"1\"}]},{\"content\":\"done\",\"role\":\"tool\",\"tool_call_id\":\"1\"}]",
  },
  { /* completed_pair (untouched) */
    "[{\"content\":\"do\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"c\"}]},{\"content\":\"result\",\"role\":\"tool\",\"tool_call_id\":\"c\"}]",
    "[{\"content\":\"do\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"c\"}]},{\"content\":\"result\",\"role\":\"tool\",\"tool_call_id\":\"c\"}]",
    "[{\"content\":\"do\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"c\"}]},{\"content\":\"result\",\"role\":\"tool\",\"tool_call_id\":\"c\"}]",
    "[{\"content\":\"do\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"c\"}]},{\"content\":\"result\",\"role\":\"tool\",\"tool_call_id\":\"c\"}]",
  },
  { /* dangling_tail: SD + SA drop the trailing assistant(tool_calls) */
    "[{\"content\":\"restart gateway\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"z\"}]}]",
    "[{\"content\":\"restart gateway\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"z\"}]}]",
    "[{\"content\":\"restart gateway\",\"role\":\"user\"}]",
    "[{\"content\":\"restart gateway\",\"role\":\"user\"}]",
  },
  { /* interrupted_mid: SI strips the interrupted assistant->tool block;
       SD leaves it (block is gone so tail is a normal pair); SA = SI */
    "[{\"content\":\"run it\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"a\"}]},{\"content\":\"[command interrupted]\",\"role\":\"tool\",\"tool_call_id\":\"a\"},{\"content\":\"still there?\",\"role\":\"user\"},{\"content\":\"yes\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"b\"}]},{\"content\":\"ok\",\"role\":\"tool\",\"tool_call_id\":\"b\"}]",
    "[{\"content\":\"run it\",\"role\":\"user\"},{\"content\":\"still there?\",\"role\":\"user\"},{\"content\":\"yes\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"b\"}]},{\"content\":\"ok\",\"role\":\"tool\",\"tool_call_id\":\"b\"}]",
    "[{\"content\":\"run it\",\"role\":\"user\"},{\"content\":\"\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"a\"}]},{\"content\":\"[command interrupted]\",\"role\":\"tool\",\"tool_call_id\":\"a\"},{\"content\":\"still there?\",\"role\":\"user\"},{\"content\":\"yes\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"b\"}]},{\"content\":\"ok\",\"role\":\"tool\",\"tool_call_id\":\"b\"}]",
    "[{\"content\":\"run it\",\"role\":\"user\"},{\"content\":\"still there?\",\"role\":\"user\"},{\"content\":\"yes\",\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"b\"}]},{\"content\":\"ok\",\"role\":\"tool\",\"tool_call_id\":\"b\"}]",
  },
  { /* orphan_tool: SI drops the orphan interrupted tool; SD leaves it; SA
       applies SI then SD -> dropped (tail now user msg) */
    "[{\"content\":\"x\",\"role\":\"user\"},{\"content\":\"exit_code 130 interrupt\",\"role\":\"tool\"}]",
    "[{\"content\":\"x\",\"role\":\"user\"}]",
    "[{\"content\":\"x\",\"role\":\"user\"},{\"content\":\"exit_code 130 interrupt\",\"role\":\"tool\"}]",
    "[{\"content\":\"x\",\"role\":\"user\"}]",
  },
};

static void run_one(const char *name, const char *input,
                    json_t *(*fn)(const json_t *), const char *expected)
{
    char *err = NULL;
    json_t *h = json_parse(input, &err);
    if (err) { free(err); fprintf(stderr, "FAIL %s: bad input\n", name); failures++; return; }
    json_t *out = fn(h);
    char *got = json_serialize(out);
    if (strcmp(got, expected) != 0) {
        fprintf(stderr, "FAIL %s\n  got=%s\n  exp=%s\n", name, got, expected);
        failures++;
    } else {
        printf("ok %s\n", name);
    }
    free(got); json_free(out); json_free(h);
}

int main(void) {
    const char *names[] = {"clean","completed_pair","dangling_tail","interrupted_mid","orphan_tool"};
    for (int i = 0; i < 5; i++) {
        const char **c = CASES[i];
        char buf[64];
        snprintf(buf, sizeof(buf), "%s.strip_interrupted", names[i]);
        run_one(buf, c[0], agent_replay_cleanup_strip_interrupted_tool_tails, c[1]);
        snprintf(buf, sizeof(buf), "%s.strip_dangling", names[i]);
        run_one(buf, c[0], agent_replay_cleanup_strip_dangling_tool_call_tail, c[2]);
        snprintf(buf, sizeof(buf), "%s.sanitize", names[i]);
        run_one(buf, c[0], agent_replay_cleanup_sanitize_replay_history, c[3]);
    }
    if (failures) { printf("\n%d FAILURES\n", failures); return 1; }
    printf("\nALL replay_cleanup checks passed\n");
    return 0;
}
