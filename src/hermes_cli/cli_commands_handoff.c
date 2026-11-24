
/*
 * cli_commands_handoff.c - real C11 port of upstream Hermes
 * hermes_cli/cli_commands_handoff.py: the /handoff pre-checks and its two-phase poll.
 * The i18n lookup (_t) and the console writer (_cp) are seams: both sides render a
 * line as "  " + key (or the raw line when it is not a key), so the printed text is
 * identical. Storage reads/writes are injected decision inputs on both sides.
 * PoP: w19_clicommand_b3f78e_keep @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_keep
 * PoP: w19_clicommand_b3f78e_prepare_session @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_prepare_session
 * PoP: w19_clicommand_b3f78e_validate_target @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_validate_target
 * PoP: w19_clicommand_b3f78e_handoff_wait @ hermes_cli/cli_commands_handoff.py:CLIHandoffMixin._handoff_wait
 */
#include "noc_w19_clicommand_b3f78e.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _HANDOFF_PENDING_TIMEOUT / _HANDOFF_RUNNING_TIMEOUT / _HANDOFF_HEARTBEAT_EVERY */
#define PENDING_TIMEOUT 60.0
#define RUNNING_TIMEOUT 900.0
#define HEARTBEAT_EVERY 30.0

static void add_line(json_t *lines, const char *text) {
    json_append(lines, json_string(text ? text : ""));
}

/* f"  {_t(key)}" with the oracle's identity _t. */
static void add_key_line(json_t *lines, const char *key) {
    char buf[512];
    snprintf(buf, sizeof(buf), "  %s", key ? key : "");
    add_line(lines, buf);
}

static const char *obj_state(json_t *row, const char *fallback) {
    if (!row || row->type != JSON_OBJECT) return fallback;
    json_t *v = json_obj_get(row, "state");
    if (v && v->type == JSON_STRING && v->str_val && v->str_val[0]) return v->str_val;
    return fallback;
}

json_t *w19_clicommand_b3f78e_keep(json_t *lines) {
    json_t *out = json_object();
    json_t *echo = json_array();
    if (lines && lines->type == JSON_ARRAY)
        for (size_t i = 0; i < json_len(lines); i++) {
            json_t *v = json_get(lines, i);
            if (v && v->type == JSON_STRING) json_append(echo, json_string(v->str_val));
        }
    json_set(out, "result", json_bool(true));
    json_set(out, "lines", echo);
    return out;
}

json_t *w19_clicommand_b3f78e_prepare_session(int agent_running, int store_present,
                                             int session_db_present, int session_exists,
                                             int set_title_raises, int get_title_raises,
                                             const char *session_id, const char *title_value) {
    json_t *out = json_object();
    json_t *lines = json_array();
    const char *sid = session_id ? session_id : "";
    char shortid[9];
    size_t n = strlen(sid) < 8 ? strlen(sid) : 8;
    memcpy(shortid, sid, n);
    shortid[n] = '\0';
    char handoff_title[64];
    snprintf(handoff_title, sizeof(handoff_title), "handoff-%s", shortid);

    const char *result = NULL;
    if (agent_running) {
        add_key_line(lines, "shared.agent_busy");
    } else if (!store_present && !session_db_present) {
        add_key_line(lines, "DB_UNAVAILABLE");     /* _db_unavailable_line(): "  " + text */
    } else if (!session_exists) {
        if (set_title_raises) {
            add_key_line(lines, "handoff.session_row_failed");
        } else {
            /* set_title writes nothing without a row, so the handoff title is the title */
            const char *session_title = get_title_raises ? "" : handoff_title;
            result = session_title[0] ? session_title : shortid;
        }
    } else {
        const char *session_title = get_title_raises ? "" : (title_value ? title_value : "");
        result = session_title[0] ? session_title : shortid;
    }
    json_set(out, "result", result ? json_string(result) : json_null());
    json_set(out, "lines", lines);
    return out;
}

json_t *w19_clicommand_b3f78e_validate_target(const char *platform_name, const char *outcome,
                                              const char *home_name) {
    json_t *out = json_object();
    json_t *lines = json_array();
    const char *o = outcome ? outcome : "ok";
    const char *result = NULL;
    if (!strcmp(o, "ok")) {
        if (home_name && home_name[0]) result = home_name;
    } else if (!strcmp(o, "config_error")) {
        add_key_line(lines, "handoff.config_load_failed");
    } else if (!strcmp(o, "no_home_channel")) {
        add_key_line(lines, "handoff.no_home_channel");
        add_key_line(lines, "handoff.set_home_hint");
    } else {
        char key[256];
        snprintf(key, sizeof(key), "handoff.%s", o);
        add_key_line(lines, key);
    }
    (void)platform_name;
    json_set(out, "result", result ? json_string(result) : json_null());
    json_set(out, "lines", lines);
    return out;
}

/* The injected state script's next row. An entry may carry "n" to be served n times
 * in a row; entries past the end repeat the last one. A {"raise":true} entry is an
 * unreadable state (the live try/except path). */
typedef struct {
    size_t pos;
    size_t used;
} script_cursor;

