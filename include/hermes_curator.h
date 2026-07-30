/**
 * @file hermes_curator.h
 * @brief B05: Curator — background skill maintenance orchestrator.
 *
 * Manages skill lifecycle: tracks state (last_run_at, run_count, paused),
 * provides status for the /curator CLI command.
 */
#ifndef HERMES_CURATOR_H
#define HERMES_CURATOR_H

#include "hermes_core_types.h"
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum summary length */
#define CURATOR_SUMMARY_MAX 4096

/** Default config values (used when config.yaml has no curator section) */
#define CURATOR_DEFAULT_INTERVAL_HOURS 168   /* 24 * 7 */
#define CURATOR_DEFAULT_STALE_AFTER_DAYS 30
#define CURATOR_DEFAULT_ARCHIVE_AFTER_DAYS 90
#define CURATOR_DEFAULT_MIN_IDLE_HOURS 2.0

/** Curator state persisted to .curator_state JSON file */
typedef struct {
    bool    enabled;           /* Is curator enabled in config */
    bool    paused;            /* User-paused state */
    int     run_count;         /* Total runs since state reset */
    time_t  last_run_at;       /* Unix timestamp of last run */
    double  last_run_duration; /* Seconds the last run took */
    char    last_run_summary[CURATOR_SUMMARY_MAX]; /* Text summary */
} curator_state_t;

/** Counts returned by apply_automatic_transitions */
typedef struct {
    int marked_stale;
    int archived;
    int reactivated;
    int checked;
    int seeded;
} curator_transition_counts_t;

/** Maximum entries in classification arrays */
#define CURATOR_MAX_DECLARATIONS 128
#define CURATOR_MAX_EVIDENCE 512

/** A single absorbed_into declaration from a skill_manage delete call */
typedef struct {
    char skill_name[128];
    char absorbed_into[128];
    bool declared;
} curator_declaration_t;

/** A consolidation entry (skill absorbed into umbrella) */
typedef struct {
    char name[128];
    char into[128];
    char evidence[CURATOR_MAX_EVIDENCE];
} curator_consolidation_t;

/** A pruning entry (skill archived without consolidation) */
typedef struct {
    char name[128];
    char reason[256];
} curator_pruning_t;

/**
 * Load curator state from the state file.
 * Returns false if no state file exists (first run).
 */
bool load_state(curator_state_t *state);

/**
 * Save curator state to the state file.
 */
void save_state(const curator_state_t *state);

/**
 * Initialize curator state with defaults.
 */
void init_state(curator_state_t *state);

/**
 * Check if curator is enabled.
 * Port of Python agent/curator.py:is_enabled().
 */
bool is_enabled(void);

/**
 * Curator BACKUP config (port of agent/curator_backup.py). These read the
 * `curator.backup` section of config.yaml via the real YAML parser (libyaml).
 */
bool curator_backup_config_enabled(void);
int  curator_backup_config_keep(void);

/**
 * Get curator run interval in hours from env or default.
 * Port of Python agent/curator.py:get_interval_hours().
 */
int get_interval_hours(void);

/**
 * Get minimum idle hours before curator runs, from env or default.
 * Port of Python agent/curator.py:get_min_idle_hours().
 */
double get_min_idle_hours(void);

/**
 * Get days after which a skill is considered stale, from env or default.
 * Port of Python agent/curator.py:get_stale_after_days().
 */
int get_stale_after_days(void);

/**
 * Get days after which a stale skill is archived, from env or default.
 * Port of Python agent/curator.py:get_archive_after_days().
 */
int get_archive_after_days(void);

/**
 * Check if curator may prune built-in skills.
 * Port of Python agent/curator.py:get_prune_builtins().
 */
bool get_prune_builtins(void);

/**
 * Check all gates: enabled, not paused, interval elapsed.
 * Returns true if curator should run now.
 * Port of Python agent/curator.py:should_run_now().
 */
bool should_run_now(void);

/**
 * Walk every curator-managed skill and apply lifecycle transitions
 * based on the latest activity timestamp. Pinned skills are never touched.
 * Port of Python agent/curator.py:apply_automatic_transitions().
 */
curator_transition_counts_t apply_automatic_transitions(void);

/**
 * Build a human-readable list of agent-created skills with usage stats.
 * Caller must free the returned string.
 * Port of Python agent/curator.py:_render_candidate_list().
 */
char *render_candidate_list(void);

/**
 * Strip whitespace from a credential string. Returns malloc'd copy
 * or NULL if value is NULL or empty after stripping.
 * Port of Python agent/curator.py:_strip_aux_credential().
 */
char *strip_aux_credential(const char *value);

/**
 * Check if needle is a complete filename stem or directory in path.
 * Port of Python agent/curator.py:_needle_in_path_component().
 */
bool needle_in_path_component(const char *needle, const char *path);

/**
 * Parse ISO-8601 datetime string to time_t.
 * Port of Python agent/curator.py:_parse_iso().
 */
time_t parse_iso_time(const char *ts);

/**
 * Gate check: should curator run? Pass -1 for idle_for_seconds to skip idle check.
 * Port of Python agent/curator.py:maybe_run_curator().
 */
bool curator_maybe_run(double idle_for_seconds);

/**
 * Walk tool calls and extract model-declared absorption targets.
 * Only skill_manage(action=delete) calls with absorbed_into are extracted.
 * Port of Python agent/curator.py:_extract_absorbed_into_declarations().
 */
int extract_absorbed_into_declarations(const char *tool_calls[], int n_calls,
                                       curator_declaration_t declarations[],
                                       int max_decls);

/**
 * Classify removed skills as consolidated (absorbed into umbrella) or pruned.
 * Heuristic: scans tool call args for content referencing the removed skill name.
 * Returns number of consolidated items; pruned stored in separate array.
 * Port of Python agent/curator.py:_classify_removed_skills().
 */
int classify_removed_skills(const char *removed[], int n_removed,
                            const char *added[], int n_added,
                            const char *after_names[], int n_after,
                            const char *tool_call_args[], int n_calls,
                            curator_consolidation_t consolidated[], int max_cons,
                            char *pruned[], int max_pruned);

/**
 * Extract structured YAML block (consolidations/prunings) from LLM final text.
 * Finds ```yaml ... ``` fenced block and parses from/into/name fields.
 * Port of Python agent/curator.py:_parse_structured_summary().
 */
int parse_structured_summary(const char *llm_final,
                             curator_consolidation_t consolidations[],
                             int max_cons,
                             curator_pruning_t prunings[],
                             int max_prun);

/**
 * Update state after a run completes.
 * Sets last_run_at, increments run_count, stores summary.
 */
void record_run(curator_state_t *state, double duration_secs,
                         const char *summary);

/**
 * Set curator paused state and persist to state file.
 * Port of Python agent/curator.py:set_paused().
 */
void set_paused(bool paused);

/**
 * Check if curator is paused from state file.
 * Port of Python agent/curator.py:is_paused().
 */
bool is_paused(void);

/**
 * Format human-readable duration string from seconds.
 * Fills buf with "Xm Ys", "Xh Ym", "Xd Xh", or "Xs".
 */
void format_duration(double seconds, char *buf, size_t sz);

/**
 * Format a time_t as a relative time string ("5m ago", "2h ago", etc.).
 */
void format_reltime(time_t t, char *buf, size_t sz);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_CURATOR_H */
