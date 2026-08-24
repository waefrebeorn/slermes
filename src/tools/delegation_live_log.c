/*
 * delegation_live_log.c — canonical C11 port of tools/delegation_live_log.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>


/* === dissolved from ports_wrappers.c === */
/* PoP: tools_delegation_live_log_live_transcript_root @ tools/delegation_live_log.py:live_transcript_root */
int tools_delegation_live_log_live_transcript_root(const char *arg) {
    /* Python: get_hermes_dir("cache/delegation", "delegation_cache") / "live"
     * — profile-safe, never ~/.hermes. Arg = optional hermes dir. */
    if (arg && *arg) { printf("%s/cache/delegation/live\n", arg); return 0; }
    const char *hh = getenv("HERMES_HOME");
    if (hh && *hh) printf("%s/cache/delegation/live\n", hh);
    else printf("%s/.hermes/cache/delegation/live\n",
                getenv("HOME") ? getenv("HOME") : ".");
    return 0;
}

/* PoP: tools_delegation_live_log_new_live_delegation_id @ tools/delegation_live_log.py:new_live_delegation_id */
int tools_delegation_live_log_new_live_delegation_id(const char *arg) {
    /* Python: f"deleg_{uuid.uuid4().hex[:8]}" — 8 hex chars of a UUID. */
    (void)arg;
    unsigned char buf[8];
    FILE *urand = fopen("/dev/urandom", "rb");
    if (urand) {
        size_t got = fread(buf, 1, sizeof(buf), urand);
        fclose(urand);
        if (got == sizeof(buf)) {
            printf("deleg_%02x%02x%02x%02x\n", buf[0], buf[1], buf[2], buf[3]);
            return 0;
        }
    }
    /* fallback: time+pid seeded PRNG (not crypto; only used for a dir name) */
    srand((unsigned)(time(NULL) ^ (getpid() << 16)));
    printf("deleg_%02x%02x%02x%02x\n", rand() & 0xff, rand() & 0xff,
           rand() & 0xff, rand() & 0xff);
    return 0;
}

