/*
 * cron.c — canonical C11 port of hermes_cli/cron.py.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "cron_jobs.h"
#include "cron_notepad.h"
#include "port_config_py_helpers.h"

/* scheduler tick boundary (src/cron/scheduler_ports.c) */
extern char *crs_tick(const char *jobs_json);
extern char *crs_run_one_job(const char *job_json);


/* === dissolved from ports_wrappers.c === */
/* PoP: hermes_cli_cron_u_normalize_skills @ hermes_cli/cron.py:_normalize_skills */
int hermes_cli_cron_u_normalize_skills(const char *arg) {
    /* Python: None -> None; single -> [single]; dedup trimmed. Arg =
     * "single\tskills_json". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (!tab) {
        /* single skill mode: single = arg, skills = None */
        printf("%s\n", arg);
        return 0;
    }
    if (tab == arg) {
        /* skills None, single None -> None */
        printf("\n");
        return 0;
    }
    json_t *arr = json_parse(tab + 1, NULL);
    if (!arr || !json_is_array(arr)) {
        if (arr) json_free(arr);
        printf("\n");
        return 0;
    }
    size_t n = json_array_size(arr);
    int first = 1;
    for (size_t i = 0; i < n; i++) {
        json_t *it = json_array_get(arr, i);
        if (!it) continue;
        const char *s = json_is_string(it) ? json_string_value(it) : "";
        while (*s == ' ') s++;
        size_t sl = strlen(s);
        while (sl > 0 && s[sl-1] == ' ') sl--;
        if (!sl) continue;
        if (!first) printf("\n");
        printf("%.*s", (int)sl, s);
        first = 0;
    }
    printf("\n");
    json_free(arr);
    return 0;
}

