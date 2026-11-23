#ifndef NOC_W9_DOCTORSTATE_855D6C_H
#define NOC_W9_DOCTORSTATE_855D6C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_doctorstate_855d6c_format_bytes @ hermes_cli/doctor_state.py:_render_state_db_stats
 * The sizefmt.format_bytes helper imported by the module. */
struct json_t *w9_doctorstate_855d6c_format_bytes(const char *value);

/* PoP: w9_doctorstate_855d6c_render @ hermes_cli/doctor_state.py:_render_state_db_stats
 * Returns a JSON array of [kind, text, detail] rows. */
struct json_t *w9_doctorstate_855d6c_render(const struct json_t *stats,
                                            const struct json_t *holders,
                                            const char *host_note);

/* PoP: w9_doctorstate_855d6c_is_malformed @ hermes_cli/doctor_state.py:_classify_unreadable_state_db */
int w9_doctorstate_855d6c_is_malformed(const char *exc);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_DOCTORSTATE_855D6C_H */
