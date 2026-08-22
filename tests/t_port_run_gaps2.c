/*
 * t_port_run_gaps2.c — parity harness for src/gateway/run_gaps2.c
 * (gateway/run.py pure REAL_GAP closures).
 *
 * Reads a JSON fixture (argv[1]) with "ops": list and emits one JSON object
 * per op, mirroring tests/sta_oracle_run_gaps2.py exactly.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "gateway_run_helpers.h"
#include "hermes_gateway_types.h"
#include "hermes_json.h"

/* run_gaps2 surface (no shared header yet) */
extern void gw_shorten_command_for_display(const char *command, int limit,
                                           char *out, size_t cap);
extern char *gw_format_concise_process_notification(const char *session_id,
                                                    const char *command,
                                                    int has_exit_code, long exit_code,
                                                    const char *output,
                                                    int has_duration, double duration_seconds);
extern char *gw_completion_notification_batch_key(const json_node_t *evt);
extern char *gw_async_delegation_group_key(const json_node_t *evt);
extern char *gw_format_coalesced_process_completions(const json_node_t *entries);
extern char *gw_format_coalesced_async_delegations(const json_node_t *blocks);
extern char *gw_final_text_for_post_turn_hooks(const json_node_t *agent_result,
                                               const json_node_t *event);
extern bool gw_reconnect_needs_attention(json_node_t *info, double now);
extern void gw_prepare_resume_pending_message(const char *reason, const char *message,
                                              bool interactive,
                                              char *recovery_out, size_t recovery_cap,
                                              char **persist_out);


/* Mirror python json.dumps(ensure_ascii=True): escape quote/backslash/control
 * chars, and render every non-ASCII codepoint as \uXXXX (surrogate pairs). */
static void jesc(const char *s, char *out, size_t cap)
{
    size_t o = 0;
    for (; *s && o + 8 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' ) { out[o++]='\\'; out[o++]='"'; }
        else if (c == '\\') { out[o++]='\\'; out[o++]='\\'; }
        else if (c == '\n') { out[o++]='\\'; out[o++]='n'; }
        else if (c == '\r') { out[o++]='\\'; out[o++]='r'; }
        else if (c == '\t') { out[o++]='\\'; out[o++]='t'; }
        else if (c == '\b') { out[o++]='\\'; out[o++]='b'; }
        else if (c == '\f') { out[o++]='\\'; out[o++]='f'; }
        else if (c < 0x20) { o += (size_t)snprintf(out+o, cap-o, "\\u%04x", c); }
        else if (c < 0x80) { out[o++] = (char)c; }
        else {
            int len = 0; unsigned cp = 0;
            if ((c >> 5) == 0x6) { cp = c & 0x1F; len = 2; }
            else if ((c >> 4) == 0xE) { cp = c & 0x0F; len = 3; }
            else { cp = c & 0x07; len = 4; }
            for (int k = 1; k < len && s[k]; k++) cp = (cp << 6) | (s[k] & 0x3F);
            s += len - 1;
            if (cp < 0x10000) o += (size_t)snprintf(out+o, cap-o, "\\u%04x", cp);
            else {
                cp -= 0x10000;
                o += (size_t)snprintf(out+o, cap-o, "\\u%04x\\u%04x",
                     0xD800 + (cp >> 10), 0xDC00 + (cp & 0x3FF));
            }
        }
    }
    out[o] = '\0';
}

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

