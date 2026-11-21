/* t_port_discord_adapter_flow15.c -- harness for the Discord adapter FLOW
 * part 15 port (src/gateway/platforms/discord_adapter_flow15.c).
 *
 * Reads JSON LINES {"func": <name>, "in": {...}} from stdin and emits
 * {"func", "in", "out"} where out captures the ordered seam trace, the final
 * return ("ret") and the state echo. The echoed "in" is the ORIGINAL input.
 */
#include "gateway/platforms/discord_adapter_flow15.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da15_fn_t)(da15_ctx_t *, const json_t *);
typedef struct { const char *func; da15_fn_t fn; } da15_case_t;

static const da15_case_t CASES[] = {
    { "_reset_voice_timeout", da15_reset_voice_timeout },
    { "_voice_timeout_limit", da15_voice_timeout_limit },
    { "_playback_timeout_limit", da15_playback_timeout_limit },
    { "_resolve_channel", da15_resolve_channel },
    { "_resolve_interaction_channel", da15_resolve_interaction_channel },
    { "_resolve_channel_skills", da15_resolve_channel_skills },
    { "_resolve_credentials", da15_resolve_credentials },
    { "_standalone_read_json_limited", da15_standalone_read_json_limited },
    { "_standalone_is_forum", da15_standalone_is_forum },
    { "_should_backfill_discord_message", da15_should_backfill_discord_message },
    { "_self_contained_prompt_content", da15_self_contained_prompt_content },
    { "_run_simple_slash", da15_run_simple_slash },
    { "_safe_sync_slash_commands", da15_safe_sync_slash_commands },
    { "_run_post_connect_initialization", da15_run_post_connect_initialization },
    { "_run_missed_message_backfill", da15_run_missed_message_backfill },
    { "_resolve_allowed_usernames", da15_resolve_allowed_usernames },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 8192, len = 0;
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

static void inject(da15_ctx_t *ctx, const char *func, const json_t *args) {
    json_t *st = ctx->state;
    if (strcmp(func, "_reset_voice_timeout") == 0) {
        json_set(st, "_voice_timeout_tasks", json_object());
    } else if (strcmp(func, "_resolve_channel") == 0) {
        json_set(st, "_client", json_object());
    } else if (strcmp(func, "_resolve_interaction_channel") == 0) {
        json_set(st, "_client", da15_truthy(da15_arg(args, "no_client")) ? json_null() : json_object());
    } else if (strcmp(func, "_resolve_channel_skills") == 0) {
        json_t *cfg = json_object();
        json_t *extra = json_object();
        const json_t *ex = da15_arg(args, "extra");
        if (ex && ex->type == JSON_OBJECT) json_free(extra), extra = json_copy((json_t *)ex);
        json_set(cfg, "extra", extra);
        json_set(st, "config", cfg);
    } else if (strcmp(func, "_should_backfill_discord_message") == 0) {
        if (da15_truthy(da15_arg(args, "no_client"))) {
            json_set(st, "_client", json_null());
        } else {
            json_t *u = json_object(); json_set(u, "id", da15_copyv(da15_arg(args, "user_id")));
            json_t *c = json_object(); json_set(c, "user", u);
            json_set(st, "_client", c);
        }
    } else if (strcmp(func, "_safe_sync_slash_commands") == 0 ||
               strcmp(func, "_run_post_connect_initialization") == 0) {
        if (strcmp(func, "_run_post_connect_initialization") == 0)
            json_set(st, "name", json_string(da15_arg_str(args, "name", "bot")));
        const json_t *cl = da15_arg(args, "_client");
        if (!cl) cl = da15_state_get(ctx, "_client");
        if (cl && cl->type == JSON_OBJECT) {
            json_t *o = json_object();
            json_set(o, "tree", json_string("<tree>"));
            json_t *http = json_object();
            json_set(http, "max_ratelimit_timeout",
                     da15_copyv(da15_getattr(cl, "http")) == NULL ? json_null()
                       : da15_copyv(da15_getattr(da15_getattr(cl, "http"), "max_ratelimit_timeout")));
            json_set(o, "http", http);
            json_set(o, "application_id", da15_copyv(da15_getattr(cl, "application_id")));
            json_t *u = json_object();
            json_set(u, "id", da15_copyv(da15_getattr(da15_getattr(cl, "user"), "id")));
            json_set(o, "user", u);
            json_set(st, "_client", o);
        } else {
            json_set(st, "_client", json_null());
        }
    } else if (strcmp(func, "_run_missed_message_backfill") == 0) {
        const json_t *cl = da15_state_get(ctx, "_client");
        json_set(st, "_client", (cl && cl->type == JSON_NULL) ? json_null() : json_object());
        json_set(st, "name", json_string(da15_arg_str(args, "name", "bot")));
        json_set(st, "_dedup", json_object());
    } else if (strcmp(func, "_resolve_allowed_usernames") == 0) {
        const json_t *gs = da15_arg(args, "guilds");
        json_set(st, "_client", (gs && gs->type == JSON_ARRAY) ? json_object() : json_null());
        if (gs && gs->type == JSON_ARRAY) {
            json_t *go = json_object(); json_set(go, "guilds", json_copy((json_t *)gs));
            json_set(st, "_client", go);
        }
        json_set(st, "_allowed_user_ids", da15_copyv(da15_arg(args, "allowed_user_ids")));
        json_set(st, "name", json_string(da15_arg_str(args, "name", "bot")));
        const json_t *snap = da15_arg(args, "gate_env_snapshot");
        if (snap && snap->type == JSON_OBJECT) json_set(st, "_gate_env_snapshot", json_copy((json_t *)snap));
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

        const da15_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da15_ctx_t ctx;
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
