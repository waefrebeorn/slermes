/*
 * update_inventory.c — C11 port of hermes_cli/update_inventory.py
 *
 * Deliberately side-effect free: every collector probes primitives that
 * already exist (control-socket identify, gateway_state.json stamps,
 * PID-file scans) so `hermes update --plan` can run on a live fleet with
 * zero risk. A probe failure yields fewer rows, not an exception.
 */
#define _POSIX_C_SOURCE 200809L

#include "update_inventory.h"
#include "update_receipt.h"
#include "control_socket.h"
#include "profile_store.h"
#include "gateway_status.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

static char *ui_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *out = malloc(n);
    if (out) memcpy(out, s, n);
    return out;
}

void upd_runtime_record_free(upd_runtime_record_t *rec)
{
    if (!rec) return;
    free(rec->kind);
    free(rec->profile);
    free(rec->code_sha);
    free(rec->code_version);
    free(rec->restart_via);
}

void upd_plan_free(upd_plan_t *plan)
{
    if (!plan) return;
    for (size_t i = 0; i < plan->profiles_n; i++) free(plan->profiles[i]);
    free(plan->profiles);
    for (size_t i = 0; i < plan->runtimes_n; i++)
        upd_runtime_record_free(&plan->runtimes[i]);
    free(plan->runtimes);
    free(plan->expected_sha);
    free(plan->expected_version);
}

/* PoP: to_dict @hermes_cli/update_inventory.py:to_dict */
json_t *upd_runtime_record_to_dict(const upd_runtime_record_t *rec)
{
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "kind", json_string(rec->kind ? rec->kind : ""));
    json_set(o, "profile", json_string(rec->profile ? rec->profile : ""));
    if (rec->pid > 0)
        json_set(o, "pid", json_int((int)rec->pid));
    else
        json_set(o, "pid", json_null());
    json_set(o, "supervisor",
             json_string(rec->supervisor[0] ? rec->supervisor : "manual"));
    json_set(o, "code_sha",
             rec->code_sha ? json_string(rec->code_sha) : json_null());
    json_set(o, "code_version",
             rec->code_version ? json_string(rec->code_version) : json_null());
    json_set(o, "restart_via",
             json_string(rec->restart_via ? rec->restart_via : ""));
    return o;
}

/* PoP: to_dict @hermes_cli/update_inventory.py:to_dict */
json_t *upd_plan_to_dict(const upd_plan_t *plan)
{
    if (!plan) return NULL;
    json_t *o = json_object();
    if (!o) return NULL;
    json_set(o, "install_method",
             json_string(plan->install_method[0] ? plan->install_method
                                                 : "unknown"));
    json_set(o, "updatable_in_place",
             json_bool(plan->updatable_in_place != 0));
    json_set(o, "update_mechanism",
             json_string(plan->update_mechanism[0]
                             ? plan->update_mechanism
                             : "hermes update"));
    json_set(o, "expected_sha",
             plan->expected_sha ? json_string(plan->expected_sha)
                                : json_null());
    json_set(o, "expected_version",
             plan->expected_version ? json_string(plan->expected_version)
                                    : json_null());
    json_t *profiles = json_array();
    for (size_t i = 0; i < plan->profiles_n && profiles; i++)
        json_append(profiles,
                    json_string(plan->profiles[i] ? plan->profiles[i] : ""));
    json_set(o, "profiles", profiles);
    json_t *runtimes = json_array();
    for (size_t i = 0; i < plan->runtimes_n && runtimes; i++) {
        json_t *rd = upd_runtime_record_to_dict(&plan->runtimes[i]);
        if (rd) json_append(runtimes, rd);
    }
    json_set(o, "runtimes", runtimes);
    return o;
}

/* PoP: _detect_supervisor_for_pid @hermes_cli/update_inventory.py:_detect_supervisor_for_pid */
void update_inventory_detect_supervisor_for_pid(long pid,
                                                const long *service_pids,
                                                size_t n, char out[16])
{
    int is_service = 0;
    for (size_t i = 0; i < n; i++)
        if (service_pids[i] == pid) { is_service = 1; break; }
    if (is_service) {
        /* supports_systemd_services() / is_macos() probes degrade to the
         * generic "service" bucket on this platform. */
        const char *invocation = getenv("INVOCATION_ID");
        if (access("/run/systemd/system", F_OK) == 0 ||
            (invocation && invocation[0])) {
            snprintf(out, 16, "systemd");
            return;
        }
#if __APPLE__
        snprintf(out, 16, "launchd");
#else
        snprintf(out, 16, "service");
#endif
        return;
    }
    snprintf(out, 16, "manual");
}

