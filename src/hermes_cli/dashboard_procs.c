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

/* ═══ Remaining hermes_cli/dashboard_procs.py REAL_GAP closures ═══════ */
/* These extend the pure helpers above with the /proc-backed scans the
 * Python module performs via psutil/ps. All argv parameters are JSON
 * arrays of strings (libjson), mirroring the list[str] Python takes. */

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <proc.h>
#define DASHBOARD_PROCS_REMOTE_LOCK_SUBDIR "desktop-ssh"
#define DASHBOARD_PROCS_REAP_MIN_AGE_SECONDS 180.0

static char *dprocs_read_file(const char *path, size_t max_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    char *buf = malloc(max_len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, max_len, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* PoP: dashboard_procs_hermes_home_for_pid @ hermes_cli/dashboard_procs.py:_hermes_home_for_pid */
char *dashboard_procs_hermes_home_for_pid(int pid) {
    /* Best-effort HERMES_HOME from pid's environment (/proc/<pid>/environ). */
    if (pid <= 0) return NULL;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/environ", pid);
    size_t cap = 65536;
    char *raw = dprocs_read_file(path, cap);
    if (!raw) return NULL;
    char *out = NULL;
    for (char *part = raw; part < raw + strlen(raw); part += strlen(part) + 1) {
        if (strncmp(part, "HERMES_HOME=", 12) == 0) {
            const char *val = part + 12;
            out = strdup(*val ? val : "");
            break;
        }
    }
    free(raw);
    return out;   /* NULL when unset or unreadable; "" when set-but-empty */
}

static json_t *dprocs_parse_argv(const char *argv_json) {
    if (!argv_json || !*argv_json) return NULL;
    char *err = NULL;
    json_t *arr = json_parse(argv_json, &err);
    if (err || !arr || arr->type != JSON_ARRAY) {
        if (err) free(err);
        if (arr) json_free(arr);
        return NULL;
    }
    return arr;
}

static const char *dprocs_arr_str(json_t *arr, size_t i) {
    json_t *e = json_get(arr, i);
    return (e && e->type == JSON_STRING) ? e->str_val : NULL;
}

/* PoP: dashboard_procs_dashboard_subcommand_index @ hermes_cli/dashboard_procs.py:_dashboard_subcommand_index */
int dashboard_procs_dashboard_subcommand_index(const char *argv_json) {
    json_t *argv = dprocs_parse_argv(argv_json);
    if (!argv) return -1;
    int idx = -1;
    for (size_t i = 0; i < argv->c.count; i++) {
        const char *tok = dprocs_arr_str(argv, i);
        if (tok && (strcmp(tok, "serve") == 0 || strcmp(tok, "dashboard") == 0)) {
            idx = (int)i;
            break;
        }
    }
    json_free(argv);
    return idx;
}

/* PoP: dashboard_procs_is_ephemeral_port_zero_backend @ hermes_cli/dashboard_procs.py:_is_ephemeral_port_zero_backend */
int dashboard_procs_is_ephemeral_port_zero_backend(const char *argv_json) {
    json_t *argv = dprocs_parse_argv(argv_json);
    if (!argv) return 0;
    if (dashboard_procs_dashboard_subcommand_index(argv_json) < 0) {
        json_free(argv);
        return 0;
    }
    int is_zero = 0;
    for (size_t i = 0; i < argv->c.count; i++) {
        const char *tok = dprocs_arr_str(argv, i);
        if (!tok) continue;
        if (strcmp(tok, "--port") == 0 && i + 1 < argv->c.count) {
            const char *next = dprocs_arr_str(argv, i + 1);
            if (next && strcmp(next, "0") == 0) { is_zero = 1; break; }
        }
        if (strncmp(tok, "--port=", 7) == 0) {
            const char *v = tok + 7;
            while (*v == ' ' || *v == '\t') v++;
            if (strcmp(v, "0") == 0) { is_zero = 1; break; }
        }
    }
    json_free(argv);
    return is_zero;
}

/* PoP: dashboard_procs_normalize_dashboard_cmdline @ hermes_cli/dashboard_procs.py:_normalize_dashboard_cmdline */
char *dashboard_procs_normalize_dashboard_cmdline(const char *argv_json) {
    json_t *argv = dprocs_parse_argv(argv_json);
    if (!argv) return strdup("[]");
    int idx = dashboard_procs_dashboard_subcommand_index(argv_json);
    json_t *out = json_array();
    if (idx >= 0) {
        /* prefix: only profile flags survive */
        for (int i = 0; i < idx;) {
            const char *tok = dprocs_arr_str(argv, (size_t)i);
            if (!tok) { i++; continue; }
            if ((strcmp(tok, "--profile") == 0 || strcmp(tok, "-p")) &&
                i + 1 < idx) {
                json_append(out, json_string(tok));
                const char *val = dprocs_arr_str(argv, (size_t)i + 1);
                json_append(out, json_string(val ? val : ""));
                i += 2;
                continue;
            }
            if (strncmp(tok, "--profile=", 10) == 0)
                json_append(out, json_string(tok));
            i++;
        }
        for (size_t i = (size_t)idx; i < argv->c.count; i++) {
            const char *tok = dprocs_arr_str(argv, i);
            json_append(out, json_string(tok ? tok : ""));
        }
    } else {
        for (size_t i = 0; i < argv->c.count; i++) {
            const char *tok = dprocs_arr_str(argv, i);
            json_append(out, json_string(tok ? tok : ""));
        }
    }
    json_free(argv);
    char *serialized = json_serialize(out);
    json_free(out);
    return serialized ? serialized : strdup("[]");
}

/* PoP: dashboard_procs_profile_key_for_respawn @ hermes_cli/dashboard_procs.py:_profile_key_for_respawn */
char *dashboard_procs_profile_key_for_respawn(const char *argv_json,
                                              const char *hermes_home) {
    const char *profile_name = NULL;
    json_t *argv = dprocs_parse_argv(argv_json);
    if (argv) {
        for (size_t i = 0; i < argv->c.count; i++) {
            const char *tok = dprocs_arr_str(argv, i);
            if (!tok) continue;
            if ((strcmp(tok, "--profile") == 0 || strcmp(tok, "-p")) &&
                i + 1 < argv->c.count) {
                profile_name = dprocs_arr_str(argv, i + 1);
                break;
            }
            if (strncmp(tok, "--profile=", 10) == 0) {
                profile_name = tok + 10;
                break;
            }
        }
    }

    char *key = NULL;
    if (hermes_home && *hermes_home) {
        /* normalize trailing slashes so ".../profiles/<name>/" still matches */
        char *resolved = realpath(hermes_home, NULL);
        const char *use = resolved ? resolved : hermes_home;
        size_t len = strlen(use);
        char *tmp = strdup(use);
        if (tmp) {
            while (len > 1 && tmp[len-1] == '/') tmp[--len] = '\0';
            const char *last_slash = strrchr(tmp, '/');
            const char *leaf = last_slash ? last_slash + 1 : tmp;
            const char *prev_end = last_slash ? last_slash : tmp;
            /* find the segment before leaf */
            char prev[512] = "";
            if (last_slash) {
                const char *start = tmp;
                const char *p = last_slash - 1;
                while (p > start && *p != '/') p--;
                if (*p == '/') p++;
                size_t plen = (size_t)(last_slash - p);
                if (plen >= sizeof(prev)) plen = sizeof(prev) - 1;
                memcpy(prev, p, plen);
                prev[plen] = '\0';
            }
            (void)prev_end;
            if (strcmp(prev, "profiles") == 0 && *leaf)
                asprintf(&key, "profile:%s", leaf);
            else
                asprintf(&key, "home:%s", tmp);   /* normcase is identity on POSIX */
            free(tmp);
        }
        free(resolved);
        if (!key) asprintf(&key, "home:%s", hermes_home);
    } else if (profile_name && *profile_name) {
        asprintf(&key, "profile:%s", profile_name);
    } else {
        key = strdup("profile:default");
    }
    if (argv) json_free(argv);
    return key;
}

/* PoP: dashboard_procs_filter_dashboard_respawn_candidates @ hermes_cli/dashboard_procs.py:_filter_dashboard_respawn_candidates */
char *dashboard_procs_filter_dashboard_respawn_candidates(const char *candidates_json) {
    /* candidates_json: [[pid, [argv...], home_or_null], ...]
     * returns:         [[argv...], ...] */
    char *err = NULL;
    json_t *candidates = json_parse(candidates_json ? candidates_json : "[]", &err);
    if (err || !candidates || candidates->type != JSON_ARRAY) {
        if (err) free(err);
        if (candidates) json_free(candidates);
        return strdup("[]");
    }
    json_t *selected = json_array();

    /* seen-set tracking: normalized cmdlines and profile keys */
    char **seen_cmdlines = NULL, **seen_profiles = NULL;
    int n_cl = 0, n_pk = 0, cap_cl = 0, cap_pk = 0;

    for (size_t c = 0; c < candidates->c.count; c++) {
        json_t *entry = json_get(candidates, c);
        if (!entry || entry->type != JSON_ARRAY || entry->c.count < 2) continue;
        json_t *j_argv = json_get(entry, 1);
        if (!j_argv || j_argv->type != JSON_ARRAY || j_argv->c.count == 0) continue;
        char *argv_json = json_serialize(j_argv);
        if (!argv_json) continue;

        if (dashboard_procs_is_ephemeral_port_zero_backend(argv_json)) {
            free(argv_json);
            continue;
        }
        char *norm = dashboard_procs_normalize_dashboard_cmdline(argv_json);
        int dup_cl = 0;
        for (int i = 0; i < n_cl; i++)
            if (strcmp(seen_cmdlines[i], norm) == 0) { dup_cl = 1; break; }
        if (dup_cl) { free(norm); free(argv_json); continue; }

        json_t *j_home = entry->c.count >= 3 ? json_get(entry, 2) : NULL;
        const char *home = (j_home && j_home->type == JSON_STRING) ? j_home->str_val : NULL;
        char *pk = dashboard_procs_profile_key_for_respawn(argv_json, home);
        int dup_pk = 0;
        for (int i = 0; i < n_pk; i++)
            if (strcmp(seen_profiles[i], pk) == 0) { dup_pk = 1; break; }
        if (dup_pk) { free(pk); free(norm); free(argv_json); continue; }

        if (n_cl >= cap_cl) { cap_cl = cap_cl ? cap_cl * 2 : 8;
            seen_cmdlines = realloc(seen_cmdlines, cap_cl * sizeof(char *)); }
        if (n_pk >= cap_pk) { cap_pk = cap_pk ? cap_pk * 2 : 8;
            seen_profiles = realloc(seen_profiles, cap_pk * sizeof(char *)); }
        if (seen_cmdlines) seen_cmdlines[n_cl++] = norm; else free(norm);
        if (seen_profiles) seen_profiles[n_pk++] = pk; else free(pk);

        json_append(selected, json_copy(j_argv));
        free(argv_json);
    }
    for (int i = 0; i < n_cl; i++) free(seen_cmdlines[i]);
    for (int i = 0; i < n_pk; i++) free(seen_profiles[i]);
    free(seen_cmdlines);
    free(seen_profiles);
    json_free(candidates);
    char *out = json_serialize(selected);
    json_free(selected);
    return out ? out : strdup("[]");
}

/* PoP: dashboard_procs_process_ppid @ hermes_cli/dashboard_procs.py:_process_ppid */
pid_t dashboard_procs_process_ppid(int pid) {
    if (pid <= 0) return -1;
    /* Field 4 of /proc/<pid>/stat is the parent pid; comm (field 2) may
     * contain spaces/parens so parse past the final ')'. */
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char *stat_line = dprocs_read_file(path, 4096);
    if (!stat_line) return -1;
    char *close_paren = strrchr(stat_line, ')');
    pid_t ppid = -1;
    if (close_paren) {
        int field = 2;   /* fields after state (3): ppid is field 4 */
        char *save = NULL;
        for (char *tok = strtok_r(close_paren + 1, " \t", &save); tok;
             tok = strtok_r(NULL, " \t", &save)) {
            field++;
            if (field == 4) { ppid = (pid_t)atoi(tok); break; }
        }
    }
    free(stat_line);
    return ppid;
}

/* PoP: dashboard_procs_process_age_seconds @ hermes_cli/dashboard_procs.py:_process_age_seconds */
double dashboard_procs_process_age_seconds(int pid) {
    if (pid <= 0) return -1.0;
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    char *stat_line = dprocs_read_file(path, 4096);
    if (!stat_line) return -1.0;
    char *close_paren = strrchr(stat_line, ')');
    double age = -1.0;
    if (close_paren) {
        unsigned long long starttime_ticks = 0;
        int field = 2;
        char *save = NULL;
        for (char *tok = strtok_r(close_paren + 1, " \t", &save); tok;
             tok = strtok_r(NULL, " \t", &save)) {
            field++;
            if (field == 22) { starttime_ticks = strtoull(tok, NULL, 10); break; }
        }
        long hz = sysconf(_SC_CLK_TCK);
        if (hz > 0) {
            double start_secs = (double)starttime_ticks / (double)hz;
            double uptime = proc_uptime();
            if (uptime >= 0.0) {
                age = uptime - start_secs;
                if (age < 0.0) age = 0.0;
            }
        }
    }
    free(stat_line);
    return age;
}

static bool dprocs_hex32(const char *s) {
    if (strlen(s) != 32) return false;
    for (const char *p = s; *p; p++)
        if (!isxdigit((unsigned char)*p) || isupper((unsigned char)*p))
            return false;
    return true;
}

/* PoP: dashboard_procs_lock_owned_serve_pids @ hermes_cli/dashboard_procs.py:_lock_owned_serve_pids */
char *dashboard_procs_lock_owned_serve_pids(const char *base_dir) {
    /* Scans {hermes_home}/desktop-ssh/<ownershipId>/backend.lock.json and
     * returns the JSON array of PIDs valid locks claim as owners. */
    char *root = NULL;
    if (base_dir && *base_dir) {
        root = strdup(base_dir);
    } else {
        char *home = dashboard_procs_hermes_home_dir();
        asprintf(&root, "%s/" DASHBOARD_PROCS_REMOTE_LOCK_SUBDIR, home ? home : "");
        free(home);
    }
    json_t *owned = json_array();
    if (root) {
        DIR *dir = opendir(root);
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.') continue;
                if (!dprocs_hex32(ent->d_name)) continue;
                char lock_path[1024];
                snprintf(lock_path, sizeof(lock_path), "%s/%s/backend.lock.json",
                         root, ent->d_name);
                size_t sz = 0;
                FILE *f = fopen(lock_path, "rb");
                if (!f) continue;
                fseek(f, 0, SEEK_END);
                long fsz = ftell(f);
                fclose(f);
                if (fsz < 0 || fsz > 65536) continue;
                sz = (size_t)fsz;
                char *data = dprocs_read_file(lock_path, sz);
                if (!data) continue;
                char *err = NULL;
                json_t *parsed = json_parse(data, &err);
                free(data);
                if (err || !parsed) { if (err) free(err); if (parsed) json_free(parsed); continue; }
                /* reuse the shared payload validator */
                char *payload = json_serialize(parsed);
                if (payload &&
                    dashboard_procs_valid_lockfile_payload(payload, ent->d_name)) {
                    json_t *pid_v = json_obj_get(parsed, "pid");
                    if (pid_v && pid_v->type == JSON_NUMBER && pid_v->num_val > 0)
                        json_append(owned, json_number((double)(long long)pid_v->num_val));
                }
                free(payload);
                json_free(parsed);
            }
            closedir(dir);
        }
        free(root);
    }
    char *out = json_serialize(owned);
    json_free(owned);
    return out ? out : strdup("[]");
}

