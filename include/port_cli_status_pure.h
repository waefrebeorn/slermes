/*
 * port_cli_status_pure.h — Pure formatting helpers from cli.py
 * (_status_bar_goal_segment, _fmt_stash_age).
 */
#ifndef PORT_CLI_STATUS_PURE_H
#define PORT_CLI_STATUS_PURE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: _status_bar_goal_segment @ cli.py:_status_bar_goal_segment */
/* Returns the "⊙ goal 3/20" segment, or "" when no goal is active.
 * snapshot_json: JSON object with optional keys goal_active (bool),
 * goal_turns_used (int), goal_max_turns (int). Returns malloc'd string; caller frees. */
char *cli_status_goal_segment(const char *snapshot_json);

/* PoP: _fmt_stash_age @ cli.py:_fmt_stash_age */
/* Human-readable age for a stash entry ("just now", "42s ago",
 * "7 min ago", "3h ago"). stashed_at and now_mono are monotonic-clock
 * seconds. Returns malloc'd string; caller frees. */
char *cli_status_fmt_stash_age(double stashed_at, double now_mono);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CLI_STATUS_PURE_H */
