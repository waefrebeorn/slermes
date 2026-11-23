#ifndef NOC_W15_SERVERREQU_CDBD30_H
#define NOC_W15_SERVERREQU_CDBD30_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_serverrequ_cdbd30_can_replay_request @ tui_gateway/server_requests.py:can_replay_request */
struct json_t *w15_serverrequ_cdbd30_can_replay_request(const char *method,
                                                         const struct json_t *params);
/* PoP: w15_serverrequ_cdbd30_is_not_shown @ tui_gateway/server_requests.py:_is_not_shown */
struct json_t *w15_serverrequ_cdbd30_is_not_shown(const struct json_t *frame);
/* PoP: w15_serverrequ_cdbd30_is_response_frame @ tui_gateway/server_requests.py:is_response_frame */
struct json_t *w15_serverrequ_cdbd30_is_response_frame(const struct json_t *obj);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_SERVERREQU_CDBD30_H */
