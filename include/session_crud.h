/*
 * session_crud.h — Slermes C11 port of tools/session_crud.py handler API.
 *
 * Public handler surface consumed by the command dispatcher (commands.c,
 * cli_cmd_session.c, web_dashboard.c). Faithful extraction from the god
 * header so callers no longer include hermes.h transitively.
 */

#ifndef SESSION_CRUD_H
#define SESSION_CRUD_H

#ifdef __cplusplus
extern "C" {
#endif

/* Dispatch a session CRUD request. Returns a heap JSON string
 * (caller frees) or an error object. */
char *session_crud_handler(const char *args_json, const char *task_id);

#ifdef __cplusplus
}
#endif

#endif /* SESSION_CRUD_H */
