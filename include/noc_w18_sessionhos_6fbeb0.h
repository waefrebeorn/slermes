#ifndef NOC_W18_SESSIONHOS_6FBEB0_H
#define NOC_W18_SESSIONHOS_6FBEB0_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w18_sessionhos_6fbeb0_check_claim @ gateway/session_hosted_service.py:_check_claim
 *
 * Owner-claim validation exactly as the live ``_check_claim`` performs it:
 * the actor subject must be a non-empty string, the room id must be a string
 * of at most 128 code points matching IDENTIFIER_RE
 * (``^[A-Za-z0-9][A-Za-z0-9._:-]*$``, re.fullmatch). Returns NULL when the
 * claim is accepted, or the literal wire code "invalid_params" when the live
 * function would raise AuthorityRefusal('invalid_params'). */
const char *w18_sessionhos_6fbeb0_check_claim(const char *actor_subject,
                                             const char *room_id);

/* PoP: w18_sessionhos_6fbeb0_attempt_retired @ gateway/session_hosted_service.py:attempt_retired
 *
 * 1 when *task* has conclusively left attempt *generation* (a newer
 * execution_generation, or the same generation with a retired status:
 * queued/deferred/settled/failed/cancelled). ``generation`` is passed as the
 * raw JSON value because the live guard is ``type(generation) is not int``:
 * booleans, strings, null and non-integral numbers all answer 0. */
int w18_sessionhos_6fbeb0_attempt_retired(const json_t *task,
                                          const json_t *generation);

/* PoP: w18_sessionhos_6fbeb0_retired_attempt_conflict @ gateway/session_hosted_service.py:retired_attempt_conflict
 *
 * The refusal payload the live function builds:
 * {"reason":"admission_conflict","field":"task",
 *  "detail":"the room task no longer runs this attempt"}. Caller owns it. */
json_t *w18_sessionhos_6fbeb0_retired_attempt_conflict(void);

/* PoP: w18_sessionhos_6fbeb0_start_ready @ gateway/session_hosted_service.py:start_ready_hosted_services
 *
 * The publish gate of the live function, with the live start()/import side
 * effects reduced to their decision: only a ready, non-draining runner whose
 * every served authority has a transport-installed service proceeds.
 *
 *   state    - runner.session_runtime_descriptor['state'] (NULL for absent)
 *   draining - truthiness of getattr(runner, '_draining', False)
 *   services - JSON array mirroring [getattr(a,'hosted_room_service',None)
 *              for a in serving_authorities(runner)]: null for a missing
 *              service, or an object whose "transport_installed" value is
 *              the truthiness of its _transport_installed attribute.
 *
 * Returns {"action":"start"|"skip","started":int} where "started" counts the
 * services the live loop would start (0 on either early return). Caller owns
 * it. */
json_t *w18_sessionhos_6fbeb0_start_ready(const char *state, int draining,
                                          const json_t *services);

/* PoP: w18_sessionhos_6fbeb0_room_profiles @ gateway/session_hosted_service.py:CanonicalHostedRoomService.room_profiles
 *
 * A saved room keeps every local member it seated: the sorted union of the
 * profiles served here (*local_profiles*, a JSON array of strings) and the
 * profiles of members whose target kind defaults to / equals "local".
 * Returns a JSON array of strings; caller owns it. */
json_t *w18_sessionhos_6fbeb0_room_profiles(const json_t *room,
                                            const json_t *local_profiles);

/* PoP: w18_sessionhos_6fbeb0_stop_read @ gateway/session_hosted_service.py:_STOP_READS
 *
 * 1 when *operation* is one of the Stop reads an admitting home still answers
 * after the profile moved: resolve_exact, resume, info, interrupt, history,
 * results, discard. */
int w18_sessionhos_6fbeb0_stop_read(const char *operation);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SESSIONHOS_6FBEB0_H */
