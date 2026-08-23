/*
 * update_receipt.c — C11 port of hermes_cli/update_receipt.py
 *
 * Phase 1 of the fleet-update reliability plan (#91277): the updater must
 * PROVE its outcome instead of assuming it. Machine-readable receipts under
 * <HERMES_HOME>/logs/update_receipts/ plus the post-update fleet version
 * matrix comparing every live gateway's stamped code_sha against HEAD.
 */
#define _POSIX_C_SOURCE 200809L

#include "update_receipt.h"
#include "control_socket.h"
#include "profile_store.h"
#include "gateway_status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define UR_RECEIPT_DIR_NAME "update_receipts"
#define UR_RECEIPT_KEEP 20

static update_receipt_t *ur_current = NULL;

struct update_receipt {
    json_t *data;
};

static char *ur_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

/* PoP: _utc_now_iso @hermes_cli/update_receipt.py:_utc_now_iso */
static char *ur_utc_now_iso(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    struct tm tm;
    gmtime_r(&ts.tv_sec, &tm);
    int ms = (int)(ts.tv_nsec / 1000000);
    char *out = malloc(48);
    if (!out) return NULL;
    snprintf(out, 48, "%04d-%02d-%02dT%02d:%02d:%02d.%03d+00:00",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
             tm.tm_min, tm.tm_sec, ms);
    return out;
}

/* Code identity from the current checkout — the C tree stamps it into
 * gateway_state.json; read it live via git when available (best-effort). */
static void ur_add_code_identity(json_t *obj)
{
    if (!obj || obj->type != JSON_OBJECT) return;
    FILE *fp = popen("git rev-parse HEAD 2>/dev/null", "r");
    if (fp) {
        char sha[128] = {0};
        if (fgets(sha, sizeof(sha), fp)) {
            size_t n = strlen(sha);
            while (n && (sha[n - 1] == '\n' || sha[n - 1] == ' '))
                sha[--n] = '\0';
            json_set(obj, "sha", json_string(sha));
        }
        pclose(fp);
    }
    fp = popen(
        "git describe --tags --abbrev=0 2>/dev/null || cat VERSION 2>/dev/null",
        "r");
    if (fp) {
        char ver[128] = {0};
        if (fgets(ver, sizeof(ver), fp)) {
            size_t n = strlen(ver);
            while (n && (ver[n - 1] == '\n' || ver[n - 1] == ' '))
                ver[--n] = '\0';
            if (ver[0]) json_set(obj, "version", json_string(ver));
        }
        pclose(fp);
    }
}

/* PoP: __init__ @hermes_cli/update_receipt.py:__init__ */
update_receipt_t *update_receipt_new(void)
{
    update_receipt_t *r = calloc(1, sizeof(*r));
    if (!r) return NULL;
    r->data = json_object();
    if (!r->data) { free(r); return NULL; }
    json_set(r->data, "schema", json_int(1));
    char *now = ur_utc_now_iso();
    if (now) { json_set(r->data, "started_at", json_string(now)); free(now); }
    else json_set(r->data, "started_at", json_null());
    json_set(r->data, "finished_at", json_null());

    extern int g_argc_shared;
    extern char **g_argv_shared;
    json_t *argv = json_array();
    for (int i = 0; i < g_argc_shared && argv; i++)
        json_append(argv, g_argv_shared[i] ? json_string(g_argv_shared[i])
                                           : json_null());
    if (argv) json_set(r->data, "argv", argv);

    json_set(r->data, "pid", json_int((int)getpid()));
    json_set(r->data, "outcome", json_string("running"));
    json_t *pre = json_object();
    ur_add_code_identity(pre);
    json_set(r->data, "pre_update", pre);
    json_set(r->data, "post_update", json_object());
    json_set(r->data, "steps", json_array());
    json_set(r->data, "skips", json_array());
    json_set(r->data, "gateway_restart", json_object());
    json_set(r->data, "fleet", json_array());
    return r;
}

