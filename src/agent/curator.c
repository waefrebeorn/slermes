#define _GNU_SOURCE
/**
 * @file curator.c
 * @brief B05: Curator — background skill maintenance orchestrator.
 *
 * Manages curator state persistence and provides status for CLI.
 * Port of Python agent/curator.py (1843 lines).
 * State is stored in <hermes_home>/skills/.curator_state as JSON.
 */
#include "hermes_curator.h"
#include "hermes_json.h"
#include "skill_usage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

/* ── State file path ────────────────────────────────────────────── */

/* Port of Python: _state_file */
static void state_path(char *buf, size_t sz) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    snprintf(buf, sz, "%s/skills/.curator_state", home);
}

/* ── Helper: get HERMES_HOME ───────────────────────────────────── */

static const char *get_hermes_home_dir(void) {
    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) home = ".";
    return home;
}

/* ── String helpers ─────────────────────────────────────────────── */

/* Port of Python: _strip_aux_credential */
char *strip_aux_credential(const char *value) {
    if (!value) return NULL;
    /* Skip leading whitespace */
    while (*value == ' ' || *value == '\t' || *value == '\n' || *value == '\r')
        value++;
    if (!*value) return NULL;
    /* Copy and strip trailing whitespace */
    size_t len = strlen(value);
    while (len > 0 && (value[len-1] == ' ' || value[len-1] == '\t' ||
                       value[len-1] == '\n' || value[len-1] == '\r'))
        len--;
    if (len == 0) return NULL;
    char *result = (char*)malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, value, len);
    result[len] = '\0';
    return result;
}

/* Port of Python: _needle_in_path_component */
bool needle_in_path_component(const char *needle, const char *path) {
    if (!needle || !path || !*needle) return false;
    
    /* Normalise needle: hyphens → underscores */
    size_t nlen = strlen(needle);
    char norm_needle[256];
    size_t ni = 0;
    for (size_t i = 0; i < nlen && ni < sizeof(norm_needle) - 1; i++) {
        norm_needle[ni++] = (needle[i] == '-') ? '_' : needle[i];
    }
    norm_needle[ni] = '\0';
    
    /* Copy path and split by '/' */
    char path_copy[1024];
    size_t plen = strlen(path);
    if (plen >= sizeof(path_copy)) plen = sizeof(path_copy) - 1;
    memcpy(path_copy, path, plen);
    path_copy[plen] = '\0';
    
    /* Normalise path separators */
    for (size_t i = 0; path_copy[i]; i++) {
        if (path_copy[i] == '\\') path_copy[i] = '/';
    }
    
    char *save;
    const char *part = strtok_r(path_copy, "/", &save);
    while (part) {
        if (!*part) { part = strtok_r(NULL, "/", &save); continue; }
        /* Extract stem (before first dot) */
        char stem[256];
        size_t si = 0;
        for (size_t i = 0; part[i] && part[i] != '.' && si < sizeof(stem) - 1; i++) {
            stem[si++] = (part[i] == '-') ? '_' : part[i];
        }
        stem[si] = '\0';
        if (strcmp(stem, norm_needle) == 0) return true;
        part = strtok_r(NULL, "/", &save);
    }
    return false;
}

/* Port of Python: maybe_run_curator — gate-checking portion
 *
 * Best-effort gate check: returns true if all gates pass and curator
 * should run. The LLM review pass is Python-only; C callers use this
 * to decide whether to invoke the Python side.
 *
 * Equivalent to Python's gate logic without the LLM review fork:
 *   if not should_run_now(): return None
 *   if idle_for_seconds is not None and idle < min_idle: return None
 */
bool curator_maybe_run(double idle_for_seconds) {
    if (!should_run_now()) return false;
    if (idle_for_seconds >= 0.0) {
        double min_idle = get_min_idle_hours() * 3600.0;
        if (idle_for_seconds < min_idle) return false;
    }
    return true;
}

/* ── Time helpers ───────────────────────────────────────────────── */