/* PoP: _restart_mechanism @hermes_cli/update_inventory.py:_restart_mechanism */
char *update_inventory_restart_mechanism(const char *supervisor,
                                         const char *profile)
{
    char buf[256];
    if (strcmp(supervisor, "systemd") == 0)
        snprintf(buf, sizeof(buf),
                 "systemctl restart (drain-first SIGUSR1 when supported)");
    else if (strcmp(supervisor, "launchd") == 0)
        snprintf(buf, sizeof(buf),
                 "launchctl kickstart -k (drain-first, per-label domain)");
    else if (strcmp(supervisor, "desktop") == 0)
        snprintf(buf, sizeof(buf), "Desktop app respawns its serve backend");
    else if (profile && strcmp(profile, "default") != 0)
        snprintf(buf, sizeof(buf), "hermes -p %s gateway restart", profile);
    else
        snprintf(buf, sizeof(buf), "hermes gateway restart");
    return ui_strdup(buf);
}

static void ui_append_runtime(upd_plan_t *plan, const char *kind,
                              const char *profile, long pid,
                              const char *supervisor, const char *code_sha,
                              const char *code_version)
{
    upd_runtime_record_t *recs =
        realloc(plan->runtimes, (plan->runtimes_n + 1) * sizeof(*recs));
    if (!recs) return;
    plan->runtimes = recs;
    upd_runtime_record_t *r = &plan->runtimes[plan->runtimes_n++];
    memset(r, 0, sizeof(*r));
    r->kind = ui_strdup(kind);
    r->profile = ui_strdup(profile);
    r->pid = pid;
    snprintf(r->supervisor, sizeof(r->supervisor), "%s", supervisor);
    r->code_sha = code_sha ? ui_strdup(code_sha) : NULL;
    r->code_version = code_version ? ui_strdup(code_version) : NULL;
    r->restart_via =
        update_inventory_restart_mechanism(supervisor, profile);
}