void update_receipt_free(update_receipt_t *receipt)
{
    if (!receipt) return;
    if (receipt->data) json_free(receipt->data);
    free(receipt);
}

json_t *update_receipt_data(update_receipt_t *r) { return r ? r->data : NULL; }

/* PoP: step @hermes_cli/update_receipt.py:step */
void update_receipt_step(update_receipt_t *r, const char *name, int ok,
                         const char *detail)
{
    if (!r || !r->data) return;
    json_t *entry = json_object();
    if (!entry) return;
    json_set(entry, "name", json_string(name ? name : ""));
    json_set(entry, "ok", json_bool(ok != 0));
    json_set(entry, "detail", json_string(detail ? detail : ""));
    char *at = ur_utc_now_iso();
    if (at) { json_set(entry, "at", json_string(at)); free(at); }
    json_t *steps = json_obj_get(r->data, "steps");
    if (steps && steps->type == JSON_ARRAY) json_append(steps, entry);
    else json_free(entry);
}

/* PoP: skip @hermes_cli/update_receipt.py:skip */
void update_receipt_skip(update_receipt_t *r, const char *name,
                         const char *reason)
{
    if (!r || !r->data) return;
    json_t *entry = json_object();
    if (!entry) return;
    json_set(entry, "name", json_string(name ? name : ""));
    json_set(entry, "reason", json_string(reason ? reason : ""));
    char *at = ur_utc_now_iso();
    if (at) { json_set(entry, "at", json_string(at)); free(at); }
    json_t *skips = json_obj_get(r->data, "skips");
    if (skips && skips->type == JSON_ARRAY) json_append(skips, entry);
    else json_free(entry);
}

/* PoP: gateway_restart_result @hermes_cli/update_receipt.py:gateway_restart_result */
void update_receipt_gateway_restart_result(
    update_receipt_t *r, json_t *restarted_services,
    json_t *relaunched_profiles, json_t *externally_supervised_profiles,
    const long *killed_pids, size_t killed_n, const char **failed_units,
    size_t failed_n, int incomplete, const char *phase_error)
{
    if (!r || !r->data) return;
    json_t *gr = json_object();
    if (!gr) return;
    json_t *empty_arr(void) { return json_array(); }
    json_set(gr, "restarted_services",
             restarted_services ? restarted_services : empty_arr());
    json_set(gr, "relaunched_profiles",
             relaunched_profiles ? relaunched_profiles : empty_arr());
    json_set(gr, "externally_supervised_profiles",
             externally_supervised_profiles ? externally_supervised_profiles
                                            : empty_arr());
    json_t *pids = json_array();
    for (size_t i = 0; i < killed_n && pids; i++)
        json_append(pids, json_int((int)killed_pids[i]));
    json_set(gr, "killed_pids", pids);
    json_t *units = json_array();
    for (size_t i = 0; i < failed_n && units; i++) {
        json_append(units, json_string(failed_units[i] ? failed_units[i] : ""));
    }
    json_set(gr, "failed_units", units);
    json_set(gr, "incomplete", json_bool(incomplete != 0));
    json_set(gr, "phase_error",
             json_string(phase_error ? phase_error : ""));
    /* replace any previous gateway_restart payload */
    json_t *old = json_obj_get(r->data, "gateway_restart");
    if (old) {
        for (size_t i = 0; i < r->data->c.count; i++)
            if (r->data->c.items[i] == old) { r->data->c.items[i] = gr; break; }
    } else {
        json_set(r->data, "gateway_restart", gr);
    }
}

/* PoP: finalize @hermes_cli/update_receipt.py:finalize */
void update_receipt_finalize(update_receipt_t *r, const char *outcome)
{
    if (!r || !r->data) return;
    json_set(r->data, "outcome",
             json_string(outcome ? outcome : ""));
    char *finished = ur_utc_now_iso();
    if (finished) {
        json_set(r->data, "finished_at", json_string(finished));
        free(finished);
    }
    json_t *post = json_obj_get(r->data, "post_update");
    ur_add_code_identity(post);
}