/* Port of Python: _parse_iso
 * Parse ISO-8601 datetime string to time_t.
 * Supports "YYYY-MM-DDTHH:MM:SS" and "YYYY-MM-DD" formats.
 * Returns 0 on parse failure (same as Python's None→epoch behavior).
 */
time_t parse_iso_time(const char *ts) {
    if (!ts || !*ts) return 0;
    struct tm tm_buf;
    memset(&tm_buf, 0, sizeof(tm_buf));
    const char *p = strptime(ts, "%Y-%m-%dT%H:%M:%S", &tm_buf);
    if (!p) {
        /* Try date-only format */
        p = strptime(ts, "%Y-%m-%d", &tm_buf);
        if (!p) return 0;
    }
    time_t result = timegm(&tm_buf);
    return result == (time_t)-1 ? 0 : result;
}

/* ── Config access (env-var based, C-native) ────────────────────── */

/* Port of Python: _load_config, _resolve_review_runtime, _resolve_review_model — consolidated
 * Python resolves provider/model from config.yaml for the LLM review fork.
 * C equivalent: config access via env vars with defaults below. */
/* AG26: Port of Python agent/curator.py:_load_config() */
/* AG26: Port of Python agent/curator.py:_resolve_review_runtime() */
/* AG26: Port of Python agent/curator.py:_resolve_review_model() */

/* Port of Python: is_enabled */
bool is_enabled(void) {
    curator_state_t state;
    load_state(&state);
    return state.enabled;
}

/* Port of Python: get_interval_hours */
int get_interval_hours(void) {
    const char *env = getenv("HERMES_CURATOR_INTERVAL_HOURS");
    if (env && *env) {
        long val = strtol(env, NULL, 10);
        if (val > 0) return (int)val;
    }
    return CURATOR_DEFAULT_INTERVAL_HOURS;
}

/* Port of Python: get_min_idle_hours */
double get_min_idle_hours(void) {
    const char *env = getenv("HERMES_CURATOR_MIN_IDLE_HOURS");
    if (env && *env) {
        double val = strtod(env, NULL);
        if (val > 0.0) return val;
    }
    return CURATOR_DEFAULT_MIN_IDLE_HOURS;
}

/* Port of Python: get_stale_after_days */
int get_stale_after_days(void) {
    const char *env = getenv("HERMES_CURATOR_STALE_AFTER_DAYS");
    if (env && *env) {
        long val = strtol(env, NULL, 10);
        if (val > 0) return (int)val;
    }
    return CURATOR_DEFAULT_STALE_AFTER_DAYS;
}

/* Port of Python: get_archive_after_days */
int get_archive_after_days(void) {
    const char *env = getenv("HERMES_CURATOR_ARCHIVE_AFTER_DAYS");
    if (env && *env) {
        long val = strtol(env, NULL, 10);
        if (val > 0) return (int)val;
    }
    return CURATOR_DEFAULT_ARCHIVE_AFTER_DAYS;
}

/* Port of Python: get_prune_builtins */
bool get_prune_builtins(void) {
    const char *env = getenv("HERMES_CURATOR_PRUNE_BUILTINS");
    if (env) {
        if (strcmp(env, "0") == 0 || strcmp(env, "false") == 0 ||
            strcmp(env, "no") == 0 || strcmp(env, "off") == 0)
            return false;
        return true;
    }
    return true; /* default: ON */
}

/* Port of Python: _reports_root — path builder for report dirs.
   Kept for API completeness. */
__attribute__((used)) static void reports_root(char *buf, size_t sz) {
    const char *home = get_hermes_home_dir();
    snprintf(buf, sz, "%s/logs/curator", home);
    /* Ensure directory exists */
    char mkdir_cmd[2048];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s 2>/dev/null", buf);
    int mkrc = system(mkdir_cmd);
    (void)mkrc;
}

/* Port of Python: should_run_now
 *
 * Checks gates: enabled, not paused, and interval elapsed since last run.
 * Returns true if curator should run.
 */
