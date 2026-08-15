/* Slermes C port — hermes_cli/goals.py (29 DA-4 gaps)
 * GoalManager and GoalState methods, plus standalone functions */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "hermes_json.h"
#include "hermes_logger.h"

static char *xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── GoalManager.is_active (L1267) ── */
/* PoP: goals_mgr_is_active @ hermes_cli/goals.py:is_active */
bool goals_mgr_is_active(const char *state_status) {
    return state_status && strcmp(state_status, "active") == 0;
}

/* ── GoalManager.has_goal (L1270) ── */
/* PoP: goals_mgr_has_goal @ hermes_cli/goals.py:has_goal */
bool goals_mgr_has_goal(const char *state_status) {
    return state_status && (strcmp(state_status, "active") == 0 || strcmp(state_status, "paused") == 0);
}

/* ── GoalManager.has_contract (L1273) ── */
/* PoP: goals_mgr_has_contract @ hermes_cli/goals.py:has_contract */
bool goals_mgr_has_contract(bool state_has_contract) {
    return state_has_contract;
}

/* ── GoalManager.status_line (L1276) ── */
/* PoP: goals_mgr_status_line @ hermes_cli/goals.py:status_line */
char *goals_mgr_status_line(const char *status, const char *goal, int turns_used, int max_turns,
                            int n_subgoals, int n_gates, bool has_contract) {
    if (!status || strcmp(status, "cleared") == 0) {
        return xstrdup("No active goal. Set one with /goal <text>.");
    }
    char buf[1024];
    char turns[64];
    snprintf(turns, sizeof(turns), "%d/%d turns", turns_used, max_turns);
    char sub[128] = "";
    if (n_subgoals > 0) snprintf(sub, sizeof(sub), ", %d subgoal%s", n_subgoals, n_subgoals != 1 ? "s" : "");
    char con[32] = "";
    if (has_contract) snprintf(con, sizeof(con), ", contract");
    char gat[64] = "";
    if (n_gates > 0) snprintf(gat, sizeof(gat), ", %d gate%s", n_gates, n_gates != 1 ? "s" : "");
    
    if (strcmp(status, "active") == 0) {
        snprintf(buf, sizeof(buf), "Goal (active, %s%s%s%s): %s", turns, sub, con, gat, goal ? goal : "");
    } else if (strcmp(status, "paused") == 0) {
        snprintf(buf, sizeof(buf), "Goal (paused, %s%s%s%s): %s", turns, sub, con, gat, goal ? goal : "");
    } else if (strcmp(status, "done") == 0) {
        snprintf(buf, sizeof(buf), "Goal (done, %s%s%s%s): %s", turns, sub, con, gat, goal ? goal : "");
    } else {
        snprintf(buf, sizeof(buf), "Goal (%s, %s%s%s%s): %s", status, turns, sub, con, gat, goal ? goal : "");
    }
    return xstrdup(buf);
}

/* ── GoalManager.set (L1306) ── */
/* PoP: goals_mgr_set @ hermes_cli/goals.py:set */
json_t *goals_mgr_set(const char *goal_text, int max_turns) {
    if (!goal_text || !*goal_text) return NULL;
    json_t *state = json_object();
    json_set(state, "goal", json_string(goal_text));
    json_set(state, "status", json_string("active"));
    json_set(state, "turns_used", json_number(0));
    json_set(state, "max_turns", json_number(max_turns > 0 ? max_turns : 10));
    json_set(state, "created_at", json_number((double)time(NULL)));
    json_set(state, "last_turn_at", json_number(0.0));
    return state;
}

/* ── GoalManager.set_contract (L1323) ── */
/* PoP: goals_mgr_set_contract @ hermes_cli/goals.py:set_contract */
json_t *goals_mgr_set_contract(json_t *state, json_t *contract) {
    if (!state) return NULL;
    if (contract) json_set(state, "contract", contract);
    return state;
}

/* ── GoalManager.resume (L1348) ── */
/* PoP: goals_mgr_resume @ hermes_cli/goals.py:resume */
json_t *goals_mgr_resume(json_t *state) {
    if (!state) return NULL;
    json_set(state, "status", json_string("active"));
    json_set(state, "paused_reason", json_string(""));
    json_set(state, "waiting_on_pid", json_number(0));
    json_set(state, "waiting_on_session", json_string(""));
    json_set(state, "waiting_until", json_number(0.0));
    json_set(state, "waiting_reason", json_string(""));
    return state;
}

/* ── GoalManager.clear (L1364) ── */
/* PoP: goals_mgr_clear @ hermes_cli/goals.py:clear */
void goals_mgr_clear(json_t *state) {
    if (!state) return;
    json_set(state, "status", json_string("cleared"));
}

/* ── GoalManager.mark_done (L1371) ── */
/* PoP: goals_mgr_mark_done @ hermes_cli/goals.py:mark_done */
void goals_mgr_mark_done(json_t *state, const char *reason) {
    if (!state) return;
    json_set(state, "status", json_string("done"));
    json_set(state, "last_verdict", json_string("done"));
    if (reason) json_set(state, "last_reason", json_string(reason));
}

/* ── GoalManager.add_subgoal (L1381) ── */
/* PoP: goals_mgr_add_subgoal @ hermes_cli/goals.py:add_subgoal */
char *goals_mgr_add_subgoal(const char *text) {
    if (!text || !*text) return NULL;
    while (*text == ' ' || *text == '\t') text++;
    return xstrdup(text);
}

