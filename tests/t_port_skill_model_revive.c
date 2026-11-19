/* t_port_skill_model_revive.c — differential harness for the wave-2 revival of
 * lib/libskillusage/skill_usage.c + src/agent/model_metadata_gaps.c.
 *
 * Self-contained: #includes the two port .c files (lib/libskillusage/skill_usage.o
 * is NOT in the binary link list, so the harness supplies it by inclusion). Links
 * against lib/libjson/json.o only. Gate:
 *   bash tests/run_one_oracle.sh skill_model_revive oracle_skill_model_revive.py "" "lib/libjson/json.o"
 *
 * Reads JSONL cases {func, in} from stdin (auto-fed tests/fixtures/skill_model_revive.jsonl),
 * echoes {func, in, out}. Flow funcs emit an ordered seam trace; the oracle diffs
 * trace + ret against the live Python driven by the SAME scripts.
 */
#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "hermes_json.h"

/* Area-qualified includes: -I src precedes the src subdirs, and src/tools has a
 * DIFFERENT skill_usage.c — the qualified path picks the lib copy unambiguously. */
#include "libskillusage/skill_usage.c"
#include "agent/model_metadata_gaps.c"

/* ---- recording seam shared by both ports ---- */
typedef struct { json_t *io; json_t *trace; } rev_ctx_t;

static int rev_is_path_keyed(const char *name)
{
    return strcmp(name, "exists") == 0 || strcmp(name, "is_dir") == 0 ||
           strcmp(name, "is_file") == 0 || strcmp(name, "read_skill_name") == 0;
}

/* Interpret a script entry identically to the oracle's materialize(). */
static json_t *rev_materialize(const char *name, const json_t *args, const json_t *script)
{
    if (script && script->type == JSON_OBJECT && json_obj_get(script, "__raise__"))
        return json_copy(script);
    if (rev_is_path_keyed(name)) {
        const char *key = args ? json_get_str(args, "path", NULL) : NULL;
        const json_t *v = (script && script->type == JSON_OBJECT && key) ? json_obj_get(script, key) : NULL;
        if (v) return json_copy(v);
        if (strcmp(name, "read_skill_name") == 0)
            return json_string(args ? json_get_str(args, "fallback", "") : "");
        return json_bool(false);
    }
    if (!script) return json_null();
    return json_copy(script);
}

static json_t *rev_seam(void *ctx, const char *name, const json_t *args)
{
    rev_ctx_t *rc = (rev_ctx_t *)ctx;
    const json_t *script = rc->io ? json_obj_get(rc->io, name) : NULL;
    json_t *ret = rev_materialize(name, args, script);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_object());
    json_set(ev, "ret", json_copy(ret));
    json_append(rc->trace, ev);
    return ret;
}

/* ---- record struct projection (string fields; empty == absent) ---- */
static void rev_build_record(const json_t *jr, skill_usage_record_t *r)
{
    memset(r, 0, sizeof(*r));
    if (!jr || jr->type != JSON_OBJECT) return;
    static const char *fields[] = {
        "name", "created_by", "last_used_at", "last_viewed_at", "last_patched_at",
        "created_at", "state", "archived_at", NULL
    };
    char *dst[] = {
        r->name, r->created_by, r->last_used_at, r->last_viewed_at, r->last_patched_at,
        r->created_at, r->state, r->archived_at
    };
    size_t caps[] = {
        sizeof(r->name), sizeof(r->created_by), sizeof(r->last_used_at), sizeof(r->last_viewed_at),
        sizeof(r->last_patched_at), sizeof(r->created_at), sizeof(r->state), sizeof(r->archived_at)
    };
    for (int i = 0; fields[i]; i++) {
        const char *v = json_get_str(jr, fields[i], NULL);
        if (v) { strncpy(dst[i], v, caps[i] - 1); dst[i][caps[i] - 1] = '\0'; }
    }
}