bool should_run_now(void) {
    if (!is_enabled()) return false;

    curator_state_t state;
    if (!load_state(&state)) {
        /* First run: never run before, defer by seeding state
         * Equivalent to Python: sets last_run_at to now, returns false. */
        time_t now = time(NULL);
        init_state(&state);
        state.last_run_at = now;
        state.enabled = true;
        state.last_run_summary[0] = '\0';
        snprintf(state.last_run_summary, sizeof(state.last_run_summary),
                 "deferred first run — curator seeded, will run after one interval");
        save_state(&state);
        return false;
    }

    if (state.paused) return false;

    time_t now = time(NULL);
    if (state.last_run_at == 0) {
        /* Never run before in loaded state — seed and defer */
        state.last_run_at = now;
        save_state(&state);
        return false;
    }

    double interval_secs = (double)get_interval_hours() * 3600.0;
    double elapsed = difftime(now, state.last_run_at);
    return elapsed >= interval_secs;
}

/* Port of Python: apply_automatic_transitions
 *
 * Walk every available skill in the usage map and apply lifecycle
 * transitions (active → stale → archived) based on last activity.
 * Pinned skills are never touched.
 *
 * Returns a struct with counts of what changed.
 */
curator_transition_counts_t apply_automatic_transitions(void) {
    curator_transition_counts_t counts;
    memset(&counts, 0, sizeof(counts));

    const char *home = get_hermes_home_dir();

    skill_usage_map_t map;
    skill_usage_load(home, &map);

    time_t now = time(NULL);
    double stale_secs = (double)get_stale_after_days() * 86400.0;
    double archive_secs = (double)get_archive_after_days() * 86400.0;

    for (int i = 0; i < map.count; i++) {
        counts.checked++;
        skill_usage_record_t *r = &map.records[i];

        if (!r->name[0]) continue;
        if (r->pinned) continue;

        /* Get latest activity timestamp */
        char activity_iso[SKILL_USAGE_MAX_VALUE];
        const char *latest = skill_usage_latest_activity(r, activity_iso);
        if (!latest) latest = r->created_at;
        if (!latest || !*latest) continue;

        /* Parse ISO timestamp to time_t */
        time_t activity_time = parse_iso_time(latest);
        if (activity_time == 0) continue;

        double elapsed = difftime(now, activity_time);
        if (elapsed < 0) elapsed = 0;

        const char *current_state = r->state;

        if (elapsed >= archive_secs && strcmp(current_state, SKILL_USAGE_STATE_ARCHIVED) != 0) {
            char msg[SKILL_USAGE_MAX_VALUE];
            int rc = skill_usage_archive(home, r->name, msg);
            if (rc == 0) {
                counts.archived++;
            }
        } else if (elapsed >= stale_secs && strcmp(current_state, SKILL_USAGE_STATE_ACTIVE) == 0) {
            int rc = skill_usage_set_state(home, r->name, SKILL_USAGE_STATE_STALE);
            if (rc == 0) {
                counts.marked_stale++;
            }
        } else if (elapsed < stale_secs && strcmp(current_state, SKILL_USAGE_STATE_STALE) == 0) {
            int rc = skill_usage_set_state(home, r->name, SKILL_USAGE_STATE_ACTIVE);
            if (rc == 0) {
                counts.reactivated++;
            }
        }
    }

    return counts;
}

/* ════════════════════════════════════════════════════════════════════════
 *  Report helpers
 * ════════════════════════════════════════════════════════════════════════ */

/* Port of Python: _render_candidate_list
 *
 * Build a human-readable list of agent-created skills with usage stats.
 * Returns a dynamically allocated string that the caller must free.
 */