static char *slurp_line(FILE *f) {
    static char buf[262144];
    if (!fgets(buf, sizeof buf, f)) return NULL;
    size_t n = strlen(buf);
    while (n && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    return buf;
}

static char EBUF[1048576];

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s fixture.json\n", argv[0]); return 2; }
    char *txt = read_file(argv[1]);
    json_node_t *fx = json_parse(txt, NULL);
    if (!fx) { fprintf(stderr, "bad fixture\n"); return 2; }
    json_node_t *ops = json_obj_get(fx, "ops");
    size_t n = json_is_array(ops) ? json_array_size(ops) : 0;
    for (size_t i = 0; i < n; i++) {
        json_node_t *op = json_get(ops, (int)i);
        const char *kind = json_get_str(op, "kind", "");
        if (strcmp(kind, "shorten") == 0) {
            char out[4096];
            gw_shorten_command_for_display(json_get_str(op, "command", ""),
                                           (int)json_get_num(op, "limit", 80),
                                           out, sizeof out);
            jesc(out, EBUF, sizeof EBUF);
            printf("{\"kind\": \"shorten\", \"out\": \"%s\"}\n", EBUF);
        } else if (strcmp(kind, "concise") == 0) {
            json_node_t *ec = json_obj_get(op, "exit_code");
            json_node_t *du = json_obj_get(op, "duration");
            char *s = gw_format_concise_process_notification(
                json_get_str(op, "session_id", ""),
                json_get_str(op, "command", ""),
                ec && ec->type == JSON_NUMBER ? 1 : 0,
                ec && ec->type == JSON_NUMBER ? (long)ec->num_val : 0,
                json_get_str(op, "output", ""),
                du && du->type == JSON_NUMBER ? 1 : 0,
                du ? du->num_val : 0.0);
            jesc(s ? s : "", EBUF, sizeof EBUF);
            printf("{\"kind\": \"concise\", \"out\": \"%s\"}\n", EBUF);
            free(s);
        } else if (strcmp(kind, "batch_key") == 0) {
            char *k = gw_completion_notification_batch_key(json_obj_get(op, "evt"));
            jesc(k ? k : "", EBUF, sizeof EBUF);
            printf("{\"kind\": \"batch_key\", \"key\": \"%s\"}\n", EBUF);
            free(k);
        } else if (strcmp(kind, "adel_key") == 0) {
            char *k = gw_async_delegation_group_key(json_obj_get(op, "evt"));
            jesc(k ? k : "", EBUF, sizeof EBUF);
            printf("{\"kind\": \"adel_key\", \"key\": \"%s\"}\n", EBUF);
            free(k);
        } else if (strcmp(kind, "coalesce") == 0) {
            char *s = gw_format_coalesced_process_completions(json_obj_get(op, "entries"));
            jesc(s ? s : "", EBUF, sizeof EBUF);
            printf("{\"kind\": \"coalesce\", \"text\": \"%s\"}\n", EBUF);
            free(s);
        } else if (strcmp(kind, "coalesce_adel") == 0) {
            char *s = gw_format_coalesced_async_delegations(json_obj_get(op, "blocks"));
            jesc(s ? s : "", EBUF, sizeof EBUF);
            printf("{\"kind\": \"coalesce_adel\", \"text\": \"%s\"}\n", EBUF);
            free(s);
        } else if (strcmp(kind, "final_text") == 0) {
            char *s = gw_final_text_for_post_turn_hooks(json_obj_get(op, "agent_result"),
                                                        json_obj_get(op, "event"));
            jesc(s ? s : "", EBUF, sizeof EBUF);
            printf("{\"kind\": \"final_text\", \"text\": \"%s\"}\n", EBUF);
            free(s);
        } else if (strcmp(kind, "reconnect") == 0) {
            json_node_t *info = json_copy(json_obj_get(op, "info"));
            bool r = gw_reconnect_needs_attention(info, json_get_num(op, "now", 0.0));
            json_node_t *qa = info ? json_obj_get(info, "queued_at") : NULL;
            char qab[64] = "null";
            if (qa && qa->type == JSON_NUMBER) {
                double v = qa->num_val;
                if (v == (double)(long long)v) snprintf(qab, sizeof qab, "%.1f", v);
                else snprintf(qab, sizeof qab, "%g", v);
            }
            printf("{\"kind\": \"reconnect\", \"needs\": %s, \"queued_at\": %s}\n",
                   r ? "true" : "false", qab);
            if (info) json_free(info);
        } else if (strcmp(kind, "resume_msg") == 0) {
            char rec[8192]; char *persist = NULL;
            gw_prepare_resume_pending_message(
                json_get_str(op, "reason", ""), json_get_str(op, "message", ""),
                json_get_bool(op, "interactive", true),
                rec, sizeof rec, &persist);
            jesc(rec, EBUF, sizeof EBUF);
            printf("{\"kind\": \"resume_msg\", \"recovery\": \"%s\"", EBUF);
            char EBUF2[262144];
            jesc(persist ? persist : "", EBUF2, sizeof EBUF2);
            printf(", \"persist\": \"%s\"}\n", EBUF2);
            free(persist);
        }
    }
    return 0;
}