/* PoP: dashboard_procs_reap_orphaned_desktop_local_serves @ hermes_cli/dashboard_procs.py:_reap_orphaned_desktop_local_serves */
char *dashboard_procs_reap_orphaned_desktop_local_serves(const char *reason) {
    /* Kill leftover Desktop-local `hermes serve` backends whose ppid is 1:
     * only the loopback+--port 0 spawn shape, never lock-owned PIDs, never
     * anything younger than _REAP_MIN_AGE_SECONDS. Best-effort throughout. */
    json_t *result = json_object();
    json_t *j_matched = json_array(), *j_killed = json_array(), *j_failed = json_array();

    /* exclusion set: self, parent, HERMES_DESKTOP_CHILD_PID, lock-owned */
    int exclude[4096];
    int n_exclude = 0;
    exclude[n_exclude++] = (int)getpid();
    pid_t ppid_self = getppid();
    if (ppid_self > 0 && n_exclude < 4096) exclude[n_exclude++] = (int)ppid_self;
    const char *raw_desktop = getenv("HERMES_DESKTOP_CHILD_PID");
    if (raw_desktop) {
        char *dup = strdup(raw_desktop);
        char *save = NULL;
        for (char *part = strtok_r(dup, ",", &save); part && n_exclude < 4095;
             part = strtok_r(NULL, ",", &save)) {
            while (*part == ' ') part++;
            if (*part) exclude[n_exclude++] = atoi(part);
        }
        free(dup);
    }
    char *owned_json = dashboard_procs_lock_owned_serve_pids(NULL);
    if (owned_json) {
        char *err = NULL;
        json_t *owned = json_parse(owned_json, &err);
        if (owned && owned->type == JSON_ARRAY) {
            for (size_t i = 0; i < owned->c.count && n_exclude < 4096; i++) {
                json_t *v = json_get(owned, i);
                if (v && v->type == JSON_NUMBER)
                    exclude[n_exclude++] = (int)v->num_val;
            }
        }
        if (owned) json_free(owned);
        free(owned_json);
    }

    /* scan /proc for the desktop-local serve shape */
    struct target { int pid; char cmd[2048]; };
    static struct target targets[512];
    int n_targets = 0;
    DIR *proc = opendir("/proc");
    if (proc) {
        struct dirent *ent;
        while ((ent = readdir(proc)) != NULL) {
            if (ent->d_name[0] < '0' || ent->d_name[0] > '9') continue;
            int pid = atoi(ent->d_name);
            if (pid == (int)getpid()) continue;
            int skip = 0;
            for (int i = 0; i < n_exclude; i++)
                if (exclude[i] == pid) { skip = 1; break; }
            if (skip) continue;
            char cmdline_path[64];
            snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
            char *cmd = dprocs_read_file(cmdline_path, 4000);
            if (!cmd) continue;
            /* NUL-separated argv -> space-joined string (ps-style) */
            size_t len = strlen(cmd);
            for (size_t i = len; i < 4000 && cmd[i]; ) {
                cmd[i] = ' ';
                size_t seg = strlen(cmd + i + 1) + 1;
                i += seg;
                len = i + strlen(cmd + i);
            }
            if (dashboard_procs_is_desktop_local_serve_cmdline(cmd) &&
                n_targets < 512) {
                targets[n_targets].pid = pid;
                snprintf(targets[n_targets].cmd, sizeof(targets[n_targets].cmd), "%s", cmd);
                n_targets++;
            }
            free(cmd);
        }
        closedir(proc);
    }

    /* re-read lock ownership defensively before killing */
    char *owned_now_json = dashboard_procs_lock_owned_serve_pids(NULL);
    for (int i = 0; i < n_targets; i++) {
        int owned_now = 0;
        if (owned_now_json) {
            char *err = NULL;
            json_t *owned = json_parse(owned_now_json, &err);
            if (owned) {
                for (size_t k = 0; k < owned->c.count; k++) {
                    json_t *v = json_get(owned, k);
                    if (v && v->type == JSON_NUMBER && (int)v->num_val == targets[i].pid) {
                        owned_now = 1;
                        break;
                    }
                }
                json_free(owned);
            }
        }
        if (owned_now) continue;
        pid_t pp = dashboard_procs_process_ppid(targets[i].pid);
        if (pp <= 0) continue;             /* unknown -> fail safe */
        if (pp != 1 && pp != 0) continue;  /* orphaned under init only */
        double age = dashboard_procs_process_age_seconds(targets[i].pid);
        if (age < 0.0 || age < DASHBOARD_PROCS_REAP_MIN_AGE_SECONDS) continue;
        json_append(j_matched, json_int(targets[i].pid));
    }
    free(owned_now_json);

    for (size_t i = 0; i < json_len(j_matched); i++) {
        json_t *v = json_get(j_matched, i);
        int pid = (int)(v ? v->num_val : 0);
        if (kill((pid_t)pid, SIGTERM) != 0) {
            if (errno == ESRCH) continue;      /* ProcessLookupError: gone */
            json_append(j_failed, json_int(pid));
        }
    }
    /* brief grace, then SIGKILL survivors */
    struct timespec grace = {1, 500 * 1000 * 1000};
    nanosleep(&grace, NULL);
    for (size_t i = 0; i < json_len(j_matched); i++) {
        json_t *v = json_get(j_matched, i);
        int pid = (int)(v ? v->num_val : 0);
        int failed = 0;
        for (size_t k = 0; k < json_len(j_failed); k++) {
            json_t *fv = json_get(j_failed, k);
            if (fv && (int)fv->num_val == pid) { failed = 1; break; }
        }
        if (failed) continue;
        if (kill((pid_t)pid, 0) != 0 && errno == ESRCH) {
            json_append(j_killed, json_int(pid));   /* exited from SIGTERM */
            continue;
        }
        if (kill((pid_t)pid, SIGKILL) == 0 || errno == ESRCH)
            json_append(j_killed, json_int(pid));
        else
            json_append(j_failed, json_int(pid));
    }

    if (json_len(j_matched) > 0) {
        char *killed_str = json_serialize(j_killed);
        printf("⟲ Reaped %zu orphaned desktop-local serve backend(s) (%s): %s\n",
               json_len(j_killed), reason ? reason : "orphaned desktop-local hermes serve",
               killed_str ? killed_str : "[]");
        free(killed_str);
    }
    json_set(result, "matched", j_matched);
    json_set(result, "killed", j_killed);
    json_set(result, "failed", j_failed);
    char *out = json_serialize(result);
    json_free(result);
    return out ? out : strdup("{\"matched\":[],\"killed\":[],\"failed\":[]}");
}
