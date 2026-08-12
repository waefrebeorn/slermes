#ifndef PORT_HERMES_CLI_HEARTBEAT_H
#define PORT_HERMES_CLI_HEARTBEAT_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: MIN_INTERVAL_SECONDS @ hermes_cli/heartbeat.py:MIN_INTERVAL_SECONDS */
extern const int hb_MIN_INTERVAL_SECONDS;
/* PoP: POLL_SECONDS @ hermes_cli/heartbeat.py:POLL_SECONDS */
extern const double hb_POLL_SECONDS;
/* PoP: HEARTBEAT_PROMPT_TEMPLATE @ hermes_cli/heartbeat.py:HEARTBEAT_PROMPT_TEMPLATE */
extern const char *hb_PROMPT_TEMPLATE;

typedef struct {
    char   *prompt;
    int     interval_seconds;
    char   *status;            /* "active" | "paused" | "cleared" */
    double  created_at;
    double  last_fired_at;
    int     fire_count;
} hb_state_t;

typedef struct {
    char        *session_id;
    hb_state_t  *state;        /* owned */
} hb_manager_t;

/* HeartBeatState */
hb_state_t *hb_state_new(const char *prompt, int interval_seconds,
                         const char *status, double created_at,
                         double last_fired_at, int fire_count);
void hb_state_free(hb_state_t *s);
char *hb_state_to_json(const hb_state_t *s);          /* PoP: to_json */
hb_state_t *hb_state_from_json(const char *raw);       /* PoP: from_json */
bool hb_state_is_due(const hb_state_t *s, double now); /* PoP: is_due */
char *hb_state_render_prompt(const hb_state_t *s);     /* PoP: render_prompt */

/* interval helpers */
int  hb_parse_interval(const char *text, int *out);    /* PoP: parse_interval */
char *hb_format_interval(int seconds);                  /* PoP: format_interval */

/* persistence */
char *hb_meta_key(const char *session_id);             /* PoP: _meta_key */
char *hb_get_session_db_path(void);                     /* PoP: _get_session_db */
hb_state_t *hb_load_heartbeat(const char *session_id);/* PoP: load_heartbeat */
bool hb_save_heartbeat(const char *session_id, const hb_state_t *state); /* PoP: save_heartbeat */

/* HeartbeatManager */
hb_manager_t *hb_manager_new(const char *session_id);   /* PoP: HeartbeatManager.__init__ */
void hb_manager_free(hb_manager_t *m);
hb_state_t *hb_manager_state(hb_manager_t *m);          /* PoP: HeartbeatManager.state */
bool hb_manager_has_heartbeat(hb_manager_t *m);         /* PoP: HeartbeatManager.has_heartbeat */
bool hb_manager_is_active(hb_manager_t *m);             /* PoP: HeartbeatManager.is_active */
char *hb_manager_status_line(hb_manager_t *m);          /* PoP: HeartbeatManager.status_line */
hb_state_t *hb_manager_set(hb_manager_t *m, const char *prompt, int interval_seconds); /* set */
hb_state_t *hb_manager_pause(hb_manager_t *m);          /* PoP: HeartbeatManager.pause */
hb_state_t *hb_manager_resume(hb_manager_t *m);         /* PoP: HeartbeatManager.resume */
bool hb_manager_clear(hb_manager_t *m);                 /* PoP: HeartbeatManager.clear */
char *hb_manager_due_prompt(hb_manager_t *m, double now); /* PoP: HeartbeatManager.due_prompt */

/* migration */
bool hb_migrate_heartbeat_to_session(const char *old_session_id,
                                     const char *new_session_id); /* PoP: migrate_heartbeat_to_session */

#ifdef __cplusplus
}
#endif
#endif /* PORT_HERMES_CLI_HEARTBEAT_H */
