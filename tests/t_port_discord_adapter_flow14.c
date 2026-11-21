/* t_port_discord_adapter_flow14.c -- harness for the Discord adapter FLOW
 * part 14 port (src/gateway/platforms/discord_adapter_flow14.c).
 *
 * Reads JSON LINES {"func": <name>, "in": {...}} from stdin and emits
 * {"func", "in", "out"} where out captures the ordered seam trace, the final
 * return ("ret") and the state echo. The echoed "in" is the ORIGINAL input.
 */
#include "gateway/platforms/discord_adapter_flow14.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da14_fn_t)(da14_ctx_t *, const json_t *);
typedef struct { const char *func; da14_fn_t fn; } da14_case_t;

static const da14_case_t CASES[] = {
    { "_record_command_sync_attempt", da14_record_command_sync_attempt },
    { "_record_command_sync_success", da14_record_command_sync_success },
    { "_missed_message_backfill_enabled", da14_missed_message_backfill_enabled },
    { "_record_discord_message_seen", da14_record_discord_message_seen },
    { "_record_recovery_attempt", da14_record_recovery_attempt },
    { "_record_discord_processing_start", da14_record_discord_processing_start },
    { "_record_discord_processing_complete", da14_record_discord_processing_complete },
    { "_record_response_async", da14_record_response_async },
    { "_record_discord_response", da14_record_discord_response },
    { "_record_recovery_scan_start", da14_record_recovery_scan_start },
    { "_record_recovery_scan_complete", da14_record_recovery_scan_complete },
    { "_refresh_skill_catalog_state", da14_refresh_skill_catalog_state },
    { "_register_skill_group", da14_register_skill_group },
    { "_register_slash_commands", da14_register_slash_commands },
    { "_register_thread_slash", da14_register_thread_slash },
    { "_reject_slash", da14_reject_slash },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

static void inject(da14_ctx_t *ctx, const char *func, const json_t *args) {
    if (strcmp(func, "_missed_message_backfill_enabled") == 0) {
        json_t *mmb = json_object();
        json_set(mmb, "missed_message_backfill", da14_copyv(da14_arg(args, "config_extra")));
        json_t *extra = json_object(); json_set(extra, "extra", mmb);
        json_set(ctx->state, "config", extra);
    } else if (strcmp(func, "_refresh_skill_catalog_state") == 0) {
        const json_t *rn = da14_arg(args, "reserved_names");
        json_set(ctx->state, "_skill_group_reserved_names", da14_copyv(rn));
    } else if (strcmp(func, "_register_skill_group") == 0) {
        json_set(ctx->state, "name", json_string(da14_arg_str(args, "name", "bot")));
    } else if (strcmp(func, "_register_slash_commands") == 0) {
        json_set(ctx->state, "name", json_string(da14_arg_str(args, "name", "bot")));
        if (da14_truthy(da14_arg(args, "no_client"))) {
            json_set(ctx->state, "_client", json_null());
        } else {
            json_t *c = json_object();
            json_set(c, "tree", json_string("<tree>"));
            json_set(ctx->state, "_client", c);
        }
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const da14_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da14_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            inject(&ctx, func, args);
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}