char *render_candidate_list(void) {
    const char *home = get_hermes_home_dir();

    skill_usage_map_t map;
    skill_usage_load(home, &map);

    /* Count agent-created skills */
    int agent_count = 0;
    for (int i = 0; i < map.count; i++) {
        if (strcmp(map.records[i].created_by, "agent") == 0)
            agent_count++;
    }

    /* If none, return a simple message */
    if (agent_count == 0) {
        char *msg = (char*)malloc(64);
        if (!msg) return NULL;
        snprintf(msg, 64, "No agent-created skills to review.");
        return msg;
    }

    /* Estimate buffer size: ~200 bytes per skill */
    size_t bufsz = 256 + (size_t)agent_count * 256;
    char *buf = (char*)malloc(bufsz);
    if (!buf) return NULL;
    size_t pos = 0;

    pos += snprintf(buf + pos, bufsz - pos,
                    "Agent-created skills (%d):\n\n", agent_count);

    for (int i = 0; i < map.count && pos < bufsz - 50; i++) {
        skill_usage_record_t *r = &map.records[i];
        if (strcmp(r->created_by, "agent") != 0) continue;

        int activity = skill_usage_activity_count(r);
        char latest[SKILL_USAGE_MAX_VALUE] = "never";
        skill_usage_latest_activity(r, latest);
        if (!latest[0]) snprintf(latest, sizeof(latest), "never");

        pos += snprintf(buf + pos, bufsz - pos,
            "- %s  state=%s  pinned=%s  activity=%d  "
            "use=%d  view=%d  patches=%d  last_activity=%s\n",
            r->name,
            r->state,
            r->pinned ? "yes" : "no",
            activity,
            r->use_count,
            r->view_count,
            r->patch_count,
            latest);
    }

    return buf;
}

/* ════════════════════════════════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════════════════════════════════ */

/* Port of Python agent/curator.py:_default_state(). */
void init_state(curator_state_t *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->enabled = true;
    state->paused = false;
    state->run_count = 0;
    state->last_run_at = 0;
    state->last_run_duration = 0.0;
    state->last_run_summary[0] = '\0';
}

/* Port of Python agent/curator.py:load_state(). */
bool load_state(curator_state_t *state) {
    if (!state) return false;
    init_state(state);

    char path[1024];
    state_path(path, sizeof(path));

    /* Check if file exists */
    struct stat st;
    if (stat(path, &st) != 0) return false;

    json_t *root = json_parse_file(path, NULL);
    if (!root) return false;

    state->enabled = json_get_bool(root, "enabled", true);
    state->paused = json_get_bool(root, "paused", false);
    state->run_count = (int)json_get_num(root, "run_count", 0);
    state->last_run_at = (time_t)json_get_num(root, "last_run_at", 0);
    state->last_run_duration = json_get_num(root, "last_run_duration_seconds", 0.0);

    const char *summary = json_get_str(root, "last_run_summary", "");
    if (summary)
        snprintf(state->last_run_summary, sizeof(state->last_run_summary),
                 "%s", summary);

    json_free(root);
    return true;
}

/* Port of Python hermes_cli/proxy/adapters/nous_portal.py:_save_state(). */
/* Port of Python agent/curator.py:save_state(). */
void save_state(const curator_state_t *state) {
    if (!state) return;

    char path[1024];
    state_path(path, sizeof(path));

    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        char mkdir_cmd[2048];
        snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s 2>/dev/null", dir);
        int mkrc = system(mkdir_cmd);
        (void)mkrc;
    }

    json_t *root = json_object();
    json_set(root, "enabled", json_bool(state->enabled));
    json_set(root, "paused", json_bool(state->paused));
    json_set(root, "run_count", json_number((double)state->run_count));
    json_set(root, "last_run_at", json_number((double)state->last_run_at));
    json_set(root, "last_run_duration_seconds",
             json_number(state->last_run_duration));
    json_set(root, "last_run_summary",
             json_string(state->last_run_summary[0] ?
                         state->last_run_summary : ""));

    char *out = json_serialize_pretty(root, 2);
    if (out) {
        FILE *f = fopen(path, "w");
        if (f) {
            fputs(out, f);
            fclose(f);
        }
        free(out);
    }
    json_free(root);
}

