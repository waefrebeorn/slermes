#ifndef NOC_W12_TURNTHREAD_3F2B0B_H
#define NOC_W12_TURNTHREAD_3F2B0B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_turnthread_3f2b0b_reset @ gateway/turn_thread_reset.py:reset_reused_thread_state
 * Models the calling thread's turn-keyed state as a JSON "world":
 *   {"tid":"<decimal>","interrupt":{...}|absent,"sudo_cache":{...}|absent,
 *    "locals":{"<module>.<attr>":{...},...}}
 * Returns {"interrupt":<state>|null,"sudo_cache":<map>|null,"locals":<map>}
 * after: set_interrupt(False,tid) on tools.interrupt (clears bit/reason/yield),
 * popping "thread:<tid>" from the sudo cache, and clearing the __dict__ of
 * every configured per-turn threading.local that is present in the world. */
struct json_t *w12_turnthread_3f2b0b_reset(const struct json_t *world);

/* PoP: w12_turnthread_3f2b0b_reset @ gateway/turn_thread_reset.py:_PER_TURN_THREAD_LOCALS
 * The configured ("module", "attr") pairs projected as "module.attr" strings. */
struct json_t *w12_turnthread_3f2b0b_per_turn_locals(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W12_TURNTHREAD_3F2B0B_H */