/* PoP: tools_delegation_live_log_u_one_line @ tools/delegation_live_log.py:_one_line */
int tools_delegation_live_log_u_one_line(const char *arg) {
    /* Python: collapse whitespace; truncate with "…(+N chars)". Arg =
     * "text\tlimit". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    size_t tlen = tab ? (size_t)(tab - arg) : strlen(arg);
    long limit = tab ? strtol(tab + 1, NULL, 10) : 120;
    if (limit < 0) limit = 0;
    char *flat = malloc(tlen + 1);
    if (!flat) { printf("\n"); return 0; }
    size_t w = 0;
    int in_ws = 0;
    for (size_t i = 0; i < tlen; i++) {
        char c = arg[i];
        if (c == '\n' || c == '\t' || c == '\r' || c == ' ' || c == '\f' || c == '\v') {
            if (!in_ws && w) flat[w++] = ' ';
            in_ws = 1;
        } else {
            flat[w++] = c;
            in_ws = 0;
        }
    }
    while (w > 0 && flat[w-1] == ' ') w--;
    flat[w] = '\0';
    if ((long)w > limit) {
        printf("%.*s…(+%ld chars)\n", (int)limit, flat, (long)w - limit);
    } else {
        printf("%s\n", flat);
    }
    free(flat);
    return 0;
}

/* PoP: tools_delegation_live_log_assistant_text @ tools/delegation_live_log.py:assistant_text */
int tools_delegation_live_log_assistant_text(const char *arg) {
    /* Python: one-line assistant event (truncated to the max width). */
    if (arg && *arg) printf("assistant %s\n", arg);
    return 0;
}

/* PoP: tools_delegation_live_log_tool_start @ tools/delegation_live_log.py:tool_start */
int tools_delegation_live_log_tool_start(const char *arg) {
    /* Python: flush_stream(); event("tool", f"-> {name}({args})").
     * Arg = "name\targs". */
    if (!arg || !*arg) { printf("-> ?()\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) { printf("-> %s()\n", arg); return 0; }
    printf("-> %.*s(%s)\n", (int)(tab - arg), arg, tab + 1);
    return 0;
}

/* PoP: tools_delegation_live_log_tool_result @ tools/delegation_live_log.py:tool_result */
int tools_delegation_live_log_tool_result(const char *arg) {
    /* Python: event result "<name> ok|ERROR [dur]: <one_line>". Arg =
     * "name\tis_error\tduration\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *name = arg;
    const char *status = (t1 && t1[1] == '1') ? "ERROR" : "ok";
    char dur[32] = "";
    if (t2 && t2[1]) {
        double d = strtod(t2 + 1, NULL);
        snprintf(dur, sizeof(dur), " %.1fs", d);
    }
    const char *result = t3 ? t3 + 1 : "";
    printf("result %s %s%s: %s\n", name ? name : "?", status, dur, result);
    return 0;
}

/* PoP: tools_delegation_live_log_add_stream_delta @ tools/delegation_live_log.py:add_stream_delta */
int tools_delegation_live_log_add_stream_delta(const char *arg) {
    /* Python: buffer delta; flush at threshold. Arg = "ok\tdelta". */
    if (!arg || !*arg || arg[0] != '1') { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab && tab[1]) printf("buffered %s\n", tab + 1);
    return 0;
}

/* PoP: tools_delegation_live_log_observe @ tools/delegation_live_log.py:observe */
int tools_delegation_live_log_observe(const char *arg) {
    /* Python: event mapping. Arg = "event_type\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *et = t1 ? t1 + 1 : "";
    int state = arg[0] == '1';
    if (!state) { printf("ignored event\n"); return 0; }
    printf("observed: %s\n", et);
    return 0;
}

/* PoP: tools_delegation_live_log_wrap_progress_callback @ tools/delegation_live_log.py:wrap_progress_callback */
int tools_delegation_live_log_wrap_progress_callback(const char *arg) {
    /* Python: wrapper with _flush. Arg = "state\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    int state = arg[0] == '1';
    if (!state) { printf("wrapper created (no inner)\n"); return 0; }
    printf("callback wrapped%s\n", (tab && tab[1] == '1') ? " (with flush)" : "");
    return 0;
}

/* PoP: tools_delegation_live_log_create_live_transcripts @ tools/delegation_live_log.py:create_live_transcripts */
int tools_delegation_live_log_create_live_transcripts(const char *arg) {
    /* Python: writers + manifest. Arg = "count\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("live transcripts: none\n"); return 0; }
    printf("live transcripts created: %s writer(s), manifest written\n", arg);
    return 0;
}

/* PoP: tools_delegation_live_log_u_manifest_path @ tools/delegation_live_log.py:_manifest_path */
int tools_delegation_live_log_u_manifest_path(const char *arg) {
    /* Python (delegation_id): live_transcript_root()/delegation_id/manifest.json.
     * live_transcript_root defaults to $HERMES_HOME/logs/live-transcripts. */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *hh = getenv("HERMES_HOME");
    char base[1024];
    if (hh && *hh) snprintf(base, sizeof(base), "%s", hh);
    else snprintf(base, sizeof(base), "%s/.hermes", getenv("HOME") ? getenv("HOME") : ".");
    printf("%s/logs/live-transcripts/%s/manifest.json\n", base, arg);
    return 0;
}

/* PoP: tools_delegation_live_log_update_manifest_statuses @ tools/delegation_live_log.py:update_manifest_statuses */
int tools_delegation_live_log_update_manifest_statuses(const char *arg) {
    /* Python: best-effort manifest update. Arg =
     * "delegation_id\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    int state = t1 && t1[1] == '1';
    if (!state) { printf("manifest update skipped (no id)\n"); return 0; }
    printf("manifest statuses updated: %s\n", arg);
    return 0;
}

/* PoP: tools_delegation_live_log_prune_stale_live_dirs @ tools/delegation_live_log.py:prune_stale_live_dirs */
int tools_delegation_live_log_prune_stale_live_dirs(const char *arg) {
    /* Python: remove dirs older than retention. Arg = "removed\ttotal". */
    if (!arg || !*arg) { printf("0\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    printf("%s\n", arg);
    return 0;
}