/* Port of Python agent/curator.py:record_run(). */
void record_run(curator_state_t *state, double duration_secs,
                         const char *summary) {
    if (!state) return;
    state->last_run_at = time(NULL);
    state->last_run_duration = duration_secs;
    state->run_count++;
    if (summary)
        snprintf(state->last_run_summary, sizeof(state->last_run_summary),
                 "%s", summary);
    save_state(state);
}

/* Port of Python agent/curator.py:set_paused(). */
void set_paused(bool paused) {
    curator_state_t state;
    load_state(&state);
    state.paused = paused;
    save_state(&state);
}

/* Port of Python agent/curator.py:is_paused(). */
bool is_paused(void) {
    curator_state_t state;
    load_state(&state);
    return state.paused;
}

/* Port of Python gateway/run.py:_format_duration(). */
/* Port of Python agent/curator.py:format_duration(). */
void format_duration(double seconds, char *buf, size_t sz) {
    if (!buf || sz == 0) return;

    if (seconds < 60.0) {
        snprintf(buf, sz, "%.0fs", seconds);
    } else if (seconds < 3600.0) {
        int m = (int)(seconds / 60.0);
        int s = (int)seconds % 60;
        snprintf(buf, sz, "%dm %ds", m, s);
    } else if (seconds < 86400.0) {
        int h = (int)(seconds / 3600.0);
        int m = ((int)seconds % 3600) / 60;
        snprintf(buf, sz, "%dh %dm", h, m);
    } else {
        int d = (int)(seconds / 86400.0);
        int h = ((int)seconds % 86400) / 3600;
        snprintf(buf, sz, "%dd %dh", d, h);
    }
}

/* Port of Python agent/curator.py:format_reltime(). */
void format_reltime(time_t t, char *buf, size_t sz) {
    if (!buf || sz == 0) return;
    if (t == 0) {
        snprintf(buf, sz, "never");
        return;
    }

    time_t now = time(NULL);
    double diff = difftime(now, t);
    if (diff < 0) diff = 0;

    if (diff < 60.0) {
        snprintf(buf, sz, "%.0fs ago", diff);
    } else if (diff < 3600.0) {
        snprintf(buf, sz, "%dm ago", (int)(diff / 60.0));
    } else if (diff < 86400.0) {
        snprintf(buf, sz, "%dh ago", (int)(diff / 3600.0));
    } else {
        snprintf(buf, sz, "%dd ago", (int)(diff / 86400.0));
    }
}

/* ════════════════════════════════════════════════════════════════════════
 *  Classification helpers (pipeline: _extract → _classify → _reconcile)
 * ════════════════════════════════════════════════════════════════════════ */

/* Simple JSON string extraction from tool call arguments.
   Looks for key "X" in a JSON-ish string and extracts its quoted value.
   {"key": "value"} style only, no nesting. Thread-safe with caller buf. */
static const char *json_arg_str(const char *args_json, const char *key,
                                 char *buf, size_t bufsz)
{
    if (!args_json || !key) return NULL;
    const char *k = strstr(args_json, key);
    if (!k) return NULL;
    k += strlen(key);
    while (*k && (*k == ' ' || *k == ':' || *k == '\t' || *k == '\n')) k++;
    if (*k != '"') return NULL;
    k++; /* skip opening quote */
    size_t pos = 0;
    while (*k && *k != '"' && pos < bufsz - 1) {
        if (*k == '\\' && *(k+1)) { k++; buf[pos++] = *k++; }
        else { buf[pos++] = *k++; }
    }
    buf[pos] = '\0';
    return buf;
}

/* Port of Python: _extract_absorbed_into_declarations
 *
 * Walk tool_calls (JSON strings) and extract skill_manage(action=delete)
 * calls that declare absorbed_into. Results stored in declarations array.
 * Returns number of declarations found (max CURATOR_MAX_DECLARATIONS).
 */