update_receipt_t *update_receipt_current(void) { return ur_current; }

void update_receipt_set_current(update_receipt_t *r) { ur_current = r; }

/* PoP: _receipt_dir @hermes_cli/update_receipt.py:_receipt_dir */
char *update_receipt_dir(void)
{
    char home[1024];
    extern void hermes_get_home(char *, size_t);
    hermes_get_home(home, sizeof(home));
    char *out = malloc(strlen(home) + 64);
    if (!out) return NULL;
    sprintf(out, "%s/logs/%s", home, UR_RECEIPT_DIR_NAME);
    return out;
}

/* PoP: begin_update_receipt @hermes_cli/update_receipt.py:begin_update_receipt */
void begin_update_receipt(void) { ur_current = update_receipt_new(); }

/* PoP: record_step @hermes_cli/update_receipt.py:record_step */
void record_step(const char *name, int ok, const char *detail)
{
    if (ur_current) update_receipt_step(ur_current, name, ok, detail);
}

/* PoP: record_skip @hermes_cli/update_receipt.py:record_skip */
void record_skip(const char *name, const char *reason)
{
    if (ur_current) update_receipt_skip(ur_current, name, reason);
}

/* PoP: record_gateway_restart @hermes_cli/update_receipt.py:record_gateway_restart */
void record_gateway_restart(json_t *restarted_services,
                            json_t *relaunched_profiles,
                            json_t *externally_supervised_profiles,
                            const long *killed_pids, size_t killed_n,
                            const char **failed_units, size_t failed_n,
                            int incomplete, const char *phase_error)
{
    if (!ur_current) {
        if (restarted_services) json_free(restarted_services);
        if (relaunched_profiles) json_free(relaunched_profiles);
        if (externally_supervised_profiles)
            json_free(externally_supervised_profiles);
        return;
    }
    update_receipt_gateway_restart_result(
        ur_current, restarted_services, relaunched_profiles,
        externally_supervised_profiles, killed_pids, killed_n, failed_units,
        failed_n, incomplete, phase_error);
}

/* PoP: _prune_old_receipts @hermes_cli/update_receipt.py:_prune_old_receipts */
void prune_old_receipts(const char *directory)
{
    DIR *dir = opendir(directory);
    if (!dir) return;
    struct dirent *de;
    while ((de = readdir(dir)) != NULL) {
        if (strncmp(de->d_name, "update_", 7) != 0 ||
            strlen(de->d_name) < 6 ||
            strcmp(de->d_name + strlen(de->d_name) - 5, ".json") != 0)
            continue;
        char full[2048];
        snprintf(full, sizeof(full), "%s/%s", directory, de->d_name);
        /* collect then sort by mtime below via a simple selection pass */
        (void)full;
    }
    closedir(dir);
    /* Simple bounded prune: repeatedly delete the OLDEST receipt beyond the
     * keep window (N=20), matching Python's sorted(mtime)[-KEEP:] removal. */
    for (int pass = 0; pass < 4096; pass++) {
        char oldest[2048] = {0};
        time_t oldest_mtime = (time_t)-1;
        int count = 0;
        dir = opendir(directory);
        if (!dir) return;
        while ((de = readdir(dir)) != NULL) {
            size_t len = strlen(de->d_name);
            if (strncmp(de->d_name, "update_", 7) != 0 || len < 6 ||
                strcmp(de->d_name + len - 5, ".json") != 0)
                continue;
            count++;
            char full[2048];
            snprintf(full, sizeof(full), "%s/%s", directory, de->d_name);
            struct stat st;
            if (stat(full, &st) != 0) continue;
            if (oldest_mtime == (time_t)-1 || st.st_mtime < oldest_mtime) {
                oldest_mtime = st.st_mtime;
                snprintf(oldest, sizeof(oldest), "%s", full);
            }
        }
        closedir(dir);
        if (count <= UR_RECEIPT_KEEP || !oldest[0]) return;
        unlink(oldest);
    }
}

