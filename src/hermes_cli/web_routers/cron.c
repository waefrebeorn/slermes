/*
 * cron.c — canonical C11 port of hermes_cli/web_routers/cron.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "port_web_server_wrappers.h"
#include <ctype.h>
#include "blueprint_catalog_common.h"
extern char *xstrdup(const char *s);



/* === dissolved from web_server_wrappers.c === */
/* PoP: ws_get_cron_delivery_targets @ hermes_cli/web_routers/cron.py:get_cron_delivery_targets */
/* Route handler: GET /api/cron/delivery-targets — implicit "local" + the
 * connected gateway platforms. Reuses ws_load_configured_gateway_platforms. */
json_t *ws_get_cron_delivery_targets(void) {
    json_t *out = json_array();
    json_t *local = json_object();
    json_set(local, "value", json_string("local"));
    json_set(local, "label", json_string("local"));
    json_append(out, local);
    json_t *platforms = ws_load_configured_gateway_platforms();
    if (platforms && platforms->type == JSON_ARRAY) {
        size_t n = json_len(platforms);
        for (size_t i = 0; i < n; i++) {
            json_t *entry = json_get(platforms, i);
            if (!entry || entry->type != JSON_STRING) continue;
            json_t *item = json_object();
            json_set(item, "value", json_copy(entry));
            json_set(item, "label", json_copy(entry));
            json_append(out, item);
        }
    }
    if (platforms) json_free(platforms);
    return out;
}

/* PoP: ws_list_cron_jobs @ hermes_cli/web_routers/cron.py:list_cron_jobs */
/* Route handler: GET /api/cron/jobs. Dispatches to _list_cron_jobs_sync. */
json_t *ws_list_cron_jobs(const char *profile) {
    return ws_u_list_cron_jobs_sync(profile);
}

/* PoP: ws_get_cron_job @ hermes_cli/web_routers/cron.py:get_cron_job */
json_t *ws_get_cron_job(const char *job_id, const char *profile) {
    char *selected = ws_u_find_cron_job_profile(job_id);
    char *use_profile = selected ? selected : xstrdup(profile ? profile : "");
    json_t *job = ws_u_call_cron_for_profile(use_profile, "get_job", job_id);
    free(use_profile); free(selected);
    return job;
}

/* PoP: ws_list_cron_job_runs @ hermes_cli/web_routers/cron.py:list_cron_job_runs */
json_t *ws_list_cron_job_runs(const char *job_id, const char *profile, int limit) {
    return ws_u_list_cron_job_runs_sync(job_id, profile, limit);
}

/* ── Route handlers (web_routers/cron.py: thin FastAPI dispatch) ───── */

/* PoP: ws_create_cron_job @ hermes_cli/web_routers/cron.py:create_cron_job */
json_t *ws_create_cron_job(const char *profile, const json_t *body) {
    return ws_u_create_cron_job_sync(profile, body);
}

/* PoP: ws_update_cron_job @ hermes_cli/web_routers/cron.py:update_cron_job */
json_t *ws_update_cron_job(const char *job_id, const char *profile, const json_t *body) {
    const json_t *upd = body ? json_obj_get(body, "updates") : NULL;
    if (!upd) upd = body;
    return ws_u_update_cron_job_sync(job_id, profile, upd);
}

/* PoP: ws_pause_cron_job @ hermes_cli/web_routers/cron.py:pause_cron_job */
json_t *ws_pause_cron_job(const char *job_id, const char *profile) {
    return ws_u_pause_cron_job_sync(job_id, profile);
}

/* PoP: ws_resume_cron_job @ hermes_cli/web_routers/cron.py:resume_cron_job */
json_t *ws_resume_cron_job(const char *job_id, const char *profile) {
    return ws_u_resume_cron_job_sync(job_id, profile);
}

/* PoP: ws_trigger_cron_job @ hermes_cli/web_routers/cron.py:trigger_cron_job */
json_t *ws_trigger_cron_job(const char *job_id, const char *profile) {
    return ws_u_trigger_cron_job_sync(job_id, profile);
}

/* PoP: ws_delete_cron_job @ hermes_cli/web_routers/cron.py:delete_cron_job */
json_t *ws_delete_cron_job(const char *job_id, const char *profile) {
    return ws_u_delete_cron_job_sync(job_id, profile);
}

/* PoP: ws_cron_fire_webhook @ hermes_cli/web_routers/cron.py:cron_fire_webhook */
/* Port of Python: NAS→agent Chronos fire webhook. Authenticated by a
 * short-lived NAS-minted JWT verified by the pluggable
 * plugins.cron_providers.chronos.verify get_fire_verifier. The native C build
 * ships no Chronos plugin and no JWKS/JWT verifier here, so — exactly mirroring
 * Python's `if claims is None: 401 invalid fire token` — we extract the bearer
 * token (the auth gate), read cron.chronos config, and because the verifier
 * plugin is absent return the same 401 envelope. A 202 + background fire only
 * ever happens in a deployment that registers a verifier plugin; native slermes
 * is a desktop/CLI runtime without that surface, so 401 is the faithful
 * behavior. Returns a json_t {status_code, body} envelope. */