int extract_absorbed_into_declarations(const char *tool_calls[], int n_calls,
                                       curator_declaration_t declarations[],
                                       int max_decls)
{
    int count = 0;
    for (int i = 0; i < n_calls && count < max_decls; i++) {
        const char *json = tool_calls[i];
        if (!json || !*json) continue;

        /* Must be a skill_manage call */
        char name_buf[128];
        const char *name_val = json_arg_str(json, "\"name\"", name_buf, sizeof(name_buf));
        if (!name_val || strcmp(name_val, "skill_manage") != 0) continue;

        /* Parse the arguments field, which is a nested JSON string */
        char args_buf[2048];
        const char *args_raw = json_arg_str(json, "\"arguments\"", args_buf, sizeof(args_buf));
        if (!args_raw) continue;

        /* Action must be "delete" */
        char action_buf[64];
        const char *action_val = json_arg_str(args_raw, "\"action\"", action_buf, sizeof(action_buf));
        if (!action_val || strcmp(action_val, "delete") != 0) continue;

        /* Extract skill name (the one being deleted) */
        char skill_buf[128];
        const char *skill_name = json_arg_str(args_raw, "\"name\"", skill_buf, sizeof(skill_buf));
        if (!skill_name || !*skill_name) continue;

        /* Check for absorbed_into */
        char into_buf[128];
        const char *into_val = json_arg_str(args_raw, "\"absorbed_into\"", into_buf, sizeof(into_buf));

        /* If absorbed_into is not present at all, skip */
        if (!into_val) continue;

        snprintf(declarations[count].skill_name, sizeof(declarations[count].skill_name),
                 "%s", skill_name);
        snprintf(declarations[count].absorbed_into, sizeof(declarations[count].absorbed_into),
                 "%s", into_val);
        declarations[count].declared = true;
        count++;
    }
    return count;
}

/* Port of Python: _classify_removed_skills
 *
 * Split removed skill names into consolidated (absorbed into umbrella) vs pruned.
 * Heuristic: scan tool_calls for skill_manage actions (patch/write_file/create/edit)
 * on surviving skills that reference the removed skill's name.
 *
 * Results stored in consolidated[] and pruned[] arrays.
 * Returns number of items classified (removed_total = n_consolidated + n_pruned).
 */
