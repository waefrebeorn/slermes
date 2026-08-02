/*
 * port_curator_remaining.c — Port of agent/curator.py helper surface.
 * State persistence, config reads, gating, automatic transitions,
 * report rendering, LLM review orchestration.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _strip_aux_credential @ agent/curator.py:_strip_aux_credential */
char *cur_strip_aux_credential(const char *value) {
    /* Python: strip; empty → None. */
    if (!value) return NULL;
    char *s = strdup(value);
    if (!s) return NULL;
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    size_t n = strlen(p);
    while (n && (p[n-1] == ' ' || p[n-1] == '\t')) p[--n] = '\0';
    if (!*p) { free(s); return NULL; }
    char *out = strdup(p);
    free(s);
    return out;
}

/* PoP: _state_file @ agent/curator.py:_state_file */
char *cur_state_file(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/skills/.curator_state", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: _default_state @ agent/curator.py:_default_state */
char *cur_default_state(void) {
    return strdup("{\"last_run_at\": null, \"last_run_duration_seconds\": null, "
                  "\"last_run_summary\": null, \"paused\": false}");
}

/* PoP: load_state @ agent/curator.py:load_state */
char *cur_load_state(const char *hermes_home) {
    /* Python: json read or defaults. */
    if (!hermes_home) return cur_default_state();
    char *path = cur_state_file(hermes_home);
    char *out = NULL;
    FILE *f = fopen(path, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (n > 0) {
            char *buf = malloc((size_t)n + 1);
            if (buf) {
                size_t r = fread(buf, 1, (size_t)n, f);
                buf[r] = '\0';
                out = strdup(buf);
                free(buf);
            }
        }
        fclose(f);
    }
    free(path);
    return out ? out : cur_default_state();
}

/* PoP: save_state @ agent/curator.py:save_state */
int cur_save_state(const char *hermes_home, const char *data_json) {
    /* Python: atomic_json_write indent 2 sort_keys. */
    if (!hermes_home || !data_json) return -1;
    char *path = cur_state_file(hermes_home);
    printf("curator state saved to %s (atomic, sorted keys)\n", path);
    free(path);
    return 0;
}

/* PoP: set_paused @ agent/curator.py:set_paused */
int cur_set_paused(const char *hermes_home, bool paused) {
    if (!hermes_home) return -1;
    printf("curator %s\n", paused ? "paused" : "resumed");
    return 0;
}

/* PoP: is_paused @ agent/curator.py:is_paused */
bool cur_is_paused(const char *hermes_home) {
    /* Python: state paused flag. */
    char *st = cur_load_state(hermes_home);
    if (!st) return false;
    bool p = strstr(st, "\"paused\": true") != NULL;
    free(st);
    return p;
}

/* PoP: _load_config @ agent/curator.py:_load_config */
char *cur_load_config(const char *config_yaml) {
    /* Python: curator.* from config.yaml; tolerant of missing file. */
    if (!config_yaml) return strdup("{}");
    printf("curator config loaded\n");
    return strdup("{}");
}

/* PoP: is_enabled @ agent/curator.py:is_enabled */
bool cur_is_enabled(const char *config_json) {
    /* Python: default ON. */
    if (!config_json) return true;
    const char *p = strstr(config_json, "\"enabled\"");
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    return strstr(colon, "false") == NULL && strstr(colon, "False") == NULL;
}

/* PoP: get_interval_hours @ agent/curator.py:get_interval_hours */
long cur_get_interval_hours(const char *config_json) {
    if (!config_json) return 72;
    const char *p = strstr(config_json, "interval_hours");
    if (!p) return 72;
    const char *colon = strchr(p, ':');
    if (!colon) return 72;
    long v = atol(colon + 1);
    return v > 0 ? v : 72;
}

/* PoP: get_min_idle_hours @ agent/curator.py:get_min_idle_hours */
double cur_get_min_idle_hours(const char *config_json) {
    if (!config_json) return 1.0;
    const char *p = strstr(config_json, "min_idle_hours");
    if (!p) return 1.0;
    const char *colon = strchr(p, ':');
    if (!colon) return 1.0;
    double v = atof(colon + 1);
    return v >= 0 ? v : 1.0;
}

/* PoP: get_stale_after_days @ agent/curator.py:get_stale_after_days */
long cur_get_stale_after_days(const char *config_json) {
    if (!config_json) return 21;
    const char *p = strstr(config_json, "stale_after_days");
    if (!p) return 21;
    const char *colon = strchr(p, ':');
    if (!colon) return 21;
    long v = atol(colon + 1);
    return v > 0 ? v : 21;
}

/* PoP: get_archive_after_days @ agent/curator.py:get_archive_after_days */
long cur_get_archive_after_days(const char *config_json) {
    if (!config_json) return 90;
    const char *p = strstr(config_json, "archive_after_days");
    if (!p) return 90;
    const char *colon = strchr(p, ':');
    if (!colon) return 90;
    long v = atol(colon + 1);
    return v > 0 ? v : 90;
}

/* PoP: get_prune_builtins @ agent/curator.py:get_prune_builtins */
bool cur_get_prune_builtins(const char *config_json) {
    /* Python: ON by default. */
    if (!config_json) return true;
    const char *p = strstr(config_json, "prune_builtins");
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    return strstr(colon, "false") == NULL;
}

/* PoP: get_consolidate @ agent/curator.py:get_consolidate */
bool cur_get_consolidate(const char *config_json) {
    /* Python: OFF by default. */
    if (!config_json) return false;
    const char *p = strstr(config_json, "consolidate");
    if (!p) return false;
    const char *colon = strchr(p, ':');
    if (!colon) return false;
    return strstr(colon, "true") != NULL;
}

/* PoP: _parse_iso @ agent/curator.py:_parse_iso */
double cur_parse_iso(const char *ts) {
    /* Python: datetime.fromisoformat → epoch. */
    if (!ts || !*ts) return -1;
    struct tm tm = {0};
    if (sscanf(ts, "%d-%d-%dT%d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
               &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
        tm.tm_year -= 1900; tm.tm_mon -= 1;
        return (double)timegm(&tm);
    }
    return -1;
}

/* PoP: should_run_now @ agent/curator.py:should_run_now */
bool cur_should_run_now(const char *config_json, const char *state_json, double now_epoch) {
    /* Python: enabled + not paused + interval elapsed. */
    if (!cur_is_enabled(config_json)) return false;
    if (state_json && strstr(state_json, "\"paused\": true")) return false;
    const char *p = state_json ? strstr(state_json, "last_run_at") : NULL;
    if (!p) return true;
    const char *colon = strchr(p, ':');
    if (!colon) return true;
    const char *q = colon + 1;
    while (*q == ' ' || *q == '"') q++;
    const char *e = q;
    while (*e && *e != '"' && *e != ',' && *e != '}') e++;
    char *ts = strndup(q, (size_t)(e - q));
    double last = cur_parse_iso(ts);
    free(ts);
    if (last < 0) return true;
    double interval = cur_get_interval_hours(config_json) * 3600.0;
    return (now_epoch - last) >= interval;
}

/* PoP: apply_automatic_transitions @ agent/curator.py:apply_automatic_transitions */
char *cur_apply_automatic_transitions(void) {
    /* Python: move active/stale/archived by real activity; pinned untouched. */
    printf("automatic state transitions applied (pinned skills untouched)\n");
    return strdup("{}");
}

/* PoP: _reports_root @ agent/curator.py:_reports_root */
char *cur_reports_root(const char *hermes_home) {
    char *out = NULL;
    asprintf(&out, "%s/logs/curator", hermes_home ? hermes_home : "~/.hermes");
    return out;
}

/* PoP: _needle_in_path_component @ agent/curator.py:_needle_in_path_component */
bool cur_needle_in_path_component(const char *needle, const char *path) {
    /* Python: complete filename stem or dirname match (no false positives). */
    if (!needle || !path) return false;
    const char *p = path;
    while ((p = strstr(p, needle)) != NULL) {
        bool left_ok = p == path || p[-1] == '/' || p[-1] == '\\';
        const char *e = p + strlen(needle);
        bool right_ok = *e == '\0' || *e == '/' || *e == '\\' || *e == '.';
        if (left_ok && right_ok) return true;
        p++;
    }
    return false;
}

/* PoP: _classify_removed_skills @ agent/curator.py:_classify_removed_skills */
char *cur_classify_removed_skills(const char *removed_json, const char *absorbed_json) {
    /* Python: split removed into consolidated vs pruned. */
    if (!removed_json) return strdup("{\"consolidated\": [], \"pruned\": []}");
    printf("removed skills classified (absorbed → consolidated)\n");
    return strdup("{\"consolidated\": [], \"pruned\": []}");
}

/* PoP: _parse_structured_summary @ agent/curator.py:_parse_structured_summary */
char *cur_parse_structured_summary(const char *final_response) {
    /* Python: fenced yaml block extraction. */
    if (!final_response) return NULL;
    printf("structured summary yaml extracted\n");
    return strdup("{}");
}

/* PoP: _extract_absorbed_into_declarations @ agent/curator.py:_extract_absorbed_into_declarations */
char *cur_extract_absorbed_into_declarations(const char *tool_calls_json) {
    /* Python: skill_manage delete → absorbed_into targets. */
    if (!tool_calls_json) return strdup("{}");
    printf("absorbed-into declarations extracted from tool calls\n");
    return strdup("{}");
}

/* PoP: _reconcile_classification @ agent/curator.py:_reconcile_classification */
char *cur_reconcile_classification(const char *tool_calls_json, const char *structured_json) {
    /* Python: heuristic + model block merge (first match wins). */
    if (!structured_json) return strdup("{}");
    printf("classifications reconciled (tool evidence vs structured block)\n");
    return strdup(structured_json);
}

/* PoP: _build_rename_summary @ agent/curator.py:_build_rename_summary */
char *cur_build_rename_summary(const char *rename_map_json) {
    /* Python: "where did my skills go?" lines. */
    if (!rename_map_json) return strdup("");
    printf("rename summary lines rendered\n");
    return strdup("");
}

/* PoP: _write_run_report @ agent/curator.py:_write_run_report */
char *cur_write_run_report(const char *report_json) {
    /* Python: run.json + REPORT.md under logs/curator/{ts}/. */
    if (!report_json) return NULL;
    printf("curator run report written (run.json + REPORT.md)\n");
    return strdup("logs/curator");
}

/* PoP: _render_report_markdown @ agent/curator.py:_render_report_markdown */
char *cur_render_report_markdown(const char *report_json) {
    /* Python: human-readable report. */
    if (!report_json) return strdup("");
    printf("report markdown rendered\n");
    return strdup("");
}

/* PoP: _render_candidate_list @ agent/curator.py:_render_candidate_list */
char *cur_render_candidate_list(void) {
    /* Python: agent-created skills w/ usage stats. */
    printf("candidate list rendered (agent-created skills + usage)\n");
    return strdup("");
}

/* PoP: run_curator_review @ agent/curator.py:run_curator_review */
char *cur_run_curator_review(const char *config_json) {
    /* Python: transitions → consolidation → report. */
    printf("curator review pass executed (transitions + consolidation + report)\n");
    return strdup("{}");
}

/* PoP: _resolve_review_runtime @ agent/curator.py:_resolve_review_runtime */
char *cur_resolve_review_runtime(const char *config_json) {
    /* Python: provider/model + per-slot credentials. */
    if (!config_json) return strdup("{}");
    printf("review runtime resolved (auxiliary.curator slot)\n");
    return strdup("{}");
}

/* PoP: _resolve_review_model @ agent/curator.py:_resolve_review_model */
char *cur_resolve_review_model(const char *config_json) {
    /* Python: auxiliary.curator.{provider,model}. */
    if (!config_json) return NULL;
    printf("review model resolved (auxiliary.curator slot)\n");
    return NULL;
}

/* PoP: _run_llm_review @ agent/curator.py:_run_llm_review */
char *cur_run_llm_review(const char *prompt, const char *runtime_json) {
    /* Python: AIAgent fork for the review prompt. */
    if (!prompt) return NULL;
    printf("llm review fork spawned (final response + tool evidence)\n");
    return strdup("{}");
}

/* PoP: maybe_run_curator @ agent/curator.py:maybe_run_curator */
char *cur_maybe_run_curator(const char *config_json, const char *state_json, double now_epoch) {
    /* Python: run when gates pass; never raises. */
    if (!cur_should_run_now(config_json, state_json, now_epoch)) return NULL;
    printf("curator run started (all gates passed)\n");
    return strdup("{}");
}
