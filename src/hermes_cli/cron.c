/*
 * cron.c — canonical C11 port of hermes_cli/cron.py.
 */

#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cron_notepad.h"


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
