/*
 * port_session_owned.h — pure helper from gateway/session_connections.py.
 */
#ifndef PORT_SESSION_OWNED_H
#define PORT_SESSION_OWNED_H

/* session_owned(params): True when params is a dict whose "owner" is a dict of
 * type "session". Handles NULL and non-dict params as False. Returns 0/1. */
int sco_session_owned(const char *params_json);

#endif /* PORT_SESSION_OWNED_H */