static json_t *rev_run(const char *func, const json_t *in)
{
    json_t *out = json_object();
    json_t *io = json_obj_get(in, "io");
    json_t *trace = json_array();
    rev_ctx_t ctx = { io, trace };

    if (strcmp(func, "latest_activity_at") == 0) {
        skill_usage_record_t r;
        rev_build_record(json_obj_get(in, "record"), &r);
        char buf[SKILL_USAGE_MAX_VALUE];
        const char *raw = skill_usage_latest_activity_at_str(&r, buf, sizeof(buf));
        double ep = skill_usage_latest_activity_at(&r);
        json_set(out, "raw", raw ? json_string(raw) : json_null());
        json_set(out, "epoch", json_number(ep));
        json_free(trace);
        return out;
    }
    if (strcmp(func, "_external_read_only_message") == 0) {
        const char *name = json_get_str(in, "skill_name", NULL);
        const char *msg = skill_usage_external_read_only_message(name);
        json_set(out, "msg", json_string(msg ? msg : ""));
        json_free(trace);
        return out;
    }
    if (strcmp(func, "_stale_pre_catalog_cache_entry") == 0) {
        const char *model = json_get_str(in, "model", "");
        long long cached = (long long)json_get_num(in, "cached", 0);
        int v = stale_pre_catalog_cache_entry(model, cached);
        json_set(out, "value", json_bool(v ? true : false));
        json_free(trace);
        return out;
    }
    if (strcmp(func, "_verified_codex_ctx_for_slug") == 0) {
        const char *model = json_get_str(in, "model_bare", NULL);
        long long v = 0;
        int rc = verified_codex_ctx_for_slug(model, &v);
        json_set(out, "value", rc ? json_number((double)v) : json_null());
        json_free(trace);
        return out;
    }
    if (strcmp(func, "_prune_builtins_enabled") == 0) {
        skill_usage_set_seam(rev_seam, &ctx);
        bool v = skill_usage_prune_builtins_enabled();
        skill_usage_set_seam(NULL, NULL);
        json_set(out, "value", json_bool(v));
        json_set(out, "trace", trace);
        return out;
    }
    if (strcmp(func, "is_hub_installed") == 0 || strcmp(func, "is_bundled") == 0) {
        skill_usage_record_t r;
        rev_build_record(json_obj_get(in, "record"), &r);
        skill_usage_set_seam(rev_seam, &ctx);
        bool v = (strcmp(func, "is_hub_installed") == 0)
                     ? skill_usage_is_hub_installed(&r)
                     : skill_usage_is_bundled(&r);
        skill_usage_set_seam(NULL, NULL);
        json_set(out, "value", json_bool(v));
        json_set(out, "trace", trace);
        return out;
    }
    if (strcmp(func, "anchored_context_tokens") == 0) {
        const json_t *messages = json_obj_get(in, "messages");
        const json_t *anchor = json_obj_get(in, "anchor");
        bool charge = json_get_bool(in, "charge_stale_thinking", true);
        model_metadata_gaps_set_seam(rev_seam, &ctx);
        long long v = 0;
        int rc = anchored_context_tokens(messages, anchor, charge, &v);
        model_metadata_gaps_set_seam(NULL, NULL);
        json_set(out, "value", rc ? json_number((double)v) : json_null());
        json_set(out, "trace", trace);
        return out;
    }
    if (strcmp(func, "restore_skill") == 0) {
        const char *home = json_get_str(in, "home", "");
        const char *name = json_get_str(in, "name", "");
        char msg[SKILL_USAGE_MAX_VALUE];
        skill_usage_set_seam(rev_seam, &ctx);
        int rc = skill_usage_restore(home, name, msg);
        skill_usage_set_seam(NULL, NULL);
        json_set(out, "rc", json_number(rc));
        json_set(out, "msg", json_string(msg));
        json_set(out, "trace", trace);
        return out;
    }
    json_set(out, "error", json_string("unknown func"));
    json_free(trace);
    return out;
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    /* read stdin line by line */
    while (1) {
        /* manual fgets growth to avoid getline portability assumptions */
        char buf[65536];
        if (!fgets(buf, sizeof(buf), stdin)) break;
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
        if (len == 0) continue;
        char *err = NULL;
        json_t *rec = json_parse(buf, &err);
        if (err) free(err);
        if (!rec) continue;
        const char *func = json_get_str(rec, "func", NULL);
        json_t *in = json_obj_get(rec, "in");
        if (!func || !in) { json_free(rec); continue; }
        json_t *out = rev_run(func, in);
        json_t *echo = json_object();
        json_set(echo, "func", json_string(func));
        json_set(echo, "in", json_copy(in));
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    return 0;
}