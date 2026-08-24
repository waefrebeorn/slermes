/*
 * compute_host.c — canonical C11 port of tui_gateway/compute_host.py.
 *
 * Compute-host surface for the TUI gateway: monotonic timestamps, the
 * SpikeAgent deterministic agent-shaped probe (pipe/interrupt measurement),
 * the JSON emit transport, and repo/build identity.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* PoP: cg_now_ns @ tui_gateway/compute_host.py:now_ns */
long long cg_now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

/* PoP: cg_repo_root @ tui_gateway/compute_host.py:_repo_root */
char *cg_repo_root(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    char cmd[1024], *out = NULL;
    snprintf(cmd, sizeof cmd,
             "cd \"%s\" && git rev-parse --show-toplevel 2>/dev/null", home?home:".");
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    char buf[1024];
    if (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        while (n && (buf[n-1]=='\n'||buf[n-1]=='\r')) buf[--n]='\0';
        out = strdup(buf);
    }
    pclose(f);
    return out;
}

/* PoP: cg_build_sha @ tui_gateway/compute_host.py:_build_sha */
char *cg_build_sha(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home || !*home) home = getenv("HOME");
    char cmd[1024], *out = NULL;
    snprintf(cmd, sizeof cmd,
             "cd \"%s\" && git rev-parse --short=12 HEAD 2>/dev/null", home?home:".");
    FILE *f = popen(cmd, "r");
    if (!f) return NULL;
    char buf[128];
    if (fgets(buf, sizeof buf, f)) {
        size_t n = strlen(buf);
        while (n && (buf[n-1]=='\n'||buf[n-1]=='\r')) buf[--n]='\0';
        out = strdup(buf);
    }
    pclose(f);
    return out;
}

/* ── SpikeAgent: deterministic AIAgent-shaped probe ────────────────────
 * The C port models the interrupt + history surface; run_conversation is a
 * native loop over the conversation engine rather than an LLM client. */

typedef struct {
    char *session_id;
    json_t *history;
    int interrupt_flag;
} cg_spike_agent_t;

/* PoP: cg_spike_agent_new @ tui_gateway/compute_host.py:SpikeAgent */
cg_spike_agent_t *cg_spike_agent_new(const char *session_id) {
    cg_spike_agent_t *a = calloc(1, sizeof *a);
    if (!a) return NULL;
    a->session_id = strdup(session_id ? session_id : "");
    a->history = json_array();
    return a;
}

/* PoP: cg_spike_agent_free @ tui_gateway/compute_host.py:SpikeAgent (lifetime) */
void cg_spike_agent_free(cg_spike_agent_t *a) {
    if (!a) return;
    free(a->session_id); json_free(a->history); free(a);
}

/* PoP: cg_spike_agent_clear_interrupt @ tui_gateway/compute_host.py:SpikeAgent.clear_interrupt */
void cg_spike_agent_clear_interrupt(cg_spike_agent_t *a) { a->interrupt_flag = 0; }

/* PoP: cg_spike_agent_interrupt @ tui_gateway/compute_host.py:SpikeAgent.interrupt */
void cg_spike_agent_interrupt(cg_spike_agent_t *a, int hard_cancel) {
    (void)hard_cancel;
    a->interrupt_flag = 1;
}

/* PoP: cg_emit_write @ tui_gateway/compute_host.py:TeeTransport.write */
int cg_emit_write(json_t *sink, const char *obj_json) {
    if (!sink || !obj_json) return 0;
    /* TeeTransport semantics: parse-validate then forward. */
    json_t *v = json_parse(obj_json, NULL);
    if (!v) return 0;
    json_free(v);
    printf("%s\n", obj_json);
    return 1;
}
