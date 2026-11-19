/* t_port_credential_pool_flow.c — harness for the agent/credential_pool.py FLOW port.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret","trace","state"[,"entries","seeder"]}}.
 * "trace" is the ordered seam-call sequence; "state" the mutated pool attributes.
 * Entry-returning flows project ret to {"id","last_status"} (the decision-relevant
 * identity + terminal state); the oracle projects the live PooledCredential identically.
 *
 * Scripted I/O lives in in.io (seam-name -> {"ret","state","raise"} or a queue LIST),
 * applied by the dispatcher exactly as the oracle's recording stubs are fed.
 */
#include "hermes_json.h"
#include "port_credential_pool_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
    json_t *io;
    json_t *cursors;
} script_ctx_t;

static json_t *script_next(script_ctx_t *sc, const char *name) {
    if (!sc || !sc->io) return NULL;
    json_t *entry = json_obj_get(sc->io, name);
    if (!entry) return NULL;
    if (entry->type == JSON_ARRAY) {
        double cur = json_get_num(sc->cursors, name, 0);
        size_t idx = (size_t)cur;
        if (idx >= json_len(entry)) return NULL;
        json_set(sc->cursors, name, json_number(cur + 1));
        return json_get(entry, idx);
    }
    return entry;
}

static void apply_state_effects(json_t *state, json_t *result) {
    if (!result || result->type != JSON_OBJECT) return;
    json_t *eff = json_obj_get(result, "state");
    if (!eff || eff->type != JSON_OBJECT) return;
    for (size_t i = 0; i < eff->c.count; i++)
        json_set(state, eff->c.keys[i], json_copy(eff->c.items[i]));
}

static json_t *dispatch(cp_flow_io_t *io, const char *name, json_t *args) {
    (void)args;
    script_ctx_t *sc = (script_ctx_t *)io->ctx;
    json_t *result = script_next(sc, name);
    apply_state_effects(io->state, result);
    if (result && result->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(result, "raise");
        if (raise && raise->type == JSON_OBJECT) {
            json_t *exc = json_object();
            json_set(exc, "__raise__", json_copy(json_obj_get(raise, "type")));
            json_t *msg = json_obj_get(raise, "message");
            json_set(exc, "message", msg ? json_copy(msg) : json_string(""));
            json_t *code = json_obj_get(raise, "code");
            if (code) json_set(exc, "code", json_copy(code));
            json_t *res = json_obj_get(raise, "result");
            if (res) json_set(exc, "result", json_copy(res));
            return exc;
        }
        json_t *ret = json_obj_get(result, "ret");
        return ret ? json_copy(ret) : json_null();
    }
    return json_null();
}

static void io_init(cp_flow_io_t *io, script_ctx_t *sc, json_t *in, json_t *state) {
    io->call = dispatch;
    io->trace = json_array();
    io->state = state;
    io->ctx = sc;
    sc->io = json_obj_get(in, "io");
    sc->cursors = json_object();
}

static json_t *state_from(json_t *in) {
    json_t *s = json_obj_get(in, "state");
    return (s && s->type == JSON_OBJECT) ? json_copy(s) : json_object();
}

/* project an entry to its decision-relevant identity + terminal state */
static json_t *entry_proj(const json_t *entry) {
    if (!entry || entry->type == JSON_NULL) return json_null();
    json_t *o = json_object();
    json_set(o, "id", json_string(cp_get_str(entry, "id", "")));
    json_t *st = cp_get(entry, "last_status");
    json_set(o, "last_status", st ? json_copy(st) : json_null());
    return o;
}

/* structural projection for entries-array comparisons (dedup/renumber/prune) */
static json_t *entry_struct(const json_t *entry) {
    json_t *o = json_object();
    json_set(o, "id", json_string(cp_get_str(entry, "id", "")));
    json_set(o, "source", json_string(cp_get_str(entry, "source", "")));
    json_set(o, "priority", json_copy(cp_get(entry, "priority")));
    json_set(o, "label", json_string(cp_get_str(entry, "label", "")));
    json_set(o, "access_token", json_string(cp_get_str(entry, "access_token", "")));
    json_t *st = cp_get(entry, "last_status");
    json_set(o, "last_status", st ? json_copy(st) : json_null());
    return o;
}