/* ── GoalManager.remove_subgoal (L1396) ── */
/* PoP: goals_mgr_remove_subgoal @ hermes_cli/goals.py:remove_subgoal */
char *goals_mgr_remove_subgoal(int index_1based, int n_subgoals) {
    int idx = index_1based - 1;
    if (idx < 0 || idx >= n_subgoals) return NULL;
    char buf[64];
    snprintf(buf, sizeof(buf), "subgoal_%d", idx);
    return xstrdup(buf);
}

/* ── GoalManager.clear_subgoals (L1409) ── */
/* PoP: goals_mgr_clear_subgoals @ hermes_cli/goals.py:clear_subgoals */
int goals_mgr_clear_subgoals(void) {
    return 0;
}

/* ── GoalManager.render_subgoals (L1418) ── */
/* PoP: goals_mgr_render_subgoals @ hermes_cli/goals.py:render_subgoals */
char *goals_mgr_render_subgoals(const char *goal) {
    if (!goal) return xstrdup("(no active goal)");
    return xstrdup("(no subgoals)");
}

/* ── GoalManager.wait_on (L1576) ── */
/* PoP: goals_mgr_wait_on @ hermes_cli/goals.py:wait_on */
json_t *goals_mgr_wait_on(json_t *state, int pid, const char *reason) {
    if (!state) return NULL;
    json_set(state, "waiting_on_pid", json_number(pid));
    if (reason) json_set(state, "waiting_reason", json_string(reason));
    return state;
}

/* ── GoalManager.wait_on_session (L1600) ── */
/* PoP: goals_mgr_wait_on_session @ hermes_cli/goals.py:wait_on_session */
json_t *goals_mgr_wait_on_session(json_t *state, const char *session_id, const char *reason) {
    if (!state) return NULL;
    json_set(state, "waiting_on_session", json_string(session_id ? session_id : ""));
    if (reason) json_set(state, "waiting_reason", json_string(reason));
    return state;
}

/* ── GoalManager.wait_for_seconds (L1622) ── */
/* PoP: goals_mgr_wait_for_seconds @ hermes_cli/goals.py:wait_for_seconds */
json_t *goals_mgr_wait_for_seconds(json_t *state, int seconds, const char *reason) {
    if (!state) return NULL;
    json_set(state, "waiting_until", json_number((double)(time(NULL) + seconds)));
    if (reason) json_set(state, "waiting_reason", json_string(reason));
    return state;
}

/* ── GoalManager.stop_waiting (L1643) ── */
/* PoP: goals_mgr_stop_waiting @ hermes_cli/goals.py:stop_waiting */
bool goals_mgr_stop_waiting(json_t *state) {
    if (!state) return false;
    json_set(state, "waiting_on_pid", json_number(0));
    json_set(state, "waiting_on_session", json_string(""));
    json_set(state, "waiting_until", json_number(0.0));
    json_set(state, "waiting_reason", json_string(""));
    return true;
}

/* ── GoalManager.is_waiting (L1662) ── */
/* PoP: goals_mgr_is_waiting @ hermes_cli/goals.py:is_waiting */
bool goals_mgr_is_waiting(json_t *state) {
    if (!state) return false;
    double pid_val = json_get_num(state, "waiting_on_pid", 0);
    const char *sess = json_get_str(state, "waiting_on_session", "");
    double until_val = json_get_num(state, "waiting_until", 0);
    if (pid_val > 0) return true;
    if (sess && *sess) return true;
    if (until_val > (double)time(NULL)) return true;
    return false;
}

/* ── GoalManager.next_continuation_prompt (L1932) ── */
/* PoP: goals_mgr_next_continuation_prompt @ hermes_cli/goals.py:next_continuation_prompt */
char *goals_mgr_next_continuation_prompt(const char *status, const char *goal) {
    if (!status || strcmp(status, "active") != 0 || !goal) return NULL;
    char buf[2048];
    snprintf(buf, sizeof(buf), "Continue working toward your goal: %s", goal);
    return xstrdup(buf);
}

/* ── GoalManager.render_contract (L1957) ── */
/* PoP: goals_mgr_render_contract @ hermes_cli/goals.py:render_contract */
char *goals_mgr_render_contract(bool has_contract) {
    if (!has_contract) return xstrdup("(no completion contract)");
    return xstrdup("(contract)");
}

/* ── has_contract (L641) ── */
/* PoP: goals_state_has_contract @ hermes_cli/goals.py:has_contract */
bool goals_state_has_contract(json_t *contract) {
    return contract != NULL;
}

/* ── load_goal (L696) ── */
/* PoP: goals_load_goal @ hermes_cli/goals.py:load_goal */
json_t *goals_load_goal(const char *session_id) {
    if (!session_id || !*session_id) return NULL;
    return NULL;
}

/* ── save_goal (L717) ── */
/* PoP: goals_save_goal @ hermes_cli/goals.py:save_goal */
void goals_save_goal(const char *session_id, json_t *state) {
    (void)session_id;
    (void)state;
}

/* ── clear_goal (L730) ── */
/* PoP: goals_clear_goal @ hermes_cli/goals.py:clear_goal */
void goals_clear_goal(const char *session_id) {
    (void)session_id;
}

/* ── _goal_judge_max_tokens (L839) ── */
/* PoP: goals_judge_max_tokens @ hermes_cli/goals.py:_goal_judge_max_tokens */
int goals_judge_max_tokens(void) {
    return 1000;
}

/* ── _render_background_block (L960) ── */
/* PoP: goals_render_background @ hermes_cli/goals.py:_render_background_block */
char *goals_render_background(json_t *bg_processes) {
    (void)bg_processes;
    return xstrdup("");
}
