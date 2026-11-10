/*
 * port_server.h -- pure helpers ported from tui_gateway/server.py.
 *
 * The dispatch / db / profile-scope / transport half stays in Python.
 */
#ifndef PORT_SERVER_H
#define PORT_SERVER_H

#include <stddef.h>

#define SV_FRAME_MAX 2048

/* _load_busy_input_mode -- strip+lower the config value, then require it to be
 * one of queue/steer/interrupt; anything else is "interrupt". Caller frees. */
char *sv_busy_input_mode(const char *raw);

/* _canonical_profile_request -- ".hermes"/"hermes" canonicalizes to "default"
 * unless a real profile dir of that name exists (`has_hermes_dir`). Caller frees. */
char *sv_canonical_profile(const char *name, int has_hermes_dir);

/* _current_profile_name -- the active profile, or "default". Caller frees. */
char *sv_current_profile(const char *active);

/* _event_frame -- the JSON-RPC event notification envelope. The "payload" key
 * is omitted (not null) when there is no payload. */
void sv_event_frame(char *out, size_t cap, const char *event, const char *sid,
                    const char *payload);

#endif /* PORT_SERVER_H */