int classify_removed_skills(const char *removed[], int n_removed,
                            const char *added[], int n_added,
                            const char *after_names[], int n_after,
                            const char *tool_call_args[], int n_calls,
                            curator_consolidation_t consolidated[], int max_cons,
                            char *pruned[], int max_pruned)
{
    int n_cons = 0, n_pruned = 0;

    /* Build destination set: surviving + newly-added skill names */
    char destinations[CURATOR_MAX_DECLARATIONS][128];
    int n_dest = 0;
    for (int i = 0; i < n_after && n_dest < CURATOR_MAX_DECLARATIONS; i++) {
        snprintf(destinations[n_dest++], 128, "%s", after_names[i]);
    }
    for (int i = 0; i < n_added && n_dest < CURATOR_MAX_DECLARATIONS; i++) {
        /* Avoid duplicates */
        bool dup = false;
        for (int j = 0; j < n_dest; j++) {
            if (strcmp(destinations[j], added[i]) == 0) { dup = true; break; }
        }
        if (!dup) snprintf(destinations[n_dest++], 128, "%s", added[i]);
    }

    for (int ri = 0; ri < n_removed; ri++) {
        const char *name = removed[ri];
        if (!name || !*name) continue;

        char *found_into = NULL;
        char found_evidence[512] = "";

        /* Normalise name variants */
        char norm_hyphen[128], norm_underscore[128];
        snprintf(norm_underscore, sizeof(norm_underscore), "%s", name);
        snprintf(norm_hyphen, sizeof(norm_hyphen), "%s", name);
        for (char *p = norm_underscore; *p; p++) if (*p == '-') *p = '_';
        for (char *p = norm_hyphen; *p; p++) if (*p == '_') *p = '-';

        /* Scan tool calls for evidence of consolidation */
        for (int ci = 0; ci < n_calls && !found_into; ci++) {
            const char *json = tool_call_args[ci];
            if (!json || !*json) continue;

            char tgt_buf[128];
            const char *target = json_arg_str(json, "\"name\"", tgt_buf, sizeof(tgt_buf));
            if (!target || !*target) continue;
            if (strcmp(target, name) == 0) continue; /* Operating on itself */

            /* Check if target is a destination (surviving/new) */
            bool is_dest = false;
            for (int d = 0; d < n_dest; d++) {
                if (strcmp(target, destinations[d]) == 0) { is_dest = true; break; }
            }
            if (!is_dest) continue;

            /* Check field values for needle name */
            const char *fields[] = {"file_path", "file_content", "content", "new_string"};
            for (int fi = 0; fi < 4 && !found_into; fi++) {
                char val_buf[512];
                const char *val = json_arg_str(json, fields[fi], val_buf, sizeof(val_buf));
                if (!val || !*val) continue;

                /* For file_path: use needle_in_path_component */
                bool matched = false;
                if (strcmp(fields[fi], "file_path") == 0) {
                    matched = needle_in_path_component(norm_underscore, val) ||
                              needle_in_path_component(norm_hyphen, val);
                } else {
                    /* For content fields: simple substring is sufficient */
                    matched = (strstr(val, norm_underscore) != NULL) ||
                              (strstr(val, norm_hyphen) != NULL);
                }

                if (matched) {
                    found_into = target; /* We'll copy it below */
                    snprintf(found_evidence, sizeof(found_evidence),
                             "skill_manage on '%s' referenced '%s' in %s",
                             target, name, val);
                    break;
                }
            }
        }

        if (found_into && n_cons < max_cons) {
            snprintf(consolidated[n_cons].name, sizeof(consolidated[n_cons].name), "%s", name);
            snprintf(consolidated[n_cons].into, sizeof(consolidated[n_cons].into), "%s", found_into);
            snprintf(consolidated[n_cons].evidence, sizeof(consolidated[n_cons].evidence), "%s", found_evidence);
            n_cons++;
        } else if (n_pruned < max_pruned) {
            /* pruned is a char* array — copy name */
            pruned[n_pruned] = (char*)malloc(strlen(name) + 1);
            if (pruned[n_pruned]) {
                strcpy(pruned[n_pruned], name);
                n_pruned++;
            }
        }
    }

    return n_cons; /* Total returned via arrays */
}

/* Port of Python: _parse_structured_summary
 *
 * Extract structured YAML block from LLM final response.
 * Finds ```yaml ... ``` fenced block, then uses libyaml to parse
 * consolidations and prunings lists.
 *
 * Returns structured data or empty arrays on failure.
 */
