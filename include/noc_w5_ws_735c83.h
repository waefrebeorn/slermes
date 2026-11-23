#ifndef NOC_W5_WS_735C83_H
#define NOC_W5_WS_735C83_H

#include <stddef.h>
struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w5_ws_735c83_peer_label @ tui_gateway/ws.py:_ws_peer_label
 * Caller frees. */
char *w5_ws_735c83_peer_label(const struct json_t *client);

/* PoP: w5_ws_735c83_is_unknown_method @ tui_gateway/ws.py:_is_unknown_method */
int w5_ws_735c83_is_unknown_method(const struct json_t *resp);

/* PoP: w5_ws_735c83_is_concurrent_read @ tui_gateway/ws.py:_WS_CONCURRENT_READ_METHODS */
int w5_ws_735c83_is_concurrent_read(const char *method);

/* PoP: w5_ws_735c83_is_streaming_event @ tui_gateway/ws.py:_STREAMING_EVENT_TYPES */
int w5_ws_735c83_is_streaming_event(const char *event_type);

/* PoP: w5_ws_735c83_answers_open_request @ tui_gateway/ws.py:_answers_open_request
 * `is_response_frame` is injected from the fixture. */
int w5_ws_735c83_answers_open_request(const struct json_t *req, int is_response);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W5_WS_735C83_H */