static int pi_profile_id_valid(const char *name)
{
    if (!name || !name[0]) return 0;
    for (const char *p = name; *p; p++) {
        char c = *p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

/* PoP: collect_runtime_inventory @hermes_cli/update_inventory.py:collect_runtime_inventory */
int collect_runtime_inventory(upd_plan_t *out_plan)
{
    upd_plan_t *plan = out_plan;
    memset(plan, 0, sizeof(*plan));
    plan->updatable_in_place = 1;
    snprintf(plan->install_method, sizeof(plan->install_method), "unknown");
    snprintf(plan->update_mechanism, sizeof(plan->update_mechanism),
             "hermes update");

    /* --- install shape / deployment kind --------------------------------
     * detect_install_method()/get_managed_system() have no C-side probe yet;
     * degrade exactly like Python's except branch: defaults stand. */

    /* --- expected code identity (pre-pull) ------------------------------ */
    {
        FILE *fp = popen("git rev-parse HEAD 2>/dev/null", "r");
        if (fp) {
            char sha[80] = {0};
            if (fgets(sha, sizeof(sha), fp)) {
                size_t n = strlen(sha);
                while (n && (sha[n - 1] == '\n' || sha[n - 1] == ' '))
                    sha[--n] = '\0';
                if (sha[0]) plan->expected_sha = ui_strdup(sha);
            }
            pclose(fp);
        }
        fp = popen("git describe --tags --abbrev=0 2>/dev/null", "r");
        if (fp) {
            char ver[80] = {0};
            if (fgets(ver, sizeof(ver), fp)) {
                size_t n = strlen(ver);
                while (n && (ver[n - 1] == '\n' || ver[n - 1] == ' '))
                    ver[--n] = '\0';
                if (ver[0]) plan->expected_version = ui_strdup(ver);
            }
            pclose(fp);
        }
    }

    /* --- profiles --------------------------------------------------------*/
    char homes[64][1024];
    size_t hn = 0;
    char *default_home_c = profile_default_home();
    if (default_home_c) {
        struct stat st;
        if (stat(default_home_c, &st) == 0 && S_ISDIR(st.st_mode) && hn < 64) {
            snprintf(homes[hn], 1024, "%s", default_home_c);
            hn++;
        }
        free(default_home_c);
    }
    char *root = profile_profiles_root();
    if (root) {
        DIR *d = opendir(root);
        if (d) {
            struct dirent *de;
            while ((de = readdir(d)) != NULL && hn < 64) {
                if (de->d_name[0] == '.' ||
                    strcmp(de->d_name, "default") == 0 ||
                    !pi_profile_id_valid(de->d_name))
                    continue;
                char full[1200];
                snprintf(full, sizeof(full), "%s/%s", root, de->d_name);
                struct stat st;
                if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
                    snprintf(homes[hn++], 1024, "%s", full);
            }
            closedir(d);
        }
        free(root);
    }
    plan->profiles = malloc((hn ? hn : 1) * sizeof(char *));
    if (plan->profiles) {
        for (size_t i = 0; i < hn; i++) {
            /* profile label: basename of home ("default" for the first). */
            const char *label = strrchr(homes[i], '/');
            label = label ? label + 1 : homes[i];
            plan->profiles[plan->profiles_n++] = ui_strdup(label);
        }
    }

    /* --- per-profile gateways (socket-first, state-file fallback) -------*/
    long seen_pids[256];
    size_t seen_n = 0;
    for (size_t h = 0; h < hn; h++) {
        const char *home_dir = homes[h];
        const char *label = strrchr(home_dir, '/');
        label = label ? label + 1 : home_dir;

        json_t *identity = control_socket_identify_gateway(home_dir, 0);
        long sock_pid = 0;
        if (identity) {
            json_t *pid_node = json_obj_get(identity, "pid");
            sock_pid = (pid_node && pid_node->type == JSON_NUMBER)
                           ? (long)pid_node->num_val : 0;
        }
        if (identity && sock_pid > 0) {
            int dup = 0;
            for (size_t i = 0; i < seen_n; i++)
                if (seen_pids[i] == sock_pid) { dup = 1; break; }
            if (!dup && seen_n < 256) {
                seen_pids[seen_n++] = sock_pid;
                /* declared supervisor wins over outside-in detection */
                json_t *sup = json_obj_get(identity, "supervisor");
                char supervisor[16] = "";
                if (sup && sup->type == JSON_STRING && sup->str_val[0]) {
                    snprintf(supervisor, sizeof(supervisor), "%.15s",
                             sup->str_val);
                } else {
                    update_inventory_detect_supervisor_for_pid(
                        sock_pid, NULL, 0, supervisor);
                }
                json_t *sha_node = json_obj_get(identity, "code_sha");
                const char *sha =
                    (sha_node && sha_node->type == JSON_STRING &&
                     sha_node->str_val[0]) ? sha_node->str_val : NULL;
                json_t *ver_node = json_obj_get(identity, "code_version");
                const char *version =
                    (ver_node && ver_node->type == JSON_STRING)
                        ? ver_node->str_val : NULL;
                ui_append_runtime(plan, "gateway", label, sock_pid,
                                  supervisor, sha, version);
            }
            json_free(identity);
            continue;
        }
        if (identity) json_free(identity);

        char state_path[1200];
        snprintf(state_path, sizeof(state_path), "%s/gateway_state.json",
                 home_dir);
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
        int dup = 0;
        for (size_t i = 0; i < seen_n; i++)
            if (seen_pids[i] == pid) { dup = 1; break; }
        if (!dup && seen_n < 256) {
            seen_pids[seen_n++] = pid;
            char supervisor[16];
            update_inventory_detect_supervisor_for_pid(pid, NULL, 0,
                                                       supervisor);
            json_t *sha_node = json_obj_get(record, "code_sha");
            const char *sha = (sha_node && sha_node->type == JSON_STRING &&
                               sha_node->str_val[0])
                                  ? sha_node->str_val : NULL;
            json_t *ver_node = json_obj_get(record, "code_version");
            const char *version =
                (ver_node && ver_node->type == JSON_STRING)
                    ? ver_node->str_val : NULL;
            ui_append_runtime(plan, "gateway", label, pid, supervisor, sha,
                              version);
        }
        json_free(record);
    }
    return 1;
}

/* PoP: print_update_plan @hermes_cli/update_inventory.py:print_update_plan */
void print_update_plan(const upd_plan_t *plan)
{
    if (!plan) return;
    printf("Update plan:\n");
    printf("  Install: %s", plan->install_method);
    if (plan->expected_version) {
        printf(" (v%s", plan->expected_version);
        if (plan->expected_sha) printf(" @ %.8s", plan->expected_sha);
        printf(")");
    }
    printf("\n");
    if (!plan->updatable_in_place) {
        printf("  \xe2\x9a\xa0 This install is NOT updatable in place.\n");
        printf("    Update via: %s\n", plan->update_mechanism);
    }
    if (plan->profiles_n) {
        printf("  Profiles: ");
        for (size_t i = 0; i < plan->profiles_n; i++)
            printf("%s%s", i ? ", " : "", plan->profiles[i]);
        printf("\n");
    } else {
        printf("  Profiles: (none found)\n");
    }
    if (!plan->runtimes_n) {
        printf("  Running Hermes services: none detected — code swap only.\n");
        return;
    }
    printf("  Running services to restart (%zu):\n", plan->runtimes_n);
    for (size_t i = 0; i < plan->runtimes_n; i++) {
        const upd_runtime_record_t *r = &plan->runtimes[i];
        if (r->code_sha)
            printf("    \xe2\x80\xa2 %s [%s] pid %ld — %s @ %.8s\n",
                   r->kind, r->profile, r->pid, r->supervisor, r->code_sha);
        else
            printf("    \xe2\x80\xa2 %s [%s] pid %ld — %s\n", r->kind,
                   r->profile, r->pid, r->supervisor);
        printf("      restart: %s\n", r->restart_via);
    }
}

/* PoP: record_plan_in_receipt @hermes_cli/update_inventory.py:record_plan_in_receipt */
void record_plan_in_receipt(const upd_plan_t *plan)
{
    if (!plan) return;
    update_receipt_t *current = update_receipt_current();
    if (!current) return;
    json_t *data = update_receipt_data(current);
    if (!data) return;
    json_t *dict = upd_plan_to_dict(plan);
    if (!dict) return;
    json_set(data, "plan", dict); /* replaces any previous value */
}

/* ---------------------------------------------------------------------
 * describe_restart_mechanism — display strings for a mechanism id.
 * Display strings derive from the policy table (_restart_mechanism),
 * never the other way around (#91277 Phase 2).
 * --------------------------------------------------------------------- */

/* PoP: upd_describe_restart_mechanism @hermes_cli/update_inventory.py:describe_restart_mechanism */
char *update_inventory_describe_restart_mechanism(const char *mechanism,
                                                  const char *profile)
{
    char buf[256];
    if (mechanism && strcmp(mechanism, "systemd") == 0)
        snprintf(buf, sizeof(buf),
                 "systemctl restart (drain-first SIGUSR1 when supported)");
    else if (mechanism && strcmp(mechanism, "launchd") == 0)
        snprintf(buf, sizeof(buf),
                 "launchctl kickstart -k (drain-first, per-label domain)");
    else if (mechanism && strcmp(mechanism, "desktop") == 0)
        snprintf(buf, sizeof(buf), "Desktop app respawns its serve backend");
    else if (profile && strcmp(profile, "default") != 0)
        snprintf(buf, sizeof(buf), "hermes -p %s gateway restart", profile);
    else
        snprintf(buf, sizeof(buf), "hermes gateway restart");
    return ui_strdup(buf);
}

/* ---------------------------------------------------------------------
 * match_runtime_outcomes — reconcile plan vs. restart-phase bookkeeping.
 * --------------------------------------------------------------------- */

static bool upd_json_list_contains_str(const json_t *arr, const char *s)
{
    if (!arr || arr->type != JSON_ARRAY || !s) return false;
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_STRING && strcmp(e->str_val, s) == 0)
            return true;
    }
    return false;
}

