/*
 * t_port_replay_cleanup.c — faithful verification harness for
 * src/agent/port_agent_replay_cleanup.c (agent/replay_cleanup.py).
 *
 * Reads a history JSON fixture from argv[1], runs the three ported C
 * strippers, and emits one JSON object per helper on stdout. The Python
 * oracle (tests/sta_oracle_replay_cleanup.py) recomputes the SAME helpers
 * from the LIVE agent/replay_cleanup.py and the runner diffs the two
 * byte-for-byte.
 */

#include "agent/port_agent_replay_cleanup.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compact JSON string emitter (no spaces) matching the C port's
 * json_serialize() output so the oracle diff is stable. */
static const char *js(const char *s)
{
    static char bufs[4][8192];
    static int bi = 0;
    char *b = bufs[bi];
    bi = (bi + 1) % 4;
    char *q = b;
    *q++ = '"';
    for (const char *p = s; p && *p && q - b < 8000; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else *q++ = c;
    }
    *q++ = '"';
    *q = '\0';
    return b;
}

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <history.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char *err = NULL;
    json_t *hist = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }

    json_t *si = agent_replay_cleanup_strip_interrupted_tool_tails(hist);
    char *si_s = json_serialize(si);
    json_t *sd = agent_replay_cleanup_strip_dangling_tool_call_tail(hist);
    char *sd_s = json_serialize(sd);
    json_t *sa = agent_replay_cleanup_sanitize_replay_history(hist);
    char *sa_s = json_serialize(sa);

    printf("{\"fn\":\"strip_interrupted\",\"out\":%s}\n", js(si_s));
    printf("{\"fn\":\"strip_dangling\",\"out\":%s}\n", js(sd_s));
    printf("{\"fn\":\"sanitize\",\"out\":%s}\n", js(sa_s));

    free(sa_s); json_free(sa);
    free(sd_s); json_free(sd);
    free(si_s); json_free(si);

    /* strip_stale_dangerous_confirmations: fixed now=2000.0, expiry=60.0
     * so the "confirm forced restart" (ts=1000) is expired and redacted,
     * the "are you there?" (ts=2000) is fresh and kept, and the
     * "confirm shutdown" (no timestamp) is kept unchanged. */
    json_t *dc = agent_replay_cleanup_strip_stale_dangerous_confirmations(hist, 2000.0, 60.0);
    char *dc_s = json_serialize(dc);
    printf("{\"fn\":\"strip_dangerous\",\"out\":%s}\n", js(dc_s));
    free(dc_s); json_free(dc);

    json_free(hist);
    free(input);
    return 0;
}
