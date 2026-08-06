/*
 * t_port_hermes_cli_shared_metrics_contract.c — Oracle harness for
 * shared_metrics_contract pure functions.
 *
 * Reads packed test vectors from the fixture file (argv[1]).
 * Each line: op|arg0|arg1|...
 * Outputs JSON for each case.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/port_hermes_cli_shared_metrics_contract.h"

/* ── JSON emit helpers ─────────────────────────────────────────── */
static char *json_esc(const char *s) {
    if (!s) return strdup("null");
    size_t need = 3;
    for (const char *p = s; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') need += 2;
        else if (c < 0x20) need += 6;
        else need++;
    }
    char *out = malloc(need);
    if (!out) return NULL;
    char *q = out;
    *q++ = '"';
    for (const char *p = s; *p; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
        else if (c < 0x20) {
            *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
            *q++ = "0123456789abcdef"[c >> 4];
            *q++ = "0123456789abcdef"[c & 15];
        } else *q++ = c;
    }
    *q++ = '"'; *q = 0;
    return out;
}

static void emit_json(const char *op, const char *in_json, const char *out_json) {
    printf("{\"op\":%s,\"in\":%s,\"out\":%s}\n",
           op ? op : "null",
           in_json ? in_json : "null",
           out_json ? out_json : "null");
}

static void emit_error(const char *op, const char *msg) {
    char *emsg = json_esc(msg);
    printf("{\"op\":%s,\"error\":%s}\n", op ? op : "null", emsg);
    free(emsg);
}

/* ── Tokenizer (parse "a|b|c" into array) ─────────────────────── */
static char **split_line(const char *line, int *out_count) {
    if (!line || !*line) { *out_count = 0; return NULL; }
    int cap = 32, count = 0;
    char **toks = malloc(cap * sizeof(char*));
    const char *p = line;
    while (*p) {
        const char *start = p;
        while (*p && *p != '|') p++;
        size_t len = (size_t)(p - start);
        toks[count] = malloc(len + 1);
        memcpy(toks[count], start, len);
        toks[count][len] = 0;
        count++;
        if (count >= cap) {
            cap *= 2;
            toks = realloc(toks, cap * sizeof(char*));
        }
        if (*p) p++;
    }
    *out_count = count;
    return toks;
}

static void free_toks(char **toks, int count) {
    for (int i = 0; i < count; i++) free(toks[i]);
    free(toks);
}

/* ── JSON in/out builders ─────────────────────────────────────── */
static char *json_str(const char *s) {
    if (!s) return strdup("null");
    return json_esc(s);
}

static char *json_int(int v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", v);
    return strdup(buf);
}

static char *json_bool(bool v) {
    return strdup(v ? "true" : "false");
}

static char *json_obj(const char *keyvals) {
    return strdup(keyvals);
}