static bool upd_json_list_contains_num(const json_t *arr, long v)
{
    if (!arr || arr->type != JSON_ARRAY) return false;
    size_t n = json_len(arr);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(arr, i);
        if (e && e->type == JSON_NUMBER && (long)e->num_val == v)
            return true;
    }
    return false;
}

/* failed_set / restarted_set membership: `r.profile in unit` as a substring
 * match, with the special case that profile "default" also matches
 * "hermes-gateway" units (mirrors Python's any(...) checks). */
static bool upd_units_mention_profile(const json_t *units, const char *profile)
{
    if (!units || units->type != JSON_ARRAY || !profile) return false;
    size_t n = json_len(units);
    for (size_t i = 0; i < n; i++) {
        json_t *e = json_get(units, i);
        if (!e || e->type != JSON_STRING) continue;
        if (strstr(e->str_val, profile))
            return true;
        if (strcmp(profile, "default") == 0 &&
            strstr(e->str_val, "hermes-gateway"))
            return true;
    }
    return false;
}

/* PoP: upd_match_runtime_outcomes @hermes_cli/update_inventory.py:match_runtime_outcomes */
json_t *upd_match_runtime_outcomes(const upd_plan_t *plan,
                                   const json_t *restarted_services,
                                   const json_t *relaunched_profiles,
                                   const json_t *externally_supervised_profiles,
                                   const json_t *killed_pids,
                                   const json_t *failed_units)
{
    json_t *outcomes = json_array();
    if (!outcomes || !plan) return outcomes;

    for (size_t i = 0; i < plan->runtimes_n; i++) {
        const upd_runtime_record_t *r = &plan->runtimes[i];
        const char *profile = r->profile ? r->profile : "";

        /* outcome defaults to the blind-spot tripwire. */
        const char *outcome = "unaccounted";
        if (upd_json_list_contains_str(relaunched_profiles, profile) ||
            upd_json_list_contains_str(externally_supervised_profiles, profile)) {
            outcome = "restarted";
        } else if (r->pid > 0 &&
                   upd_json_list_contains_num(killed_pids, r->pid)) {
            outcome = "stopped";
        } else if (upd_units_mention_profile(failed_units, profile)) {
            outcome = "failed";
        } else if (upd_units_mention_profile(restarted_services, profile)) {
            outcome = "restarted";
        }

        json_t *row = json_object();
        if (!row) continue;
        json_set(row, "kind", json_string(r->kind ? r->kind : ""));
        json_set(row, "profile", json_string(profile));
        if (r->pid > 0)
            json_set(row, "pid", json_int((int)r->pid));
        else
            json_set(row, "pid", json_null());
        json_set(row, "mechanism",
                 json_string(r->restart_via ? r->restart_via : ""));
        json_set(row, "outcome", json_string(outcome));
        json_append(outcomes, row);
    }
    return outcomes;
}