/* PoP: hermes_cli_cron_u_cron_api @ hermes_cli/cron.py:_cron_api */
int hermes_cli_cron_u_cron_api(const char *arg) {
    /* Python: delegate to the cronjob tool and return its JSON. Arg =
     * "action\tparams-json". */
    if (!arg || !*arg) { printf("{\n}\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    if (tab) printf("{\"action\":\"%.*s\",\"params\":%s}\n", (int)(tab - arg), arg, tab + 1);
    else printf("{\"action\":\"%s\"}\n", arg);
    return 0;
}

/* PoP: hermes_cli_cron_u_active_cron_provider_name @ hermes_cli/cron.py:_active_cron_provider_name */
int hermes_cli_cron_u_active_cron_provider_name(const char *arg) {
    /* Python: resolved provider name or builtin. Arg = "provider". */
    if (!arg || !*arg) { printf("builtin\n"); return 0; }
    printf("%s\n", arg);
    return 0;
}

/* PoP: hermes_cli_cron_u_warn_if_gateway_not_running @ hermes_cli/cron.py:_warn_if_gateway_not_running */
int hermes_cli_cron_u_warn_if_gateway_not_running(const char *arg) {
    /* Python: ticker warning. Arg =
     * "builtin\tgateway_up\tstate\tresult". */
    if (!arg || !*arg) { printf("\n"); return 0; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    int builtin = arg[0] == '1';
    int gateway_up = t1 && t1[1] == '1';
    int state = t2 && t2[1] == '1';
    if (!builtin || gateway_up || !state) { printf("no warning\n"); return 0; }
    printf("  ⚠  Gateway is not running — jobs won't fire automatically.\n");
    printf("     Start it with: hermes gateway install\n");
    printf("     Check status:  hermes cron status\n");
    return 0;
}

/* PoP: hermes_cli_cron_cron_runs @ hermes_cli/cron.py:cron_runs */
int hermes_cli_cron_cron_runs(const char *arg) {
    /* Python: execution history lines. Arg = "records_json" (array). */
    if (!arg || !*arg) { printf("No cron execution attempts recorded.\n"); return 0; }
    json_t *j = json_parse(arg, NULL);
    if (!j || !json_is_array(j) || json_array_size(j) == 0) {
        if (j) json_free(j);
        printf("No cron execution attempts recorded.\n");
        return 0;
    }
    size_t n = json_array_size(j);
    for (size_t i = 0; i < n; i++) {
        json_t *r = json_array_get(j, i);
        if (!r) continue;
        printf("%s  %-9s  job=%s  source=%s  %s\n",
               json_get_str(r, "id", "?"), json_get_str(r, "status", "?"),
               json_get_str(r, "job_id", "?"), json_get_str(r, "source", "?"),
               json_get_str(r, "claimed_at", "?"));
        const char *err = json_get_str(r, "error", "");
        if (err[0]) printf("    %s\n", err);
    }
    json_free(j);
    return 0;
}

/* PoP: hermes_cli_cron_u_print_active_jobs_summary @ hermes_cli/cron.py:_print_active_jobs_summary */
int hermes_cli_cron_u_print_active_jobs_summary(const char *arg) {
    /* Python: "<N> active job(s)" + min next run. Arg = "count\tnext_run".
     * count 0 = none. */
    if (!arg || !*arg) { printf("  No active jobs\n"); return 0; }
    const char *tab = strchr(arg, '\t');
    long count = strtol(arg, NULL, 10);
    if (count <= 0) { printf("  No active jobs\n"); return 0; }
    printf("  %ld active job(s)\n", count);
    if (tab && tab[1]) printf("  Next run: %s\n", tab + 1);
    return 0;
}

/* PoP: hermes_cli_cron_u_job_action @ hermes_cli/cron.py:_job_action */
int hermes_cli_cron_u_job_action(const char *arg) {
    /* Python: cron action result render. Arg =
     * "action\tsuccess\tname\tjob_id\tstate". */
    if (!arg || !*arg) { printf("1\n"); return 1; }
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    const char *t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
    const char *action = arg;
    int success = t1 && t1[1] == '1';
    const char *name = t2 ? t2 + 1 : "";
    const char *job_id = t3 ? t3 + 1 : "";
    if (!success) {
        printf("Failed to %s job: unknown error\n", action);
        return 1;
    }
    printf("%s job: %s (%s)\n", action, name[0] ? name : job_id, job_id);
    return 0;
}

/* PoP: hermes_cli_cron_cron_notepad @ hermes_cli/cron.py:cron_notepad */
/* Handle `hermes cron notepad <job_id> [get|set|delete|list]`. Arg =
 * "job_id\taction\tkey\tvalue". Returns exit code (0 ok, 1 error);
 * result text printed to stdout. Delegates to the cron_notepad backend. */
int hermes_cli_cron_cron_notepad(const char *arg) {
    if (!arg || !*arg) {
        printf("A job ID is required.\n");
        return 1;
    }
    char job_id[256];
    char action[16];
    char key[256];
    char value[4096];
    job_id[0] = action[0] = key[0] = value[0] = '\0';
    const char *t1 = strchr(arg, '\t');
    const char *t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
    const char *t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
    size_t jl = t1 ? (size_t)(t1 - arg) : strlen(arg);
    if (jl >= sizeof(job_id)) jl = sizeof(job_id) - 1;
    memcpy(job_id, arg, jl); job_id[jl] = '\0';
    if (t1) {
        size_t al = (size_t)(t2 ? t2 - (t1 + 1) : strlen(t1 + 1));
        if (al >= sizeof(action)) al = sizeof(action) - 1;
        memcpy(action, t1 + 1, al); action[al] = '\0';
    } else {
        strcpy(action, "list");
    }
    if (t2) {
        size_t kl = (size_t)(t3 ? t3 - (t2 + 1) : strlen(t2 + 1));
        if (kl >= sizeof(key)) kl = sizeof(key) - 1;
        memcpy(key, t2 + 1, kl); key[kl] = '\0';
    }
    if (t3) {
        size_t vl = strlen(t3 + 1);
        if (vl >= sizeof(value)) vl = sizeof(value) - 1;
        memcpy(value, t3 + 1, vl); value[vl] = '\0';
    }
    if (action[0] == '\0') strcpy(action, "list");

    if (!job_id[0]) {
        printf("A job ID is required.\n");
        return 1;
    }
    if (strcmp(action, "set") == 0) {
        if (!key[0] || !value[0]) {
            printf("Usage: hermes cron notepad <job_id> set <key> <value>\n");
            return 1;
        }
        /* delegate to backend upsert */
        json_t *res = cron_notepad_set_note(job_id, key, value, NULL);
        if (res) { json_free(res); printf("Set notepad key '%s' for job %s.\n", key, job_id); }
        else { printf("Failed to set notepad key '%s' for job %s.\n", key, job_id); return 1; }
        return 0;
    }
    if (strcmp(action, "get") == 0) {
        if (!key[0]) {
            printf("Usage: hermes cron notepad <job_id> get <key>\n");
            return 1;
        }
        char *stored = cron_notepad_get_note(job_id, key);
        if (!stored) {
            printf("No notepad key '%s' for job %s.\n", key, job_id);
            return 1;
        }
        printf("%s\n", stored);
        free(stored);
        return 0;
    }
    if (strcmp(action, "delete") == 0) {
        if (!key[0]) {
            printf("Usage: hermes cron notepad <job_id> delete <key>\n");
            return 1;
        }
        int ok = cron_notepad_delete_note(job_id, key);
        if (ok) {
            printf("Deleted notepad key '%s' for job %s.\n", key, job_id);
            return 0;
        }
        printf("No notepad key '%s' for job %s.\n", key, job_id);
        return 1;
    }
    /* list (default) */
    json_t *notes = cron_notepad_list_notes(job_id);
    if (!notes || json_array_size(notes) == 0) {
        printf("Notepad for job %s is empty.\n", job_id);
        if (notes) json_free(notes);
        return 0;
    }
    size_t n = json_array_size(notes);
    for (size_t i = 0; i < n; i++) {
        json_t *note = json_array_get(notes, i);
        if (!note) continue;
        json_t *k = json_obj_get(note, "key");
        json_t *v = json_obj_get(note, "value");
        const char *ks = (k && k->type == JSON_STRING) ? k->str_val : "";
        const char *vs = (v && v->type == JSON_STRING) ? v->str_val : "";
        printf("  %s = %s\n", ks, vs);
    }
    json_free(notes);
    return 0;
}

/* ══════════════════════════════════════════════════════════════════
 * CLI command surface (hermes_cli/cron.py cron_list/tick/status/
 * create/edit/resume) over the cronjobs_* store backends.
 * ══════════════════════════════════════════════════════════════════ */

static const char *cj_str(const json_t *o, const char *k, const char *fb) {
    json_t *v = o ? json_obj_get((json_t *)o, k) : NULL;
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : fb;
}
static double cj_num(const json_t *o, const char *k, double fb) {
    json_t *v = o ? json_obj_get((json_t *)o, k) : NULL;
    if (!v) return fb;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) { char *e; double d = strtod(v->str_val, &e); if (e != v->str_val) return d; }
    if (v->type == JSON_BOOL) return v->num_val;
    return fb;
}

/* Python _active_cron_provider_name(): config cron.provider or "builtin". */
static void cj_active_provider(char *out, size_t sz) {
    snprintf(out, sz, "builtin");
    json_t *cfg = config_py_load_config_readonly();
    if (cfg) {
        json_t *p = config_py_get_nested(cfg, "cron.provider");
        if (p && p->type == JSON_STRING && p->str_val && p->str_val[0])
            snprintf(out, sz, "%s", p->str_val);
        json_free(cfg);
    }
}

/* Python: effective_job_state(job) — scheduler-honoured paused/completed/active. */
static void cj_state_label(const json_t *job, char *out, size_t sz) {
    char *st = cronjobs_effective_job_state(job);
    snprintf(out, sz, "%s", st ? st : "active");
    free(st);
}

/* Python _warn_if_gateway_not_running() — builtin provider + ticker silent.
 * The C CLI has no in-process gateway-pid scan (gw_find_gateway_pids is
 * private to gateway.c), so liveness is judged from the shared ticker
 * heartbeat the running gateway writes every ~60s. */
static void cj_warn_if_gateway_not_running(void) {
    char provider[128];
    cj_active_provider(provider, sizeof(provider));
    if (strcmp(provider, "builtin") != 0) return;
    double hb = cronjobs_ticker_heartbeat_age();
    if (hb < 0 || hb <= 200.0) return; /* fresh heartbeat → gateway alive */
    printf("  ⚠  Gateway is not running — jobs won't fire automatically.\n");
    printf("     Start it with: hermes gateway install\n");
    printf("     Check status:   hermes cron status\n");
}

/* PoP: hermes_cli_cron_cron_list @ hermes_cli/cron.py:cron_list */
void hermes_cli_cron_cron_list(int show_all) {
    json_t *jobs = cronjobs_list_jobs(show_all ? true : false);
    if (!jobs || json_len(jobs) == 0) {
        printf("\033[2mNo scheduled jobs.\033[0m\n");
        printf("\033[2mCreate one with 'hermes cron create ...' or the /cron command in chat.\033[0m\n");
        if (jobs) json_free(jobs);
        return;
    }
    printf("\n");
    printf("\033[36m┌─────────────────────────────────────────────────────────────────────────┐\033[0m\n");
    printf("\033[36m│                         Scheduled Jobs                                  │\033[0m\n");
    printf("\033[36m└─────────────────────────────────────────────────────────────────────────┘\033[0m\n");
    printf("\n");

    size_t n = json_len(jobs);
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_get(jobs, i);
        if (!job) continue;
        const char *job_id = cj_str(job, "id", "?");
        const char *name = cj_str(job, "name", "(unnamed)");
        static char schedbuf[256];
        const char *sched = cj_str(job, "schedule_display", NULL);
        if (!sched) {
            json_t *so = json_obj_get((json_t *)job, "schedule");
            if (so && so->type == JSON_OBJECT)
                sched = cj_str(so, "value", "?");
            else
                sched = cj_str(job, "schedule", "?");
        }
        char state[32];
        cj_state_label(job, state, sizeof(state));
        const char *next_run = cj_str(job, "next_run_at", "?");

        /* repeat may be present-but-null (#32896 pattern). */
        char repeatbuf[32];
        const char *repeat_str = "∞";
        json_t *rep = json_obj_get((json_t *)job, "repeat");
        if (rep && rep->type == JSON_OBJECT) {
            json_t *times = json_obj_get(rep, "times");
            if (times && times->type == JSON_NUMBER && times->num_val > 0) {
                snprintf(repeatbuf, sizeof(repeatbuf), "%.0f/%.0f",
                         cj_num(rep, "completed", 0), times->num_val);
                repeat_str = repeatbuf;
            }
        }

        /* deliver may be present-but-null → default ["local"]. */
        char deliverbuf[256];
        const char *deliver_str = "local";
        json_t *del = json_obj_get((json_t *)job, "deliver");
        if (del && del->type == JSON_ARRAY && json_len(del) > 0) {
            deliverbuf[0] = '\0';
            for (size_t d = 0; d < json_len(del); d++) {
                json_t *it = json_get(del, d);
                if (it && it->type == JSON_STRING && it->str_val) {
                    if (deliverbuf[0]) strncat(deliverbuf, ", ", sizeof(deliverbuf) - strlen(deliverbuf) - 1);
                    strncat(deliverbuf, it->str_val, sizeof(deliverbuf) - strlen(deliverbuf) - 1);
                }
            }
            if (deliverbuf[0]) deliver_str = deliverbuf;
        } else if (del && del->type == JSON_STRING && del->str_val && del->str_val[0]) {
            deliver_str = del->str_val;
        }

        /* skills: array field, else legacy single skill string. */
        char skillsbuf[1024];
        const char *skills_str = NULL;
        json_t *sk = json_obj_get((json_t *)job, "skills");
        if (sk && sk->type == JSON_ARRAY && json_len(sk) > 0) {
            skillsbuf[0] = '\0';
            for (size_t d = 0; d < json_len(sk); d++) {
                json_t *it = json_get(sk, d);
                if (it && it->type == JSON_STRING && it->str_val) {
                    if (skillsbuf[0]) strncat(skillsbuf, ", ", sizeof(skillsbuf) - strlen(skillsbuf) - 1);
                    strncat(skillsbuf, it->str_val, sizeof(skillsbuf) - strlen(skillsbuf) - 1);
                }
            }
            if (skillsbuf[0]) skills_str = skillsbuf;
        } else if (!sk || sk->type != JSON_ARRAY) {
            const char *single = cj_str(job, "skill", NULL);
            if (single) skills_str = single;
        }

        const char *status_col = "\033[32m";
        const char *status_txt = "[active]";
        if (strcmp(state, "paused") == 0) { status_col = "\033[33m"; status_txt = "[paused]"; }
        else if (strcmp(state, "completed") == 0) { status_col = "\033[34m"; status_txt = "[completed]"; }
        else {
            json_t *en = json_obj_get((json_t *)job, "enabled");
            if (en && en->type == JSON_BOOL && !en->num_val) { status_col = "\033[31m"; status_txt = "[disabled]"; }
        }

        printf("  \033[33m%s\033[0m %s%s\033[0m\n", job_id, status_col, status_txt);
        printf("    Name:      %s\n", name);
        printf("    Schedule:  %s\n", sched);
        printf("    Repeat:    %s\n", repeat_str);
        printf("    Next run:  %s\n", next_run);
        printf("    Deliver:   %s\n", deliver_str);
        if (skills_str) printf("    Skills:    %s\n", skills_str);
        const char *script = cj_str(job, "script", NULL);
        if (script) printf("    Script:    %s\n", script);
        const char *mon = cj_str(job, "monitor_script", NULL);
        if (!mon) mon = cj_str(job, "monitor_url", NULL);
        if (mon) {
            printf("    Monitor:   %s (agent runs only on output change)\n", mon);
            json_t *ms = json_obj_get((json_t *)job, "monitor_state");
            const char *chg = cj_str(ms, "last_changed_at", NULL);
            if (chg) printf("    Changed:   %s\n", chg);
        }
        json_t *na = json_obj_get((json_t *)job, "no_agent");
        if (na && ((na->type == JSON_BOOL && na->num_val) || na->type == JSON_NUMBER))
            printf("    Mode:      \033[2mno-agent\033[0m (script stdout delivered directly)\n");
        const char *workdir = cj_str(job, "workdir", NULL);
        if (workdir) printf("    Workdir:   %s\n", workdir);

        const char *last_status = cj_str(job, "last_status", NULL);
        if (last_status) {
            const char *last_run = cj_str(job, "last_run_at", "?");
            if (strcmp(last_status, "ok") == 0) {
                printf("    Last run:  %s  \033[32mok\033[0m\n", last_run);
            } else {
                printf("    Last run:  %s  \033[31m%s: %s\033[0m\n", last_run, last_status,
                       cj_str(job, "last_error", "?"));
                int streak = (int)cj_num(job, "failure_streak", 0);
                if (streak >= 2)
                    printf("    \033[31m  (%d failures in a row)\033[0m\n", streak);
            }
        }
        json_t *lex = json_obj_get((json_t *)job, "latest_execution");
        if (lex && lex->type == JSON_OBJECT)
            printf("    Execution: %s  %s\n", cj_str(lex, "status", "?"), cj_str(lex, "id", "?"));
        const char *derr = cj_str(job, "last_delivery_error", NULL);
        if (derr) printf("    \033[33m⚠ Delivery failed:\033[0m %s\n", derr);
        json_t *ferr = json_obj_get((json_t *)job, "last_fire_error");
        if (ferr && ferr->type == JSON_OBJECT) {
            const char *detail = cj_str(ferr, "detail", NULL);
            if (detail)
                printf("    \033[31m⚠ Missed scheduled fire:\033[0m %s  %s\n",
                       cj_str(ferr, "at", "?"), detail);
        }
        printf("\n");
    }
    json_free(jobs);
    cj_warn_if_gateway_not_running();
}

/* PoP: hermes_cli_cron_cron_tick @ hermes_cli/cron.py:cron_tick */
int hermes_cli_cron_cron_tick(void) {
    /* Run due jobs once and exit. The execute→save→deliver→mark pipeline is
     * the scheduler port boundary (crs_run_one_job); this adds due-job
     * selection over the real store, run bookkeeping, heartbeat recording
     * and clean failure reporting instead of a traceback (#87644). */
    json_t *jobs = cronjobs_list_jobs(true);
    if (!jobs) {
        printf("\033[31m✗ Cron tick failed: could not read job store\033[0m\n");
        printf("  Check `hermes cron status` and the gateway log for details.\n");
        return 1;
    }
    time_t now = time(NULL);
    int failures = 0;
    size_t n = json_len(jobs);
    for (size_t i = 0; i < n; i++) {
        json_t *job = json_get(jobs, i);
        if (!job) continue;
        json_t *en = json_obj_get(job, "enabled");
        if (en && en->type == JSON_BOOL && !en->num_val) continue;
        const char *nra = cj_str(job, "next_run_at", NULL);
        if (!nra) continue;
        struct tm tmv;
        memset(&tmv, 0, sizeof(tmv));
        if (!strptime(nra, "%Y-%m-%dT%H:%M:%S", &tmv)) continue;
        tmv.tm_isdst = -1;
        time_t due = mktime(&tmv);
        if (due != (time_t)-1 && due > now) continue; /* not due yet */
        char *ser = json_serialize(job);
        char *res = crs_run_one_job(ser ? ser : "{}");
        free(ser);
        const char *id = cj_str(job, "id", "?");
        if (res) {
            cronjobs_mark_job_run(id, true, NULL, NULL);
        } else {
            failures++;
            cronjobs_mark_job_run(id, false, "tick execution failed", NULL);
        }
        free(res);
    }
    json_free(jobs);
    cronjobs_record_ticker_heartbeat(failures == 0);
    if (failures > 0) {
        printf("\033[31m✗ Cron tick failed: %d job run(s) errored\033[0m\n", failures);
        printf("  Check `hermes cron status` and the gateway log for details.\n");
        return 1;
    }
    return 0;
}

/* PoP: hermes_cli_cron_cron_status @ hermes_cli/cron.py:cron_status */
void hermes_cli_cron_cron_status(void) {
    printf("\n");
    char provider[128];
    cj_active_provider(provider, sizeof(provider));

    if (strcmp(provider, "builtin") != 0) {
        /* External provider: intentionally NO ticker thread / heartbeat file
         * between webhook fires — report the provider and skip the
         * ticker-liveness heuristics entirely. */
        printf("\033[32m✓ Cron provider: %s — jobs fire via the managed scheduler, "
               "not the in-process ticker.\033[0m\n", provider);
        printf("\033[2m  (No ticker heartbeat is expected for an external provider; "
               "due jobs are delivered by an authenticated webhook.)\033[0m\n");
        printf("\n");
        json_t *act = cronjobs_list_jobs(false);
        long cnt = act ? (long)json_len(act) : 0;
        printf("  %ld active job(s)\n", cnt);
        if (act) json_free(act);
        printf("\n");
        return;
    }

    /* Gateway liveness from the shared ticker heartbeat: Python branches on
     * find_gateway_pids() first, but that /proc walker is private to
     * gateway.c, so C judges liveness from the ~60s heartbeat the live
     * gateway ticker writes (#32612/#32895 semantics preserved via the
     * heartbeat + last-success markers). */
    const double STALE_AFTER = CRONJOBS_TICKER_INTERVAL_SECONDS * 3 + 20;
    double hb_age = cronjobs_ticker_heartbeat_age();
    double ok_age = cronjobs_ticker_success_age();

    if (hb_age >= 0 && hb_age <= STALE_AFTER) {
        printf("\033[32m✓ Gateway is running — cron jobs will fire automatically\033[0m\n");
        printf("  Ticker heartbeat: %.0fs ago\n", hb_age);
        if (ok_age >= 0 && ok_age > STALE_AFTER)
            printf("\033[33m⚠ Gateway and cron ticker are running, but no tick has "
                   "succeeded in %.0fs — ticks may be failing.\033[0m\n", ok_age);
    } else if (hb_age >= 0 && hb_age > STALE_AFTER) {
        printf("\033[33m⚠ Gateway cron ticker looks STALLED — no heartbeat for %.0fs "
               "(expected every ~%ds).\033[0m\n", hb_age, CRONJOBS_TICKER_INTERVAL_SECONDS);
        printf("  Cron jobs may NOT be firing. Restart the gateway service.\n");
    } else {
        printf("\033[31m✗ Gateway is not running — cron jobs will NOT fire\033[0m\n");
        printf("\n");
        printf("  To enable automatic execution:\n");
        printf("    hermes gateway install    # Install as a user service\n");
        printf("    sudo hermes gateway install --system  # Linux servers: boot-time system service\n");
        printf("    hermes gateway            # Or run in foreground\n");
    }
    printf("\n");
    json_t *act = cronjobs_list_jobs(false);
    long cnt = act ? (long)json_len(act) : 0;
    printf("  %ld active job(s)\n", cnt);
    if (act) json_free(act);
    printf("\n");
}

/* PoP: hermes_cli_cron_cron_create @ hermes_cli/cron.py:cron_create */
int hermes_cli_cron_cron_create(const char *args_json) {
    json_t *a = args_json ? json_parse(args_json, NULL) : NULL;
    if (!a || a->type != JSON_OBJECT) {
        if (a) json_free(a);
        printf("\033[31mFailed to create job: invalid arguments\033[0m\n");
        return 1;
    }
    json_t *skills = json_obj_get(a, "skills"); /* already normalized by caller */

    cronjobs_create_opts opts;
    memset(&opts, 0, sizeof(opts));
    opts.schedule = cj_str(a, "schedule", "");
    opts.prompt = cj_str(a, "prompt", "");
    opts.name = cj_str(a, "name", NULL);
    opts.deliver = cj_str(a, "deliver", NULL);
    json_t *rep = json_obj_get(a, "repeat");
    if (rep && rep->type == JSON_NUMBER && rep->num_val > 0) {
        opts.repeat = (int)rep->num_val;
        opts.has_repeat = true;
    }
    opts.skill = cj_str(a, "skill", NULL);
    opts.skills = skills;
    opts.script = cj_str(a, "script", NULL);
    opts.workdir = cj_str(a, "workdir", NULL);
    opts.model = cj_str(a, "model", NULL);
    opts.provider = cj_str(a, "model_provider", cj_str(a, "provider", NULL));
    json_t *nag = json_obj_get(a, "no_agent");
    opts.no_agent = nag && ((nag->type == JSON_BOOL && nag->num_val) || nag->type == JSON_NUMBER);
    opts.monitor_script = cj_str(a, "monitor_script", NULL);
    opts.monitor_url = cj_str(a, "monitor_url", NULL);

    char *err = NULL;
    json_t *created = cronjobs_create_job(&opts, &err);
    if (!created) {
        printf("\033[31mFailed to create job: %s\033[0m\n", err ? err : "unknown error");
        free(err);
        json_free(a);
        return 1;
    }
    /* continuity / reasoning_effort ride the update path (create-opts has no
     * fields for them; the store accepts them as updates). */
    json_t *cont = json_obj_get(a, "continuity");
    json_t *reff = json_obj_get(a, "reasoning_effort");
    bool cont_on = cont && ((cont->type == JSON_BOOL && cont->num_val) ||
                            (cont->type == JSON_NUMBER && cont->num_val));
    bool has_reff = reff && reff->type == JSON_STRING && reff->str_val && reff->str_val[0];
    if (cont_on || has_reff) {
        json_t *upd = json_object();
        if (cont_on) json_set(upd, "continuity", json_bool(true));
        if (has_reff) json_set(upd, "reasoning_effort", json_string(reff->str_val));
        json_t *u = cronjobs_update_job(cj_str(created, "id", ""), upd, &err);
        if (u) json_free(u);
        json_free(upd);
    }

    printf("\033[32mCreated job: %s\033[0m\n", cj_str(created, "id", "?"));
    printf("  Name: %s\n", cj_str(created, "name", "(unnamed)"));
    printf("  Schedule: %s\n", cj_str(created, "schedule_display",
                                       cj_str(created, "schedule", "?")));
    json_t *sk = json_obj_get(created, "skills");
    if (sk && sk->type == JSON_ARRAY && json_len(sk) > 0) {
        printf("  Skills: ");
        for (size_t i = 0; i < json_len(sk); i++) {
            json_t *it = json_get(sk, i);
            if (it && it->type == JSON_STRING) printf("%s%s", i ? ", " : "", it->str_val);
        }
        printf("\n");
    }
    const char *script = cj_str(created, "script", NULL);
    if (script) printf("  Script: %s\n", script);
    const char *mons = cj_str(created, "monitor_script", NULL);
    if (mons) printf("  Monitor: %s (agent runs only on output change)\n", mons);
    const char *monu = cj_str(created, "monitor_url", NULL);
    if (monu) printf("  Monitor: %s (agent runs only on output change)\n", monu);
    json_t *na = json_obj_get(created, "no_agent");
    if (na && ((na->type == JSON_BOOL && na->num_val) || na->type == JSON_NUMBER))
        printf("  Mode: no-agent (script stdout delivered directly)\n");
    json_t *co = json_obj_get(created, "continuity");
    if (co && ((co->type == JSON_BOOL && co->num_val) || co->type == JSON_STRING))
        printf("  Continuity: on (each run sees the previous run's output)\n");
    const char *wd = cj_str(created, "workdir", NULL);
    if (wd) printf("  Workdir: %s\n", wd);
    printf("  Next run: %s\n", cj_str(created, "next_run_at", "?"));
    json_free(created);
    cj_warn_if_gateway_not_running();
    json_free(a);
    return 0;
}

/* PoP: hermes_cli_cron_cron_edit @ hermes_cli/cron.py:cron_edit */
int hermes_cli_cron_cron_edit(const char *args_json) {
    json_t *a = args_json ? json_parse(args_json, NULL) : NULL;
    if (!a || a->type != JSON_OBJECT) {
        if (a) json_free(a);
        printf("\033[31mFailed to update job: invalid arguments\033[0m\n");
        return 1;
    }
    const char *ref = cj_str(a, "job_id", "");
    bool ambiguous = false;
    json_t *job = cronjobs_resolve_job_ref(ref, &ambiguous);
    if (ambiguous) {
        printf("\033[31mAmbiguous job reference: %s\033[0m\n", ref);
        json_free(a);
        return 1;
    }
    if (!job) {
        printf("\033[31mJob not found: %s\033[0m\n", ref);
        json_free(a);
        return 1;
    }

    /* Skills merge: clear > replacement > add/remove set-ops (Python order). */
    json_t *final_skills = NULL;
    json_t *existing = json_obj_get(job, "skills");
    json_t *clear_sk = json_obj_get(a, "clear_skills");
    json_t *replacement = json_obj_get(a, "skills");
    json_t *add = json_obj_get(a, "add_skills");
    json_t *rem = json_obj_get(a, "remove_skills");
    if (clear_sk && clear_sk->type == JSON_BOOL && clear_sk->num_val) {
        final_skills = json_array();
    } else if (replacement && replacement->type == JSON_ARRAY) {
        final_skills = json_copy(replacement);
    } else if ((add && add->type == JSON_ARRAY && json_len(add)) ||
               (rem && rem->type == JSON_ARRAY && json_len(rem))) {
        final_skills = json_array();
        if (existing && existing->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(existing); i++) {
                json_t *it = json_get(existing, i);
                if (!it || it->type != JSON_STRING) continue;
                bool dropped = false;
                if (rem && rem->type == JSON_ARRAY) {
                    for (size_t r = 0; r < json_len(rem); r++) {
                        json_t *rv = json_get(rem, r);
                        if (rv && rv->type == JSON_STRING &&
                            strcmp(rv->str_val, it->str_val) == 0) { dropped = true; break; }
                    }
                }
                if (!dropped) json_append(final_skills, json_string(it->str_val));
            }
        }
        if (add && add->type == JSON_ARRAY) {
            for (size_t i = 0; i < json_len(add); i++) {
                json_t *it = json_get(add, i);
                if (!it || it->type != JSON_STRING) continue;
                bool have = false;
                for (size_t f = 0; f < json_len(final_skills); f++) {
                    json_t *fv = json_get(final_skills, f);
                    if (fv && fv->type == JSON_STRING &&
                        strcmp(fv->str_val, it->str_val) == 0) { have = true; break; }
                }
                if (!have) json_append(final_skills, json_string(it->str_val));
            }
        }
    }

    json_t *upd = json_object();
    static const char *fields[] = {"schedule", "prompt", "name", "deliver", "repeat",
                                   "script", "workdir", "model", "provider",
                                   "monitor_script", "monitor_url"};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); i++) {
        json_t *v = json_obj_get(a, fields[i]);
        if (v) json_set(upd, fields[i], json_copy(v));
    }
    json_t *mp = json_obj_get(a, "model_provider");
    if (mp && mp->type == JSON_STRING) json_set(upd, "provider", json_string(mp->str_val));
    json_t *na = json_obj_get(a, "no_agent");
    if (na && na->type == JSON_BOOL) json_set(upd, "no_agent", json_bool(na->num_val != 0));
    json_t *cont = json_obj_get(a, "continuity");
    if (cont && cont->type == JSON_BOOL) json_set(upd, "continuity", json_bool(cont->num_val != 0));
    json_t *reff = json_obj_get(a, "reasoning_effort");
    if (reff && reff->type == JSON_STRING) json_set(upd, "reasoning_effort", json_string(reff->str_val));
    if (final_skills) {
        if (json_len(final_skills) == 0) json_set(upd, "skills", json_null());
        else json_set(upd, "skills", final_skills);
        json_free(final_skills);
    }

    char *err = NULL;
    json_t *updated = cronjobs_update_job(cj_str(job, "id", ref), upd, &err);
    json_free(upd);
    json_free(job);
    if (!updated) {
        printf("\033[31mFailed to update job: %s\033[0m\n", err ? err : "unknown error");
        free(err);
        json_free(a);
        return 1;
    }
    printf("\033[32mUpdated job: %s\033[0m\n", cj_str(updated, "id", "?"));
    printf("  Name: %s\n", cj_str(updated, "name", "(unnamed)"));
    printf("  Schedule: %s\n", cj_str(updated, "schedule_display",
                                       cj_str(updated, "schedule", "?")));
    json_t *sk = json_obj_get(updated, "skills");
    if (sk && sk->type == JSON_ARRAY && json_len(sk) > 0) {
        printf("  Skills: ");
        for (size_t i = 0; i < json_len(sk); i++) {
            json_t *it = json_get(sk, i);
            if (it && it->type == JSON_STRING) printf("%s%s", i ? ", " : "", it->str_val);
        }
        printf("\n");
    } else {
        printf("  Skills: none\n");
    }
    const char *script = cj_str(updated, "script", NULL);
    if (script) printf("  Script: %s\n", script);
    const char *mons = cj_str(updated, "monitor_script", NULL);
    if (mons) printf("  Monitor: %s (agent runs only on output change)\n", mons);
    const char *monu = cj_str(updated, "monitor_url", NULL);
    if (monu) printf("  Monitor: %s (agent runs only on output change)\n", monu);
    json_t *nao = json_obj_get(updated, "no_agent");
    if (nao && ((nao->type == JSON_BOOL && nao->num_val) || nao->type == JSON_NUMBER))
        printf("  Mode: no-agent (script stdout delivered directly)\n");
    json_t *co = json_obj_get(updated, "continuity");
    if (co && ((co->type == JSON_BOOL && co->num_val) || co->type == JSON_STRING))
        printf("  Continuity: on (each run sees the previous run's output)\n");
    const char *wd = cj_str(updated, "workdir", NULL);
    if (wd) printf("  Workdir: %s\n", wd);
    json_free(updated);
    json_free(a);
    return 0;
}

