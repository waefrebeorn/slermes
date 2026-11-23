#ifndef NOC_W10_SESSIONMOA_08E3F3_H
#define NOC_W10_SESSIONMOA_08E3F3_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessionmoa_08e3f3_moa_override @ gateway/session_moa_turn.py:moa_override
 * Returns the virtual MoA provider override object for the given preset. */
struct json_t *w10_sessionmoa_08e3f3_moa_override(const struct json_t *preset);
/* PoP: w10_sessionmoa_08e3f3_stamp_moa_turn @ gateway/session_moa_turn.py:stamp_moa_turn
 * preset = event._moa_preset; envelope = base envelope object. Returns a copy of
 * envelope with moa_turn_v1 added when preset is truthy. */
struct json_t *w10_sessionmoa_08e3f3_stamp_moa_turn(const struct json_t *preset,
                                                    const struct json_t *envelope);
/* PoP: w10_sessionmoa_08e3f3_restore_moa_turn @ gateway/session_moa_turn.py:restore_moa_turn
 * Returns envelope["moa_turn_v1"]["preset"] (copy) or JSON null when absent. */
struct json_t *w10_sessionmoa_08e3f3_restore_moa_turn(const struct json_t *envelope);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONMOA_08E3F3_H */