/* ═══════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════ */
int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <fixture_file>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 1; }

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = 0;
        if (!len || line[0] == '#') continue;

        int argc = 0;
        char **argv = split_line(line, &argc);
        if (argc < 1) { free_toks(argv, argc); continue; }

        const char *op = argv[0];

        if (strcmp(op, "duration_bucket") == 0) {
            int ms = (argc > 1) ? atoi(argv[1]) : 0;
            const char *result = smc_duration_bucket(ms);
            emit_json(json_esc(op), json_int(ms), json_esc(result));

        } else if (strcmp(op, "count_bucket") == 0) {
            int c = (argc > 1) ? atoi(argv[1]) : 0;
            const char *result = smc_count_bucket(c);
            emit_json(json_esc(op), json_int(c), json_esc(result));

        } else if (strcmp(op, "execution_surface") == 0) {
            const char *val = (argc > 1) ? argv[1] : "";
            const char *result = smc_execution_surface(val);
            emit_json(json_esc(op), json_esc(val), json_esc(result));

        } else if (strcmp(op, "task_entrypoint") == 0) {
            const char *ep = (argc > 1) ? argv[1] : "";
            const char *surface = (argc > 2) ? argv[2] : "";
            bool has_parent_task = (argc > 3 && strcmp(argv[3], "true") == 0);
            bool has_parent_session = (argc > 4 && strcmp(argv[4], "true") == 0);
            const char *result = smc_task_entrypoint(ep, surface, has_parent_task, has_parent_session);
            char in_buf[512];
            snprintf(in_buf, sizeof(in_buf),
                "{\"ep\":%s,\"surface\":%s,\"parent_task\":%s,\"parent_session\":%s}",
                json_esc(ep), json_esc(surface),
                has_parent_task ? "true" : "false",
                has_parent_session ? "true" : "false");
            emit_json(json_esc(op), in_buf, json_esc(result));

        } else if (strcmp(op, "task_start_fields") == 0) {
            const char *ep = (argc > 1) ? argv[1] : "";
            const char *surface = (argc > 2) ? argv[2] : "";
            char *result = smc_task_start_fields(ep, surface);
            char in_buf[512];
            snprintf(in_buf, sizeof(in_buf),
                "{\"entrypoint\":%s,\"platform\":%s}",
                json_esc(ep), json_esc(surface));
            emit_json(json_esc(op), in_buf, result ? result : "null");
            free(result);

        } else if (strcmp(op, "task_terminal_fields") == 0) {
            const char *ep = (argc > 1) ? argv[1] : "";
            const char *surface = (argc > 2) ? argv[2] : "";
            int dur_ms = (argc > 3) ? atoi(argv[3]) : 0;
            int mc = (argc > 4) ? atoi(argv[4]) : 0;
            int tc = (argc > 5) ? atoi(argv[5]) : 0;
            int rc = (argc > 6) ? atoi(argv[6]) : 0;
            char *result = smc_task_terminal_fields(ep, surface, dur_ms, mc, tc, rc);
            char in_buf[1024];
            snprintf(in_buf, sizeof(in_buf),
                "{\"ep\":%s,\"surface\":%s,\"dur_ms\":%d,\"mc\":%d,\"tc\":%d,\"rc\":%d}",
                json_esc(ep), json_esc(surface), dur_ms, mc, tc, rc);
            emit_json(json_esc(op), in_buf, result ? result : "null");
            free(result);

        } else if (strcmp(op, "task_terminal_state") == 0) {
            const char *reason = (argc > 1) ? argv[1] : "";
            bool interrupted = (argc > 2 && strcmp(argv[2], "true") == 0);
            bool completed = (argc > 3 && strcmp(argv[3], "true") == 0);
            bool failed = (argc > 4 && strcmp(argv[4], "true") == 0);
            char *result = smc_task_terminal_state(reason, interrupted, completed, failed);
            char in_buf[512];
            snprintf(in_buf, sizeof(in_buf),
                "{\"reason\":%s,\"interrupted\":%s,\"completed\":%s,\"failed\":%s}",
                json_esc(reason),
                interrupted ? "true" : "false",
                completed ? "true" : "false",
                failed ? "true" : "false");
            emit_json(json_esc(op), in_buf, result ? result : "null");
            free(result);

        } else if (strcmp(op, "model_family") == 0) {
            const char *declared = (argc > 1) ? argv[1] : "";
            const char *model = (argc > 2) ? argv[2] : "";
            const char *resp = (argc > 3) ? argv[3] : "";
            const char *result = smc_model_family(declared, model, resp);
            char in_buf[512];
            snprintf(in_buf, sizeof(in_buf),
                "{\"declared\":%s,\"model\":%s,\"response_model\":%s}",
                json_esc(declared), json_esc(model), json_esc(resp));
            emit_json(json_esc(op), in_buf, json_esc(result));

        } else if (strcmp(op, "model_call_outcome") == 0) {
            const char *outcome = (argc > 1) ? argv[1] : "";
            const char *result = smc_model_call_outcome(outcome);
            emit_json(json_esc(op), json_esc(outcome), json_esc(result));

        } else if (strcmp(op, "provider_family") == 0) {
            const char *provider = (argc > 1) ? argv[1] : "";
            const char *result = smc_provider_family(provider);
            emit_json(json_esc(op), json_esc(provider), json_esc(result));

        } else if (strcmp(op, "counter_dims_valid") == 0) {
            const char *metric = (argc > 1) ? argv[1] : "";
            /* Build key/value arrays for smc_counter_dimensions_are_valid */
            int n_dims = argc - 2;
            if (n_dims > 32) n_dims = 32;
            const char *keys[32], *vals[32];
            for (int i = 0; i < n_dims; i++) {
                char *kv = argv[2 + i];
                char *eq = strchr(kv, '=');
                if (eq) {
                    keys[i] = kv;
                    *eq = 0;
                    vals[i] = eq + 1;
                } else {
                    keys[i] = kv;
                    vals[i] = "";
                }
            }
            bool result = smc_counter_dimensions_are_valid(
                metric, keys, vals, (size_t)n_dims);
            char in_buf[1024] = {0};
            strcat(in_buf, "{\"metric\":");
            char *ms = json_esc(metric);
            strcat(in_buf, ms);
            free(ms);
            strcat(in_buf, ",\"dims\":{");
            for (int i = 0; i < n_dims; i++) {
                if (i > 0) strcat(in_buf, ",");
                char *ks = json_esc(keys[i]);
                char *vs = json_esc(vals[i]);
                strcat(in_buf, ks);
                strcat(in_buf, ":");
                strcat(in_buf, vs);
                free(ks); free(vs);
            }
            strcat(in_buf, "}}");
            emit_json(json_esc(op), in_buf, result ? "true" : "false");

        } else {
            emit_error(json_esc(op), "unknown op");
        }

        free_toks(argv, argc);
    }

    fclose(f);
    return 0;
}