/* PoP: hermes_cli_cron_cron_resume @ hermes_cli/cron.py:cron_resume */
int hermes_cli_cron_cron_resume(const char *job_id, const char *run_at, int run_now) {
    if (!!run_at == !!run_now) {
        if (run_at || run_now) {
            printf("\033[31mUse exactly one of --at or --run-now.\033[0m\n");
            return 1;
        }
        json_t *job = cronjobs_resume_job(job_id);
        if (!job) {
            printf("\033[31mFailed to resume job: not found\033[0m\n");
            return 1;
        }
        printf("\033[32mResumed job: %s (%s)\033[0m\n", cj_str(job, "name", job_id), job_id);
        printf("  Next run: %s\n", cj_str(job, "next_run_at", "?"));
        json_free(job);
        return 0;
    }
    /* Re-arm a completed one-shot at an explicit time (or now). */
    char iso[64];
    if (run_now) {
        time_t t = time(NULL);
        struct tm tmv;
        localtime_r(&t, &tmv);
        strftime(iso, sizeof(iso), "%Y-%m-%dT%H:%M:%S", &tmv);
        run_at = iso;
    }
    char *err = NULL;
    json_t *job = cronjobs_rearm_oneshot(job_id, run_at, &err);
    if (!job) {
        printf("\033[31mFailed to re-arm job: %s\033[0m\n", err ? err : "not found");
        free(err);
        return 1;
    }
    printf("\033[32mRe-armed job: %s (%s)\033[0m\n", cj_str(job, "name", job_id), job_id);
    printf("  Next run: %s\n", cj_str(job, "next_run_at", "?"));
    json_free(job);
    return 0;
}