static json_t *entries_struct(const json_t *entries) {
    json_t *a = json_array();
    if (entries && entries->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(entries); i++)
            json_append(a, entry_struct(json_get(entries, i)));
    return a;
}

static json_t *make_out(cp_flow_io_t *io, json_t *ret, json_t *entries, json_t *seeder) {
    json_t *out = json_object();
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", io->trace ? json_copy(io->trace) : json_array());
    json_t *st = io->state ? json_copy(io->state) : json_object();
    json_obj_del(st, "_entries");  /* entry objects compared via projections, not raw state */
    json_set(out, "state", st);
    if (entries) json_set(out, "entries", entries_struct(entries));
    if (seeder) {
        json_t *s = json_copy(seeder);
        json_t *se = cp_get(s, "entries");
        if (se) json_set(s, "entries", entries_struct(se));
        json_set(out, "seeder", s);
    }
    return out;
}

static json_t *run_case(const char *func, json_t *in) {
    cp_flow_io_t io;
    script_ctx_t sc;
    json_t *state = state_from(in);
    io_init(&io, &sc, in, state);
    json_t *ret = NULL;
    json_t *entries_out = NULL;
    json_t *seeder_out = NULL;

    if (strcmp(func, "is_terminal_auth_failure") == 0) {
        ret = json_bool(cp_is_terminal_auth_failure((long)json_get_num(in, "status_code", 0),
            json_obj_get(in, "normalized_error")));
    } else if (strcmp(func, "is_sole_credential") == 0) {
        ret = json_bool(cp_is_sole_credential(json_obj_get(in, "entries")));
    } else if (strcmp(func, "next_priority") == 0) {
        ret = json_number((double)cp_next_priority(json_obj_get(in, "entries")));
    } else if (strcmp(func, "is_manual_source") == 0) {
        ret = json_bool(cp_is_manual_source(json_get_str(in, "source", "")));
    } else if (strcmp(func, "exhausted_ttl") == 0) {
        const char *fr = json_get_str(in, "failure_reason", NULL);
        ret = json_number((double)cp_exhausted_ttl((long)json_get_num(in, "error_code", 0),
            json_get_bool(in, "sole_credential", false), fr && *fr ? fr : NULL));
    } else if (strcmp(func, "exhausted_until") == 0) {
        ret = cp_exhausted_until(json_obj_get(in, "entry"), json_get_bool(in, "sole_credential", false));
    } else if (strcmp(func, "singleton_predates_entry") == 0) {
        ret = json_bool(cp_singleton_predates_entry(json_obj_get(in, "state"), json_obj_get(in, "entry")));
    } else if (strcmp(func, "codex_principal_identity") == 0) {
        ret = cp_codex_principal_identity(&io, json_obj_get(in, "access_token"));
    } else if (strcmp(func, "codex_entry_tracks_singleton") == 0) {
        ret = json_bool(cp_codex_entry_tracks_singleton(&io, json_obj_get(in, "entry"),
            json_obj_get(in, "singleton_tokens")));
    } else if (strcmp(func, "norm_url") == 0) {
        char *r = cp_norm_url(json_obj_get(in, "url"));
        ret = json_string(r ? r : ""); free(r);
    } else if (strcmp(func, "normalize_custom_pool_name") == 0) {
        char *r = cp_normalize_custom_pool_name(json_get_str(in, "name", ""));
        ret = json_string(r ? r : ""); free(r);
    } else if (strcmp(func, "store_owns_pool_provider") == 0) {
        ret = json_bool(cp_store_owns_pool_provider(json_obj_get(in, "auth_store"),
            json_get_str(in, "provider", "")));
    } else if (strcmp(func, "retain_sources_not_in") == 0) {
        json_t *entries = json_copy(json_obj_get(in, "entries"));
        bool r = cp_retain_sources_not_in(entries, json_obj_get(in, "drop"));
        ret = json_bool(r);
        entries_out = entries;
    } else if (strcmp(func, "normalize_pool_priorities") == 0) {
        json_t *entries = json_copy(json_obj_get(in, "entries"));
        bool r = cp_normalize_pool_priorities(json_get_str(in, "provider", ""), entries);
        ret = json_bool(r);
        entries_out = entries;
    } else if (strcmp(func, "prune_stale_seeded_entries") == 0) {
        json_t *entries = json_copy(json_obj_get(in, "entries"));
        bool r = cp_prune_stale_seeded_entries(&io, entries, json_obj_get(in, "active_sources"),
            json_get_bool(in, "prune_env_sources", true));
        ret = json_bool(r);
        entries_out = entries;
    } else if (strcmp(func, "get_env_prefer_dotenv") == 0) {
        char *r = cp_get_env_prefer_dotenv(&io, json_get_str(in, "key", ""));
        ret = json_string(r ? r : ""); free(r);
    } else if (strcmp(func, "env_payload") == 0) {
        ret = cp_env_payload(&io, json_get_str(in, "env_var", ""), json_get_str(in, "token", ""),
            json_get_str(in, "base_url", ""));
    } else if (strcmp(func, "env_key_var_candidates") == 0) {
        ret = cp_env_key_var_candidates(&io, json_obj_get(in, "env_vars"), json_obj_get(in, "entries"));
    } else if (strcmp(func, "find") == 0) {
        json_t *e = cp_find(&io, json_obj_get(in, "match"));
        ret = e ? json_string(cp_get_str(e, "id", "")) : json_null();
    } else if (strcmp(func, "replace_entry") == 0) {
        cp_replace_entry(&io, json_get_str(in, "old_id", ""), json_obj_get(in, "new_entry"));
        ret = json_null();
    } else if (strcmp(func, "adopt") == 0) {
        json_t *e = cp_adopt(&io, json_obj_get(in, "entry"), json_obj_get(in, "updates"),
            json_get_bool(in, "persist", true));
        ret = entry_proj(e);
        json_free(e);
    } else if (strcmp(func, "quarantine_sources") == 0) {
        cp_quarantine_sources(&io, json_obj_get(in, "entry"), json_obj_get(in, "sources"));
        ret = json_null();
    } else if (strcmp(func, "mark_exhausted") == 0) {
        const char *fr = json_get_str(in, "failure_reason", NULL);
        json_t *e = cp_mark_exhausted(&io, json_obj_get(in, "entry"),
            (long)json_get_num(in, "status_code", 0), json_obj_get(in, "error_context"),
            json_get_bool(in, "persist", true), fr && *fr ? fr : NULL);
        ret = entry_proj(e);
        json_free(e);
    } else if (strcmp(func, "available_entries") == 0) {
        const char *model = json_get_str(in, "model", NULL);
        ret = cp_available_entries(&io, json_get_bool(in, "clear_expired", false),
            json_get_bool(in, "refresh", false), model && *model ? model : NULL);
    } else if (strcmp(func, "lift_reopened_cooldowns") == 0) {
        const char *model = json_get_str(in, "model", NULL);
        ret = json_bool(cp_lift_reopened_cooldowns(&io, model && *model ? model : NULL));
    } else if (strcmp(func, "reclaim") == 0) {
        const char *model = json_get_str(in, "model", NULL);
        json_t *e = cp_reclaim(&io, json_get_str(in, "credential_id", ""), model && *model ? model : NULL);
        ret = e ? e : json_null();
    } else if (strcmp(func, "identify_failed_entry") == 0) {
        const char *cid = json_get_str(in, "credential_id", NULL);
        const char *hint = json_get_str(in, "api_key_hint", NULL);
        ret = cp_identify_failed_entry(&io, cid && *cid ? cid : NULL, hint && *hint ? hint : NULL);
    } else if (strcmp(func, "rotate_unmatched") == 0) {
        json_t *e = cp_rotate_unmatched(&io);
        ret = (e && e->type != JSON_NULL) ? json_string(cp_get_str(e, "id", "")) : json_null();
        json_free(e);
    } else if (strcmp(func, "entry_needs_refresh") == 0) {
        ret = json_bool(cp_entry_needs_refresh(&io, json_obj_get(in, "entry")));
    } else if (strcmp(func, "reset_cleared_after") == 0) {
        ret = cp_reset_cleared_after(&io, json_obj_get(in, "entry"));
    } else if (strcmp(func, "resync_stale_entry") == 0) {
        json_t *e = cp_resync_stale_entry(&io, json_obj_get(in, "entry"));
        ret = entry_proj(e);
        json_free(e);
    } else if (strcmp(func, "refresh_pending_entries") == 0) {
        cp_flow_refresh_pending_entries(&io, json_obj_get(in, "pending"));
        ret = json_null();
    } else if (strcmp(func, "try_refresh_current_unlocked") == 0) {
        const char *fk = json_get_str(in, "failed_key", NULL);
        json_t *e = cp_try_refresh_current_unlocked(&io, fk && *fk ? fk : NULL);
        ret = entry_proj(e);
        json_free(e);
    } else if (strcmp(func, "refresh_entry") == 0) {
        const char *fk = json_get_str(in, "failed_key", NULL);
        json_t *e = cp_refresh_entry(&io, json_obj_get(in, "entry"),
            json_get_bool(in, "force", false), fk && *fk ? fk : NULL);
        ret = entry_proj(e);
        json_free(e);
    } else if (strcmp(func, "mark_dead_refresh_grant") == 0) {
        cp_mark_dead_refresh_grant(&io, json_obj_get(in, "entry"), json_obj_get(in, "exc"));
        ret = json_null();
    } else if (strcmp(func, "fail_closed_unpersisted_rotation") == 0) {
        cp_fail_closed_unpersisted_rotation(&io, json_obj_get(in, "entry"),
            json_obj_get(in, "exc"), json_get_str(in, "store", ""));
        ret = json_null();
    } else if (strcmp(func, "recover_failed_refresh") == 0) {
        json_t *e = cp_recover_failed_refresh(&io, json_obj_get(in, "entry"), json_obj_get(in, "exc"));
        ret = entry_proj(e);
        json_free(e);
    } else if (strcmp(func, "upsert_entry") == 0) {
        json_t *entries = json_copy(json_obj_get(in, "entries"));
        json_t *payload = json_copy(json_obj_get(in, "payload"));
        bool r = cp_upsert_entry(&io, entries, json_get_str(in, "provider", ""),
            json_get_str(in, "source", ""), payload);
        ret = json_bool(r);
        entries_out = entries;
        json_free(payload);
    } else if (strcmp(func, "seeder_upsert") == 0) {
        json_t *seeder = json_copy(json_obj_get(in, "seeder"));
        json_t *payload = json_copy(json_obj_get(in, "payload"));
        bool r = cp_seeder_upsert(&io, seeder, json_get_str(in, "source", ""), payload);
        ret = json_bool(r);
        seeder_out = seeder;
        json_free(payload);
    } else {
        ret = json_object();
        json_set(ret, "error", json_string("unknown func"));
    }

    json_t *out = make_out(&io, ret, entries_out, seeder_out);
    if (entries_out) json_free(entries_out);
    if (seeder_out) json_free(seeder_out);
    json_free(sc.cursors);
    json_free(state);
    return out;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char line[1 << 20];
    while (fgets(line, sizeof line, stdin)) {
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *rec = json_parse(line, &err);
        if (!rec) { free(err); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in) in = json_object();
        json_t *out = run_case(func, in);
        json_t *line_out = json_object();
        json_set(line_out, "func", json_string(func));
        json_set(line_out, "in", json_copy(in));
        json_set(line_out, "out", out);
        char *ser = json_serialize(line_out);
        printf("%s\n", ser ? ser : "{}");
        free(ser);
        json_free(line_out);
        json_free(rec);
    }
    return 0;
}