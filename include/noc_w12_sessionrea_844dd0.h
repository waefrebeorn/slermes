#ifndef NOC_W12_SESSIONREA_844DD0_H
#define NOC_W12_SESSIONREA_844DD0_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_sessionrea_844dd0_parse @ gateway/session_reactions.py:_parse
 * Faithful port of _parse(params): validates the authority reaction params and
 * returns 0 with *out = {"row_id": <int|null>, "newest_role": <value|null>,
 * "emoji": <str|null>, "author": <str>}. Returns -1 (with *out = NULL) on
 * AuthorityRefusal('invalid_params').
 * NOTE: a decimal STRING in params["row_id"] is the int64 transport seam; the
 * harness converts it with strtoll before calling. */
int w12_sessionrea_844dd0_parse(const struct json_t *params, struct json_t **out);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_SESSIONREA_844DD0_H */
