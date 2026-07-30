/*
 * t_port_web_server_prune.c — oracle harness for the prune engine. Ops:
 *   {"op":"candidates","db":"...","older":<num|null|absent>,"filters":{...}}
 *   {"op":"prune","db":"...","older":...,"filters":{...}}  (+post-state)
 *   {"op":"endpoint","db":"...","body":{...}}
 * Filters use the Python kwarg names.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "web_server_prune.h"
#include "web_server_sessions_admin.h"

static const char *gs(json_t *o, const char *k) {
    json_t *v = o ? json_object_get(o, k) : NULL;
    return (v && v->type == JSON_STRING && v->str_val[0]) ? v->str_val : NULL;
}

static bool gn(json_t *o, const char *k, double *out) {
    json_t *v = o ? json_object_get(o, k) : NULL;
    if (!v || v->type != JSON_NUMBER) return false;
    *out = v->num_val;
    return true;
}

static void fill_filters(json_t *jf, ws_prune_filters_t *f) {
    memset(f, 0, sizeof *f);
    f->archived = -1;
    double d;
    if (gn(jf, "last_active_before", &d)) { f->has_last_active_before = true; f->last_active_before = d; }
    if (gn(jf, "last_active_after", &d)) { f->has_last_active_after = true; f->last_active_after = d; }
    if (gn(jf, "started_before", &d)) { f->has_started_before = true; f->started_before = d; }
    if (gn(jf, "started_after", &d)) { f->has_started_after = true; f->started_after = d; }
    f->source = gs(jf, "source");
    f->title_like = gs(jf, "title_like");
    f->end_reason = gs(jf, "end_reason");
    f->cwd_prefix = gs(jf, "cwd_prefix");
    f->model_like = gs(jf, "model_like");
    f->provider = gs(jf, "provider");
    f->user_id = gs(jf, "user_id");
    f->chat_id = gs(jf, "chat_id");
    f->chat_type = gs(jf, "chat_type");
    f->branch_like = gs(jf, "branch_like");
    if (gn(jf, "min_messages", &d)) { f->has_min_messages = true; f->min_messages = (int)d; }
    if (gn(jf, "max_messages", &d)) { f->has_max_messages = true; f->max_messages = (int)d; }
    if (gn(jf, "min_tokens", &d)) { f->has_min_tokens = true; f->min_tokens = (long long)d; }
    if (gn(jf, "max_tokens", &d)) { f->has_max_tokens = true; f->max_tokens = (long long)d; }
    if (gn(jf, "min_cost", &d)) { f->has_min_cost = true; f->min_cost = d; }
    if (gn(jf, "max_cost", &d)) { f->has_max_cost = true; f->max_cost = d; }
    if (gn(jf, "min_tool_calls", &d)) { f->has_min_tool_calls = true; f->min_tool_calls = (int)d; }
    if (gn(jf, "max_tool_calls", &d)) { f->has_max_tool_calls = true; f->max_tool_calls = (int)d; }
    json_t *arch = jf ? json_object_get(jf, "archived") : NULL;
    if (arch && arch->type == JSON_BOOL) f->archived = arch->bool_val ? 1 : 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    FILE *fp = fopen(argv[1], "rb");
    if (!fp) return 2;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);
    json_t *fx = json_parse(buf, NULL);
    if (!fx) return 2;
    const char *op = json_get_str(fx, "op", "");
    const char *db = json_get_str(fx, "db", "");

    json_t *older_v = json_object_get(fx, "older");
    bool has_older = older_v && older_v->type == JSON_NUMBER;
    double older = has_older ? older_v->num_val : 0;

    ws_prune_filters_t f;
    fill_filters(json_object_get(fx, "filters"), &f);

    if (strcmp(op, "candidates") == 0) {
        json_t *rows = ws_prune_candidates(db, has_older, older, &f);
        char *d = json_dumps(rows, 0);
        printf("%s\n", d);
        free(d);
        json_free(rows);
    } else if (strcmp(op, "prune") == 0) {
        int removed = ws_prune_sessions(db, has_older, older, &f);
        ws_session_count_opts_t all = {.include_archived = true};
        printf("{\"removed\":%d,\"remaining\":%d}\n", removed,
               ws_sessions_session_count(db, &all));
    } else if (strcmp(op, "endpoint") == 0) {
        json_t *r = ws_prune_endpoint(db, json_object_get(fx, "body"));
        char *d = json_dumps(r, 0);
        printf("%s\n", d);
        free(d);
        json_free(r);
    } else {
        return 2;
    }
    json_free(fx);
    return 0;
}