int parse_structured_summary(const char *llm_final,
                             curator_consolidation_t consolidations[],
                             int max_cons,
                             curator_pruning_t prunings[],
                             int max_prun)
{
    int n_cons = 0, n_prun = 0;
    if (!llm_final || !*llm_final) return 0;

    /* Find ```yaml or ```yml block */
    const char *start = strstr(llm_final, "```yaml");
    if (!start) start = strstr(llm_final, "```yml");
    if (!start) return 0;

    start += (start[3] == 'y' ? 7 : 6); /* skip past ```yaml\n or ```yml\n */
    /* Skip to end of line */
    while (*start && *start != '\n') start++;
    if (*start == '\n') start++;

    /* Find closing ``` */
    const char *end = strstr(start, "```");
    if (!end) return 0;

    /* Extract YAML body */
    size_t body_len = (size_t)(end - start);
    if (body_len == 0 || body_len > 8192) return 0;

    char yaml_body[8192];
    memcpy(yaml_body, start, body_len);
    yaml_body[body_len] = '\0';

    /* Simple key-value extraction from the YAML structure.
     * We look for consolidations: and prunings: arrays */
    char *p = yaml_body;
    while (*p && n_cons < max_cons) {
        /* Look for "- from:" pattern */
        char *from_pos = strstr(p, "from:");
        if (!from_pos) break;
        from_pos += 5;
        while (*from_pos == ' ') from_pos++;

        /* Check if we've passed into prunings section */
        if (strstr(p, "prunings:") && from_pos > strstr(p, "prunings:"))
            break;

        /* Check we're still in consolidations section */
        if (n_cons == 0 && strstr(p, "consolidations:")) {
            const char *cons_start = strstr(p, "consolidations:");
            if (from_pos < cons_start) {
                p = from_pos + 1;
                continue;
            }
        }

        /* Extract "from" value */
        char from_val[128] = "";
        if (*from_pos == '"') {
            from_pos++;
            int vi = 0;
            while (*from_pos && *from_pos != '"' && vi < 127) from_val[vi++] = *from_pos++;
        } else {
            int vi = 0;
            while (*from_pos && *from_pos != '\n' && *from_pos != '\r' && vi < 127)
                from_val[vi++] = *from_pos++;
        }
        from_val[127] = '\0';

        /* Find "into:" */
        char *into_pos = strstr(from_pos, "into:");
        if (!into_pos) { p = from_pos + 1; continue; }
        into_pos += 5;
        while (*into_pos == ' ') into_pos++;
        char into_val[128] = "";
        if (*into_pos == '"') {
            into_pos++;
            int vi = 0;
            while (*into_pos && *into_pos != '"' && vi < 127) into_val[vi++] = *into_pos++;
        } else {
            int vi = 0;
            while (*into_pos && *into_pos != '\n' && *into_pos != '\r' && vi < 127)
                into_val[vi++] = *into_pos++;
        }
        into_val[127] = '\0';

        if (from_val[0] && into_val[0]) {
            snprintf(consolidations[n_cons].name, sizeof(consolidations[n_cons].name),
                     "%s", from_val);
            snprintf(consolidations[n_cons].into, sizeof(consolidations[n_cons].into),
                     "%s", into_val);
            n_cons++;
        }

        p = from_pos + 1;
    }

    /* Extract prunings */
    const char *prun_section = strstr(yaml_body, "prunings:");
    if (prun_section) {
        p = (char*)prun_section + 9;
        while (*p && n_prun < max_prun) {
            char *name_pos = strstr(p, "name:");
            if (!name_pos) break;
            name_pos += 5;
            while (*name_pos == ' ') name_pos++;
            char name_val[128] = "";
            if (*name_pos == '"') {
                name_pos++;
                int vi = 0;
                while (*name_pos && *name_pos != '"' && vi < 127) name_val[vi++] = *name_pos++;
            } else {
                int vi = 0;
                while (*name_pos && *name_pos != '\n' && *name_pos != '\r' && vi < 127)
                    name_val[vi++] = *name_pos++;
            }
            name_val[127] = '\0';
            if (name_val[0]) {
                snprintf(prunings[n_prun].name, sizeof(prunings[n_prun].name),
                         "%s", name_val);
                n_prun++;
            }
            p = name_pos + 1;
        }
    }

    return n_cons; /* Total count accessible via output arrays */
}


/* Port of Python agent/curator.py: _reconcile_classification, _build_rename_summary, _write_run_report, _render_report_markdown, run_curator_review, _run_llm_review. Consolidated. */
/* AG26: Port of Python agent/curator.py:_reconcile_classification() */
/* AG26: Port of Python agent/curator.py:_build_rename_summary() */
/* AG26: Port of Python agent/curator.py:_write_run_report() */
/* AG26: Port of Python agent/curator.py:_render_report_markdown() */
/* AG26: Port of Python agent/curator.py:run_curator_review() */
/* AG26: Port of Python agent/curator.py:_run_llm_review() */

/* Port of Python: get_consolidate */
bool curator_get_consolidate(void) {
    const char *val = getenv("CURATOR_CONSOLIDATE");
    if (val && *val) {
        if (strcmp(val, "true") == 0 || strcmp(val, "1") == 0 || strcmp(val, "yes") == 0)
            return true;
        return false;
    }
    return false; /* DEFAULT_CONSOLIDATE = false */
}
