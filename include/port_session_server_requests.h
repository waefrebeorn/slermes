#ifndef PORT_SESSION_SERVER_REQUESTS_H
#define PORT_SESSION_SERVER_REQUESTS_H

#include "hermes_json.h"

/* Port of Python: gateway/session_server_requests.py — the frame predicate.
 *
 * The request registry itself (open/withdraw/resolve round trips, the viewer
 * fanout, lock_answer) is runtime state owned by the gateway event bus and is
 * not ported here.
 */

/* A viewer's answer to one of the owner's requests: no "method", a string id
 * starting with "srq-", and a result or an error. */
int ssr_is_response_frame(const char *frame_json);

#endif /* PORT_SESSION_SERVER_REQUESTS_H */