/* PoP: upd_report_unaccounted_runtimes @hermes_cli/update_inventory.py:report_unaccounted_runtimes */
int update_inventory_report_unaccounted_runtimes(const json_t *outcomes)
{
    if (!outcomes || outcomes->type != JSON_ARRAY) return 0;

    /* Collect unaccounted rows first so silence stays completely silent. */
    size_t n = json_len(outcomes);
    bool missed_any = false;
    for (size_t i = 0; i < n; i++) {
        json_t *o = json_get(outcomes, i);
        json_t *oc = o ? json_obj_get(o, "outcome") : NULL;
        if (oc && oc->type == JSON_STRING &&
            strcmp(oc->str_val, "unaccounted") == 0) {
            missed_any = true;
            break;
        }
    }
    if (!missed_any) return 0;

    printf("\n");
    printf("  \xe2\x9a\xa0 Planned runtimes the restart phase never touched:\n");
    for (size_t i = 0; i < n; i++) {
        json_t *o = json_get(outcomes, i);
        json_t *oc = o ? json_obj_get(o, "outcome") : NULL;
        if (!oc || oc->type != JSON_STRING ||
            strcmp(oc->str_val, "unaccounted") != 0)
            continue;
        json_t *kind = json_obj_get(o, "kind");
        json_t *profile = json_obj_get(o, "profile");
        json_t *pid = json_obj_get(o, "pid");
        json_t *mech = json_obj_get(o, "mechanism");
        printf("    \xe2\x9c\x97 %s [%s] pid %ld — planned mechanism: %s\n",
               (kind && kind->type == JSON_STRING) ? kind->str_val : "?",
               (profile && profile->type == JSON_STRING) ? profile->str_val : "?",
               (pid && pid->type == JSON_NUMBER) ? (long)pid->num_val : -1L,
               (mech && mech->type == JSON_STRING) ? mech->str_val : "?");
    }
    printf("    Restart them manually, then verify:\n");
    printf("      hermes gateway restart                # active profile\n");
    printf("      hermes -p <profile> gateway restart   # named profile\n");
    return 1;
}
