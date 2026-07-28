/*
 * t_port_cron_scheduler_orchestr.c — behavioral oracle harness for the
 * cron/scheduler.py ORCHESTRATION + DELIVERY runtime surface ported in
 * port_cron_scheduler_runtime_impl.c.
 *
 * Reads a fixture path from argv[1]; the fixture is a JSON object:
 *   { "op": "route_media"|"no_agent"|"summarize_fail", ... }
 * Emits a single JSON object (printed to stdout) describing the C result,
 * which the Python oracle (sta_oracle_cron_scheduler_orchestr.py) compares.
 *
 * We normalize timestamps out of the comparison (now_iso() is non-
 * deterministic) — the Python oracle compares structural invariants, not
 * the literal Run Time line.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "cron_scheduler_runtime.h"
#include "cron_scheduler_delivery.h"
#include "cron_jobs.h"

/* Minimal JSON-string escaper (allocates; caller frees). */
static char *jstr(const char *s)
{
    if (!s) return strdup("null");
    size_t n = 0;
    for (const char *p = s; *p; p++) {
        if (*p == '"' || *p == '\\' || *p == '\n' || *p == '\r' || *p == '\t')
            n += 2; else n += 1;
    }
    char *out = malloc(n + 3);
    char *o = out;
    *o++ = '"';
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '"': *o++ = '\\'; *o++ = '"'; break;
        case '\\': *o++ = '\\'; *o++ = '\\'; break;
        case '\n': *o++ = '\\'; *o++ = 'n'; break;
        case '\r': *o++ = '\\'; *o++ = 'r'; break;
        case '\t': *o++ = '\\'; *o++ = 't'; break;
        default: *o++ = *p; break;
        }
    }
    *o++ = '"'; *o = '\0';
    return out;
}

static void emit_key(const char *k) { printf("\"%s\":", k); }

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture.json>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f); buf[sz] = '\0';
    fclose(f);

    json_t *fx = json_parse(buf, NULL);
    free(buf);
    if (!fx || !json_node_is_object(fx)) { fprintf(stderr, "bad fixture\n"); return 2; }

    const char *op = json_get_str(fx, "op", "");
    printf("{");

    if (strcmp(op, "route_media") == 0) {
        const char *platform = json_get_str(fx, "platform", "telegram");
        const char *path     = json_get_str(fx, "path", "");
        bool is_voice        = json_get_bool(fx, "is_voice", false);
        scheduler_media_kind_t k = scheduler_route_media(platform, path, is_voice);
        emit_key("kind");
        printf("%d", (int)k);
    }
    else if (strcmp(op, "summarize_fail") == 0) {
        const char *jid  = json_get_str(fx, "job_id", "j1");
        const char *err  = json_get_str(fx, "error", "boom");
        char *out = scheduler_summarize_cron_failure_for_delivery(jid, err);
        emit_key("text"); printf("%s", jstr(out));
        free(out);
    }
    else if (strcmp(op, "no_agent") == 0) {
        const char *script = json_get_str(fx, "script", NULL);
        const char *name   = json_get_str(fx, "name", "job");
        const char *jid    = json_get_str(fx, "id", "j1");
        json_t *job = json_object();
        json_set(job, "id", json_string(jid));
        json_set(job, "name", json_string(name));
        if (script) json_set(job, "script", json_string(script));
        json_set(job, "no_agent", json_bool(true));

        char *doc = NULL, *final = NULL, *err = NULL;
        bool ok = scheduler_run_job_no_agent(job, &doc, &final, &err);
        emit_key("ok");      printf("%s", ok ? "true" : "false");
        emit_key("silent");  printf("%s", (final && strcmp(final, SCHEDULER_SILENT_MARKER) == 0) ? "true" : "false");
        emit_key("final");   printf("%s", jstr(final));
        emit_key("doc_head");
        /* strip the "**Run Time:** ...\n" segment for determinism */
        if (doc) {
            char *p = strstr(doc, "**Run Time:**");
            if (p) { char *nl = strchr(p, '\n'); if (nl) *nl = '\0'; }
            printf("%s", jstr(doc));
        } else printf("null");
        emit_key("err"); printf("%s", jstr(err));
        free(doc); free(final); free(err);
        json_free(job);
    }
    else {
        emit_key("error"); printf("\"unknown op\"");
    }

    printf("}\n");
    if (fx) json_free(fx);
    return 0;
}