/* PoP: finalize_update_receipt @hermes_cli/update_receipt.py:finalize_update_receipt */
char *finalize_update_receipt(const char *outcome, json_t *fleet,
                              const char *stop_reason)
{
    update_receipt_t *receipt = ur_current;
    ur_current = NULL;
    if (!receipt) return NULL;
    char *written = NULL;
    do {
        update_receipt_finalize(receipt, outcome);
        if (stop_reason && stop_reason[0])
            json_set(receipt->data, "stop_reason", json_string(stop_reason));
        if (fleet)
            json_set(receipt->data, "fleet", fleet); /* ownership moves */

        char *directory = update_receipt_dir();
        if (!directory) break;
        mkdir(directory, 0755); /* parents best-effort */
        time_t t = time(NULL);
        struct tm tm;
        localtime_r(&t, &tm);
        char stamp[32];
        strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M%S", &tm);
        char path[1400];
        snprintf(path, sizeof(path), "%s/update_%s_%d.json", directory, stamp,
                 (int)getpid());
        char *payload = json_serialize_pretty(receipt->data, 2);
        FILE *fp = fopen(path, "w");
        if (fp && payload) {
            fputs(payload, fp);
            fclose(fp);
            written = ur_strdup(path);
        } else if (fp) {
            fclose(fp);
        }
        free(payload);
        /* Stable pointer for the dashboard/desktop: latest receipt. */
        char latest[1400];
        snprintf(latest, sizeof(latest), "%s/latest.json", directory);
        payload = json_serialize_pretty(receipt->data, 2);
        fp = fopen(latest, "w");
        if (fp && payload) {
            fputs(payload, fp);
        }
        if (fp) fclose(fp);
        free(payload);
        prune_old_receipts(directory);
        free(directory);
    } while (0);
    update_receipt_free(receipt);
    return written;
}

/* PoP: finalize_pending_update_receipt @hermes_cli/update_receipt.py:finalize_pending_update_receipt */
char *finalize_pending_update_receipt(int exit_code, const char *stop_reason)
{
    if (!ur_current) return NULL;
    const char *outcome;
    if (exit_code == 0 || exit_code == -1)
        outcome = "success";
    else if (exit_code == 2)
        outcome = "refused";
    else
        outcome = "failed";
    if (exit_code >= 0)
        json_set(ur_current->data, "exit_code", json_int(exit_code));
    return finalize_update_receipt(outcome, NULL, stop_reason);
}

/* PoP: read_latest_receipt @hermes_cli/update_receipt.py:read_latest_receipt */
json_t *read_latest_receipt(void)
{
    char *directory = update_receipt_dir();
    if (!directory) return NULL;
    char path[1400];
    snprintf(path, sizeof(path), "%s/latest.json", directory);
    free(directory);
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return NULL; }
    char *text = malloc((size_t)sz + 1);
    if (!text) { fclose(fp); return NULL; }
    size_t rd = fread(text, 1, (size_t)sz, fp);
    fclose(fp);
    text[rd] = '\0';
    json_t *payload = json_parse(text, NULL);
    free(text);
    if (!payload || payload->type != JSON_OBJECT) {
        if (payload) json_free(payload);
        return NULL;
    }
    return payload;
}

/* Profile homes in sorted order: ("default", default_home) plus each valid
 * profile dir under profiles/. Mirrors _get_default_hermes_home +
 * _get_profiles_root + _PROFILE_ID_RE enumeration. */