static void read_next(json_t *states, script_cursor *cur, size_t *reads, json_t **row_out, int *raised_out) {
    *row_out = NULL;
    *raised_out = 0;
    (*reads)++;
    size_t n = (states && states->type == JSON_ARRAY) ? json_len(states) : 0;
    if (!n) return;
    while (cur->pos < n) {
        json_t *entry = json_get(states, cur->pos);
        size_t count = 1;
        if (entry && entry->type == JSON_OBJECT) {
            json_t *k = json_obj_get(entry, "n");
            if (k && k->type == JSON_NUMBER && k->num_val > 0) count = (size_t)k->num_val;
        }
        if (cur->used >= count) { cur->pos++; cur->used = 0; continue; }
        cur->used++;
        if (entry && entry->type == JSON_OBJECT) {
            json_t *raise = json_obj_get(entry, "raise");
            if (raise && raise->type == JSON_BOOL && raise->bool_val) { *raised_out = 1; return; }
            if (raise && raise->type == JSON_NUMBER && raise->num_val != 0.0) { *raised_out = 1; return; }
        }
        *row_out = entry;
        return;
    }
    json_t *last = json_get(states, n - 1);
    if (last && last->type == JSON_OBJECT) {
        json_t *raise = json_obj_get(last, "raise");
        if (raise && raise->type == JSON_BOOL && raise->bool_val) { *raised_out = 1; return; }
    }
    *row_out = last;
}

json_t *w19_clicommand_b3f78e_handoff_wait(json_t *states, int fail_pending_raises,
                                           const char *platform_name, const char *session_title) {
    json_t *out = json_object();
    json_t *lines = json_array();
    json_t *fail_calls = json_array();
    size_t reads = 0, sleeps = 0;
    script_cursor cur = { 0, 0 };
    double clock = 1000.0;
    int should_exit = 0, result = 1, broke = 0;
    double pending_deadline = clock + PENDING_TIMEOUT;
    double running_deadline = 0.0;
    int have_running = 0;
    double next_heartbeat = 0.0;
    int have_heartbeat = 0;
    const char *last_state = "pending";
    json_t *recovered = NULL;
    (void)platform_name;   /* rides _t kwargs only, which the identity _t drops */
    (void)session_title;

    for (;;) {
        json_t *state_row = NULL;
        int raised = 0;
        if (recovered) {
            state_row = recovered;
            recovered = NULL;
        } else {
            read_next(states, &cur, &reads, &state_row, &raised);
            if (raised) state_row = NULL;            /* except Exception: state_row = None */
        }
        const char *current = obj_state(state_row, "pending");
        if (strcmp(current, last_state)) {
            if (!strcmp(current, "running")) {
                add_key_line(lines, "handoff.picked_up");
                running_deadline = clock + RUNNING_TIMEOUT;
                have_running = 1;
                next_heartbeat = clock + HEARTBEAT_EVERY;
                have_heartbeat = 1;
            }
            last_state = current;
        }
        if (!strcmp(current, "completed")) {
            add_line(lines, "");
            add_key_line(lines, "handoff.complete");
            add_key_line(lines, "handoff.resume_hint");
            add_line(lines, "");
            should_exit = 1;
            result = 0;
            broke = 1;
            break;
        }
        if (!strcmp(current, "failed")) {
            /* _t is identity here, so the error kwarg never reaches the printed line */
            add_key_line(lines, "handoff.failed");
            add_key_line(lines, "handoff.session_intact_retry");
            result = 1;
            broke = 1;
            break;
        }
        double now = clock;
        if (!strcmp(current, "pending")) {
            if (now >= pending_deadline) {
                /* unclaimed: CAS-clear so the user can retry, then re-read once */
                if (!fail_pending_raises) add_line(fail_calls, "timed out waiting for gateway");
                json_t *row = NULL;
                int raise2 = 0;
                read_next(states, &cur, &reads, &row, &raise2);
                recovered = raise2 ? NULL : row;
                const char *rec = recovered ? obj_state(recovered, "") : "";
                if (!strcmp(rec, "running") || !strcmp(rec, "completed")) continue;
                break;
            }
        } else {
            if (have_heartbeat && now >= next_heartbeat) {
                add_key_line(lines, "handoff.still_transferring");
                next_heartbeat = now + HEARTBEAT_EVERY;
            }
            if (have_running && now >= running_deadline) {
                /* the gateway owns the row: never fail it here (split-brain bug) */
                add_key_line(lines, "handoff.taking_long");
                add_key_line(lines, "handoff.check_platform");
                add_key_line(lines, "handoff.no_longer_waiting");
                add_key_line(lines, "handoff.retry_when_settled");
                result = 1;
                broke = 1;
                break;
            }
        }
        clock += 0.5;                                /* time.sleep(0.5) on the injected clock */
        sleeps++;
        if (sleeps > 4000) { broke = 1; break; }      /* the poll never outlives its deadlines */
    }
    if (!broke) {                                    /* the pending timeout falls through here */
        add_key_line(lines, "handoff.timed_out");
        add_key_line(lines, "handoff.session_intact");
        result = 1;
    }
    json_set(out, "result", json_bool(result ? true : false));
    json_set(out, "lines", lines);
    json_set(out, "should_exit", json_bool(should_exit ? true : false));
    json_set(out, "reads", json_number((double)reads));
    json_set(out, "fail_calls", fail_calls);
    json_set(out, "sleeps", json_number((double)sleeps));
    return out;
}
