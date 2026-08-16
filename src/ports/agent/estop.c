#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/estop.c — C11 port of agent/estop.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_estop.c ── */
/*
 * port_agent_estop.c — Faithful C11 port of agent/estop.py.
 *
 * Global emergency stop (ESTOP): a resumable pause for NEW work only.
 * ``hermes pause`` writes a sentinel JSON file at $HERMES_HOME/ESTOP;
 * ``hermes resume`` removes it. While engaged: cron skips due jobs, the
 * kanban dispatcher skips spawns, new gateway turns get a brief "paused"
 * reply instead of an agent run. In-flight work is NEVER killed.
 *
 * Contract mirrors the Python exactly:
 *   - single os.stat check so callers may run every tick;
 *   - fail SAFE on stat errors (engaged);
 *   - corrupt/empty sentinel body still counts as engaged (metadata not);
 *   - per-component "logged once per engagement" bookkeeping.
 *
 * HERMES_HOME resolution (C port convention): SLERMES_HOME → HERMES_HOME →
 * $HOME/.slermes. The Python get_hermes_home() adds a context-local override
 * + profile-warning layer that has no analogue in the stateless C binary; the
 * env-only path mirrors hermes_constants._hermes_home_from_env. The default
 * dir name differs (.slermes vs .hermes) only by the C port's naming choice.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include "json.h"
#include "agent_estop.h"

static const char *ESTOP_SENTINEL_NAME = "ESTOP";

/* --- _hermes_home: resolve the active home dir (env-only, like Python) --- */
static void pget_hermes_home(char *out, size_t cap)
{
    const char *home = getenv("SLERMES_HOME");
    if (!home || !*home) home = getenv("HERMES_HOME");
    if (home && *home) {
        snprintf(out, cap, "%s", home);
        return;
    }
    const char *uh = getenv("HOME");
    if (uh && *uh)
        snprintf(out, cap, "%s/.slermes", uh);
    else
        snprintf(out, cap, ".slermes");
}

/* PoP: estop_sentinel_path @ agent/estop.py:sentinel_path */
char *estop_sentinel_path(void)
{
    char home[4096];
    pget_hermes_home(home, sizeof(home));
    char *path = NULL;
    asprintf(&path, "%s/%s", home, ESTOP_SENTINEL_NAME);
    return path;
}

/* --- per-component log-once bookkeeping --------------------------------- */
#define ESTOP_LOGGED_MAX 16
static char g_logged[ESTOP_LOGGED_MAX][64];
static int  g_logged_count = 0;

static int pget_logged(const char *component)
{
    for (int i = 0; i < g_logged_count; i++)
        if (strcmp(g_logged[i], component) == 0) return i;
    return -1;
}

/* PoP: estop_hermes_home @ agent/estop.py:_hermes_home */
/* (resolution lives in pget_hermes_home above; this returns the resolved
 * path the same way _hermes_home does.) */
char *estop_hermes_home(void)
{
    char home[4096];
    pget_hermes_home(home, sizeof(home));
    return strdup(home);
}

/* PoP: estop_is_engaged @ agent/estop.py:is_engaged */
/* Fail-SAFE: stat failure → engaged (pause must hold when FS misbehaves). */
int estop_is_engaged(void)
{
    char *path = estop_sentinel_path();
    struct stat st;
    int rc = stat(path, &st);
    free(path);
    if (rc != 0) {
        /* ENOENT → not engaged. Other stat errors → fail SAFE (engaged),
         * since Path.exists() swallows ENOENT but surfaces OSError only for
         * genuine access failures; those mean the pause must hold. */
        return (errno == ENOENT) ? 0 : 1;
    }
    return 1;
}

/* --- UTC ISO-8601 timestamp (mirrors datetime.now(timezone.utc).isoformat) */
static void pnow_iso(char *out, size_t cap)
{
    time_t now = time(NULL);
    struct tm tm_info;
    gmtime_r(&now, &tm_info);
    /* strftime %Y-%m-%dT%H:%M:%S+00:00 style */
    strftime(out, cap, "%Y-%m-%dT%H:%M:%S+00:00", &tm_info);
}

/* PoP: pauses @ agent/estop.py:engage */
/* Create the sentinel JSON body {"engaged_at":..., "reason":...}. Best effort:
 * an empty/partial sentinel still pauses (fail safe). */
char *estop_engage(const char *reason)
{
    char *path = estop_sentinel_path();
    char ts[64];
    pnow_iso(ts, sizeof(ts));
    char *body = NULL;
    asprintf(&body, "{\n  \"engaged_at\": \"%s\",\n  \"reason\": %s\n}\n",
             ts, (reason && *reason) ? "\"" : "null");
    /* rewrite body with escaped reason if present */
    if (reason && *reason) {
        free(body);
        /* escape double quotes in reason */
        char *esc = NULL;
        size_t elen = 0;
        FILE *e = open_memstream(&esc, &elen);
        if (e) {
            for (const char *p = reason; *p; p++) {
                if (*p == '"' || *p == '\\') fputc('\\', e);
                fputc(*p, e);
            }
            fclose(e);
        }
        asprintf(&body, "{\n  \"engaged_at\": \"%s\",\n  \"reason\": \"%s\"\n}\n",
                 ts, esc ? esc : reason);
        free(esc);
    }
    /* ensure parent dir */
    char *slash = strrchr(path, '/');
    if (slash) {
        *slash = '\0';
        mkdir(path, 0700);
        *slash = '/';
    }
    FILE *fp = fopen(path, "w");
    if (fp) { fputs(body, fp); fclose(fp); }
    else {
        /* best effort: empty/partial sentinel still pauses (fail safe) */
        fclose(fopen(path, "a") ? fopen(path, "a") : NULL);
    }
    free(body);
    return path;
}

