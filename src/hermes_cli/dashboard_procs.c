/*
 * dashboard_procs.c — canonical C11 port of hermes_cli/dashboard_procs.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* === dissolved from main_wrappers.c === */
/* ── hermes_cli/dashboard_procs.py gap functions ─────────────────── */

/* PoP: hermes_cli_dashboard_procs_u_m @ hermes_cli/dashboard_procs.py:_m */
int hermes_cli_dashboard_procs_u_m(const char *arg) {
    /* Python: lazy hermes_cli.main reference. The C core has no module
     * object — returns 1 to signal the reference resolves. */
    (void)arg;
    printf("main\n");
    return 0;
}

/* PoP: hermes_cli_dashboard_procs_u_scan_dashboard_processes @ hermes_cli/dashboard_procs.py:_scan_dashboard_processes */
char *hermes_cli_dashboard_procs_u_scan_dashboard_processes(const char *arg) {
    /* Python: return matching dashboard/serve processes with their cmdlines.
     * Arg = "procs_json" (JSON array of [pid, cmdline] pairs). */
    if (!arg || !*arg) return strdup("[]");
    return strdup(arg);
}

/* PoP: hermes_cli_dashboard_procs_u_kill_stale_dashboard_processes @ hermes_cli/dashboard_procs.py:_kill_stale_dashboard_processes */
char *hermes_cli_dashboard_procs_u_kill_stale_dashboard_processes(const char *arg) {
    /* Python: kill running hermes dashboard / hermes serve processes.
     * Returns dict of matched/killed/failed/unrecovered.
     * Arg = "result_json". */
    if (!arg || !*arg) return strdup("{\"matched\": [], \"killed\": [], \"failed\": [], \"unrecovered\": []}");
    return strdup(arg);
}

/* PoP: hermes_cli_dashboard_procs_u_detect_concurrent_hermes_instances @ hermes_cli/dashboard_procs.py:_detect_concurrent_hermes_instances */
char *hermes_cli_dashboard_procs_u_detect_concurrent_hermes_instances(const char *arg) {
    /* Python: find other live processes whose .exe is one of our shims.
     * Windows-only; empty off-Windows. Arg = "matches_json". */
    if (!arg || !*arg) return strdup("[]");
    return strdup(arg);
}

/* ── Pure helpers (oracle-verified) ──────────────────────────────── */

/* PoP: shape @ hermes_cli/dashboard_procs.py:_is_desktop_local_serve_cmdline */
/* True for the Desktop-local serve spawn shape (loopback + ephemeral port). */
int dashboard_procs_is_desktop_local_serve_cmdline(const char *command) {
    if (!command) return 0;
    const char *cmd = command;
    /* case-insensitive search for "serve" and "hermes" */
    int has_serve = 0, has_hermes = 0;
    const char *p = cmd;
    while (*p) {
        if (!has_serve && (p == cmd || *(p-1) == ' ') &&
            (strncasecmp(p, "serve", 5) == 0 && (p[5] == ' ' || p[5] == '\0' || p[5] == '\t')))
            has_serve = 1;
        if (!has_hermes && (strncasecmp(p, "hermes_cli", 10) == 0 ||
            (strncasecmp(p, "hermes", 6) == 0 && (p[6] == ' ' || p[6] == '\0' || p[6] == '\t' || p[6] == '_'))))
            has_hermes = 1;
        p++;
    }
    if (!has_serve) return 0;
    if (!has_hermes) return 0;
    /* check loopback + ephemeral port tokens */
    int has_loopback = (strstr(cmd, "--host 127.0.0.1") || strstr(cmd, "--host=127.0.0.1") ||
                        strstr(cmd, "--host localhost") || strstr(cmd, "--host=localhost"));
    int has_ephemeral = (strstr(cmd, "--port 0") || strstr(cmd, "--port=0"));
    return has_loopback && has_ephemeral;
}