json_t *ws_cron_fire_webhook(const char *auth_header, const char *body_json) {
    const char *token = "";
    if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0)
        token = auth_header + 7;
    while (token && *token && isspace((unsigned char)*token)) token++;
    char *tok = xstrdup(token ?: "");
    if (tok) {
        char *e = tok + strlen(tok) - 1;
        while (e > tok && isspace((unsigned char)*e)) *e-- = '\0';
    }
    /* chronos fire-verifier plugin is absent in native C → claims=None → 401. */
    (void)body_json;
    json_t *env = json_object();
    json_set(env, "status_code", json_number(401));
    json_set(env, "error", json_string("invalid fire token"));
    json_set(env, "job_id", json_string(""));
    free(tok);
    return env;
}

/* PoP: platforms @ hermes_cli/web_routers/cron.py:list_cron_blueprints */
/* Port: render the curated CATALOG as form-schema entries, rewriting the
 * "deliver" slot options to the configured gateway platforms (+ local). */
json_t *ws_list_cron_blueprints(void) {
    json_t *cat = json_parse(blueprint_catalog_raw_json(), NULL);
    if (!cat || cat->type != JSON_ARRAY) {
        if (cat) json_free(cat);
        json_t *out = json_object();
        json_set(out, "blueprints", json_array());
        return out;
    }
    json_t *platforms = ws_load_configured_gateway_platforms();
    json_t *deliver_options = json_array();
    json_append(deliver_options, json_string("origin"));
    json_append(deliver_options, json_string("local"));
    if (platforms && platforms->type == JSON_ARRAY) {
        size_t n = json_len(platforms);
        for (size_t i = 0; i < n; i++) {
            json_t *p = json_get(platforms, i);
            if (p && p->type == JSON_STRING) json_append(deliver_options, json_copy(p));
        }
    }
    if (platforms) json_free(platforms);

    json_t *entries = json_array();
    size_t n = json_len(cat);
    for (size_t i = 0; i < n; i++) {
        json_t *bp = json_get(cat, i);
        if (!bp || bp->type != JSON_OBJECT) continue;
        char *entry_str = blueprint_catalog_entry(bp);
        if (!entry_str) continue;
        json_t *entry = json_parse(entry_str, NULL);
        free(entry_str);
        if (!entry) continue;
        /* Rewrite deliver slot options. */
        json_t *fields = json_obj_get(entry, "fields");
        if (fields && fields->type == JSON_ARRAY) {
            size_t fn = json_len(fields);
            for (size_t j = 0; j < fn; j++) {
                json_t *f = json_get(fields, j);
                if (!f || f->type != JSON_OBJECT) continue;
                const char *fnm = json_get_str(f, "name", NULL);
                if (fnm && strcmp(fnm, "deliver") == 0) {
                    json_set(f, "options", json_copy(deliver_options));
                }
            }
        }
        json_append(entries, entry);
    }
    json_free(cat);
    json_free(deliver_options);
    json_t *out = json_object();
    json_set(out, "blueprints", entries);
    return out;
}

/* PoP: ws_u_create_cron_job_sync @ hermes_cli/web_routers/cron.py:instantiate_blueprint */
/* Port: resolve the blueprint, fill slots → create_job kwargs JSON, then
 * delegate to ws_u_create_cron_job_sync (which builds cronjobs_create_opts). */
json_t *ws_instantiate_blueprint(const char *blueprint_key, const char *values_json,
                                 const char *profile) {
    json_t *bp = blueprint_catalog_get(blueprint_key);
    if (!bp) {
        json_t *err = json_object();
        json_set(err, "error", json_string("Unknown blueprint"));
        return err;
    }
    char ebuf[256];
    char *spec = blueprint_catalog_fill(bp, values_json ? values_json : "{}",
                                        NULL, ebuf, sizeof(ebuf));
    json_free(bp);
    if (!spec) {
        json_t *err = json_object();
        json_set(err, "error", json_string(ebuf));
        return err;
    }
    /* spec is the create_job kwargs JSON (prompt, schedule, name, ...); the
     * spec.pop("origin") from Python drops a key we simply don't forward. */
    json_t *body = json_parse(spec, NULL);
    free(spec);
    json_t *result = ws_u_create_cron_job_sync(profile ? profile : "default", body);
    if (body) json_free(body);
    return result;
}