/* PoP: estop_disengage @ agent/estop.py:disengage */
int estop_disengage(void)
{
    char *path = estop_sentinel_path();
    int rc = unlink(path);
    free(path);
    return (rc == 0) ? 1 : 0; /* FileNotFoundError & OSError → False */
}

/* PoP: estop_get_state @ agent/estop.py:get_state */
/* Returns {"reason":..., "engaged_at":...} as JSON or NULL when not engaged.
 * Corrupt body → engaged with both fields null. Caller frees. */
char *estop_get_state(void)
{
    char *path = estop_sentinel_path();
    struct stat st;
    if (stat(path, &st) != 0) { free(path); return NULL; } /* not engaged */
    char *raw = NULL;
    if (st.st_size > 0) {
        FILE *fp = fopen(path, "r");
        if (fp) {
            raw = malloc(st.st_size + 1);
            size_t rd = fread(raw, 1, st.st_size, fp);
            raw[rd] = '\0';
            fclose(fp);
        }
    }
    free(path);
    /* build {"reason":..., "engaged_at":...} — defaults to null */
    char *reason = NULL;
    char *engaged_at = NULL;
    if (raw) {
        char *err = NULL;
        json_t *j = json_parse(raw, &err);
        free(err);
        if (j && j->type == JSON_OBJECT) {
            json_t *r = json_obj_get(j, "reason");
            if (r) {
                if (r->type == JSON_STRING) reason = strdup(r->str_val ? r->str_val : "");
                /* else leave null */
            }
            json_t *e = json_obj_get(j, "engaged_at");
            if (e && e->type == JSON_STRING) engaged_at = strdup(e->str_val ? e->str_val : "");
            json_free(j);
        }
        free(raw);
    }
    json_t *out = json_object();
    if (reason) json_set(out, "reason", json_string(reason));
    else json_set(out, "reason", json_null());
    if (engaged_at) json_set(out, "engaged_at", json_string(engaged_at));
    else json_set(out, "engaged_at", json_null());
    char *s = json_serialize(out);
    json_free(out);
    free(reason); free(engaged_at);
    return s;
}

/* PoP: estop_paused_reply @ agent/estop.py:paused_reply */
char *estop_paused_reply(void)
{
    char *state_json = estop_get_state();
    if (!state_json) return NULL;
    char *reason = NULL;
    json_t *j = json_parse(state_json, NULL);
    free(state_json);
    if (j && j->type == JSON_OBJECT) {
        json_t *r = json_obj_get(j, "reason");
        if (r && r->type == JSON_STRING && r->str_val && *r->str_val)
            reason = strdup(r->str_val);
        json_free(j);
    }
    char *reply = NULL;
    if (reason && *reason)
        asprintf(&reply, "⏸️ Hermes is paused (%s). New work is on hold; run `hermes resume` to pick things back up.", reason);
    else
        asprintf(&reply, "⏸️ Hermes is paused. New work is on hold; run `hermes resume` to pick things back up.");
    free(reason);
    return reply;
}

/* PoP: estop_check_paused @ agent/estop.py:check_paused */
int estop_check_paused(const char *component)
{
    if (!estop_is_engaged()) {
        /* disengaged → re-arm this component */
        int idx = pget_logged(component);
        if (idx >= 0) {
            g_logged[idx][0] = '\0';
            /* shift down to fill the gap */
            for (int i = idx; i < g_logged_count - 1; i++)
                memcpy(g_logged[i], g_logged[i+1], 64);
            g_logged_count--;
        }
        return 0;
    }
    int first = (pget_logged(component) < 0);
    if (first && g_logged_count < ESTOP_LOGGED_MAX) {
        strncpy(g_logged[g_logged_count], component ? component : "", 63);
        g_logged[g_logged_count][63] = '\0';
        g_logged_count++;
    }
    if (first) {
        char *state_json = estop_get_state();
        char *reason = NULL;
        if (state_json) {
            json_t *j = json_parse(state_json, NULL);
            if (j && j->type == JSON_OBJECT) {
                json_t *r = json_obj_get(j, "reason");
                if (r && r->type == JSON_STRING && r->str_val && *r->str_val)
                    reason = strdup(r->str_val);
                json_free(j);
            }
            free(state_json);
        }
        char *path = estop_sentinel_path();
        char *suffix = reason ? reason : NULL;
        if (suffix)
            fprintf(stderr, "%s dispatch paused by global emergency stop (reason: %s) — remove with `hermes resume` (%s)\n",
                    component, suffix, path);
        else
            fprintf(stderr, "%s dispatch paused by global emergency stop — remove with `hermes resume` (%s)\n",
                    component, path);
        free(reason);
        free(path);
    }
    return 1;
}

/* PoP: estop_reset_log_state_for_tests @ agent/estop.py:_reset_log_state_for_tests */
void estop_reset_log_state_for_tests(void)
{
    g_logged_count = 0;
}