static int pi_profile_id_valid(const char *name)
{
    /* _PROFILE_ID_RE ≈ [A-Za-z0-9][A-Za-z0-9_-]* */
    if (!name || !name[0]) return 0;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

static void ur_fleet_state(json_t *identity_or_record,
                           const char *expected_sha, const char *state_out[],
                           const char **code_sha, const char **code_version)
{
    /* helper computing state per the shared ladder */
    json_t *sha_node =
        identity_or_record ? json_obj_get(identity_or_record, "code_sha") : NULL;
    *code_sha = (sha_node && sha_node->type == JSON_STRING && sha_node->str_val[0])
                    ? sha_node->str_val : NULL;
    json_t *ver_node =
        identity_or_record
            ? json_obj_get(identity_or_record, "code_version")
            : NULL;
    *code_version =
        (ver_node && ver_node->type == JSON_STRING) ? ver_node->str_val : NULL;
    if (!*code_sha || !expected_sha)
        *state_out = "unknown";
    else if (strcmp(*code_sha, expected_sha) == 0)
        *state_out = "current";
    else
        *state_out = "stale";
}

/* PoP: collect_fleet_versions @hermes_cli/update_receipt.py:collect_fleet_versions */
json_t *collect_fleet_versions(void)
{
    json_t *results = json_array();
    if (!results) return NULL;

    /* expected sha from the freshly-updated checkout */
    char expected_sha[80] = {0};
    {
        FILE *fp = popen("git rev-parse HEAD 2>/dev/null", "r");
        if (fp) {
            if (!fgets(expected_sha, sizeof(expected_sha), fp))
                expected_sha[0] = '\0';
            pclose(fp);
            size_t n = strlen(expected_sha);
            while (n && (expected_sha[n - 1] == '\n' ||
                         expected_sha[n - 1] == ' '))
                expected_sha[--n] = '\0';
        }
    }

    char *default_home_c = profile_default_home();
    char *root = profile_profiles_root();
    char homes[64][1024];
    char names[64][128];
    size_t hn = 0;
    if (default_home_c) {
        struct stat st;
        if (stat(default_home_c, &st) == 0 && S_ISDIR(st.st_mode) &&
            hn < 64) {
            snprintf(names[hn], 128, "default");
            snprintf(homes[hn], 1024, "%s", default_home_c);
            hn++;
        }
        free(default_home_c);
    }
    if (root) {
        DIR *d = opendir(root);
        if (d) {
            /* collect + sort by name */
            char *cands[256];
            size_t cn = 0;
            struct dirent *de;
            while ((de = readdir(d)) != NULL && cn < 256) {
                if (de->d_name[0] == '.' ||
                    strcmp(de->d_name, "default") == 0 ||
                    !pi_profile_id_valid(de->d_name))
                    continue;
                cands[cn++] = strdup(de->d_name);
            }
            closedir(d);
            for (size_t i = 0; i + 1 < cn; i++)
                for (size_t j = i + 1; j < cn; j++)
                    if (strcmp(cands[i], cands[j]) > 0) {
                        char *tmp = cands[i];
                        cands[i] = cands[j];
                        cands[j] = tmp;
                    }
            for (size_t i = 0; i < cn && hn < 64; i++) {
                snprintf(names[hn], 128, "%s", cands[i]);
                snprintf(homes[hn], 1024, "%s/%s", root, cands[i]);
                hn++;
            }
            for (size_t i = 0; i < cn; i++) free(cands[i]);
        }
        free(root);
    }

    for (size_t h = 0; h < hn; h++) {
        const char *profile = names[h];
        const char *home = homes[h];
        /* Prefer the gateway-owned control socket (#92091). */
        json_t *identity = control_socket_identify_gateway(home, 0);
        if (identity) {
            json_t *pid_node = json_obj_get(identity, "pid");
            long pid = (pid_node && pid_node->type == JSON_NUMBER)
                           ? (long)pid_node->num_val : 0;
            if (pid > 0) {
                const char *state;
                const char *code_sha;
                const char *code_version;
                ur_fleet_state(identity, expected_sha, &state, &code_sha,
                               &code_version);
                json_t *e = json_object();
                if (e) {
                    json_set(e, "profile", json_string(profile));
                    json_set(e, "pid", json_int((int)pid));
                    json_set(e, "code_sha",
                             code_sha ? json_string(code_sha) : json_null());
                    json_set(e, "code_version",
                             code_version ? json_string(code_version)
                                          : json_null());
                    json_set(e, "state", json_string(state));
                    json_set(e, "source", json_string("socket"));
                    json_append(results, e);
                }
                json_free(identity);
                continue;
            }
            json_free(identity);
        }
        /* Fall back to gateway_state.json for pre-socket gateways. */
        char state_path[1200];
        snprintf(state_path, sizeof(state_path),
                 "%s/gateway_state.json", home);
        char *raw = gwstatus_read_runtime_status(state_path);
        if (!raw) continue;
        json_t *record = json_parse(raw, NULL);
        free(raw);
        if (!record) continue;
        json_t *pid_node = json_obj_get(record, "pid");
        long pid = (pid_node && pid_node->type == JSON_NUMBER)
                       ? (long)pid_node->num_val : 0;
        if (pid <= 0 || !gwstatus_pid_exists(pid)) {
            json_free(record);
            continue;
        }
        const char *state;
        const char *code_sha;
        const char *code_version;
        ur_fleet_state(record, expected_sha, &state, &code_sha,
                       &code_version);
        json_t *e = json_object();
        if (e) {
            json_set(e, "profile", json_string(profile));
            json_set(e, "pid", json_int((int)pid));
            json_set(e, "code_sha",
                     code_sha ? json_string(code_sha) : json_null());
            json_set(e, "code_version",
                     code_version ? json_string(code_version) : json_null());
            json_set(e, "state", json_string(state));
            json_append(results, e);
        }
        json_free(record);
    }
    return results;
}

/* PoP: print_fleet_version_matrix @hermes_cli/update_receipt.py:print_fleet_version_matrix */
int print_fleet_version_matrix(json_t *fleet)
{
    if (!fleet || fleet->type != JSON_ARRAY || json_len(fleet) == 0)
        return 0;
    int any_stale = 0;
    printf("\nFleet version check:\n");
    for (size_t i = 0; i < json_len(fleet); i++) {
        json_t *e = json_get(fleet, i);
        if (!e || e->type != JSON_OBJECT) continue;
        json_t *sha_node = json_obj_get(e, "code_sha");
        const char *sha =
            (sha_node && sha_node->type == JSON_STRING) ? sha_node->str_val : NULL;
        char short_sha[9] = "?";
        short_sha[1] = '\0';
        if (sha && sha[0]) {
            snprintf(short_sha, sizeof(short_sha), "%.8s", sha);
        }
        json_t *st = json_obj_get(e, "state");
        const char *state = (st && st->type == JSON_STRING) ? st->str_val : "";
        json_t *prof = json_obj_get(e, "profile");
        const char *profile =
            (prof && prof->type == JSON_STRING) ? prof->str_val : "?";
        json_t *pid_node = json_obj_get(e, "pid");
        long pid =
            (pid_node && pid_node->type == JSON_NUMBER) ? (long)pid_node->num_val : 0;
        if (strcmp(state, "current") == 0) {
            printf("  \xe2\x9c\x93 %s (pid %ld) @ %s \xe2\x80\x94 up to date\n",
                   profile, pid, short_sha);
        } else if (strcmp(state, "stale") == 0) {
            any_stale = 1;
            printf("  \xe2\x9c\x97 %s (pid %ld) @ %s \xe2\x80\x94 STALE "
                   "(pre-update code)\n",
                   profile, pid, short_sha);
        } else {
            printf("  ? %s (pid %ld) \xe2\x80\x94 version unknown (gateway "
                   "predates version stamping; restart to enable)\n",
                   profile, pid);
        }
    }
    if (any_stale) {
        printf("\n");
        printf("  \xe2\x9a\xa0 Stale gateways keep serving pre-update code "
               "until restarted:\n");
        printf("      slermes gateway restart                # active profile\n");
        printf("      slermes -p <profile> gateway restart   # named profile\n");
    }
    return any_stale;
}