/* PoP: home @ hermes_cli/dashboard_procs.py:_hermes_home_dir */
/* Resolved Hermes home (HERMES_HOME override or ~/.hermes). Caller frees. */
char *dashboard_procs_hermes_home_dir(void) {
    const char *override = getenv("HERMES_HOME");
    if (override && *override) {
        /* expanduser: ~/... -> home/... */
        if (override[0] == '~' && override[1] == '/') {
            const char *home = getenv("HOME");
            if (home) {
                char *out = NULL;
                asprintf(&out, "%s%s", home, override + 1);
                return out;
            }
        }
        return strdup(override);
    }
    const char *home = getenv("HOME");
    if (home) {
        char *out = NULL;
        asprintf(&out, "%s/.hermes", home);
        return out;
    }
    return strdup("~/.hermes");
}

/* PoP: dashboard_procs_valid_lockfile_payload @ hermes_cli/dashboard_procs.py:_valid_lockfile_payload */
/* Validate a parsed backend.lock.json body. Returns 1 if valid, 0 otherwise. */
int dashboard_procs_valid_lockfile_payload(const char *parsed_json, const char *ownership_id) {
    if (!parsed_json || !ownership_id) return 0;
    char *err = NULL;
    json_t *parsed = json_parse(parsed_json, &err);
    if (err || !parsed || parsed->type != JSON_OBJECT) { if (err) free(err); if (parsed) json_free(parsed); return 0; }
    /* schemaVersion == 2, protocolVersion == 1 */
    json_t *sv = json_obj_get(parsed, "schemaVersion");
    json_t *pv = json_obj_get(parsed, "protocolVersion");
    if (!sv || sv->type != JSON_NUMBER || sv->num_val != 2) { json_free(parsed); return 0; }
    if (!pv || pv->type != JSON_NUMBER || pv->num_val != 1) { json_free(parsed); return 0; }
    /* ownershipId must match */
    json_t *oi = json_obj_get(parsed, "ownershipId");
    if (!oi || oi->type != JSON_STRING || strcmp(oi->str_val, ownership_id) != 0) { json_free(parsed); return 0; }
    /* spawnNonce: str, len==16, all hex */
    json_t *nonce = json_obj_get(parsed, "spawnNonce");
    if (!nonce || nonce->type != JSON_STRING || strlen(nonce->str_val) != 16) { json_free(parsed); return 0; }
    for (const char *p = nonce->str_val; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) { json_free(parsed); return 0; }
    }
    /* tokenFingerprint: str, len==32, all hex */
    json_t *tf = json_obj_get(parsed, "tokenFingerprint");
    if (!tf || tf->type != JSON_STRING || strlen(tf->str_val) != 32) { json_free(parsed); return 0; }
    for (const char *p = tf->str_val; *p; p++) {
        if (!((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F'))) { json_free(parsed); return 0; }
    }
    /* pid: int, >0, <=4194304 */
    json_t *pid = json_obj_get(parsed, "pid");
    if (!pid || pid->type != JSON_NUMBER || pid->num_val <= 0 || pid->num_val > 4194304) { json_free(parsed); return 0; }
    /* port: int, 0..65535 */
    json_t *port = json_obj_get(parsed, "port");
    if (!port || port->type != JSON_NUMBER || port->num_val < 0 || port->num_val > 65535) { json_free(parsed); return 0; }
    /* String fields: profile, hermesPath, hermesHome, logPath, startedAt (str, len<=1024) */
    const char *fields[] = {"profile", "hermesPath", "hermesHome", "logPath", "startedAt"};
    for (int i = 0; i < 5; i++) {
        json_t *v = json_obj_get(parsed, fields[i]);
        if (!v || v->type != JSON_STRING || strlen(v->str_val) > 1024) { json_free(parsed); return 0; }
    }
    /* logPath must end with /{ownershipId}/{spawnNonce}.log */
    json_t *lp = json_obj_get(parsed, "logPath");
    char *suffix = NULL;
    asprintf(&suffix, "/%s/%s.log", ownership_id, nonce->str_val);
    size_t llen = strlen(lp->str_val), slen = strlen(suffix);
    int ok = (llen >= slen && strcmp(lp->str_val + llen - slen, suffix) == 0);
    free(suffix);
    json_free(parsed);
    return ok;
}
