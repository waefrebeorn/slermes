#ifndef NOC_W13_SESSIONTUI_41EB5B_H
#define NOC_W13_SESSIONTUI_41EB5B_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_sessiontui_41eb5b_word @ gateway/session_tui_config.py:_word
 * '' for null, else str(value).strip().lower() (Python coercion). */
struct json_t *w13_sessiontui_41eb5b_word(const struct json_t *value);

/* PoP: w13_sessiontui_41eb5b_statusbar @ gateway/session_tui_config.py:_statusbar
 * False -> 'off'; a stripped/lowered string in {off,top,bottom} -> that word;
 * everything else -> 'top'. */
struct json_t *w13_sessiontui_41eb5b_statusbar(const struct json_t *raw);

/* PoP: w13_sessiontui_41eb5b_profile_leaf @ gateway/session_tui_config.py:_set_profile_leaf
 * Applies a dotted config.yaml leaf to a copy of cfg with main's validation.
 * Returns the edited cfg, or {"__raise__":"AuthorityRefusal"|"ValueError"}
 * where Python refused. */
struct json_t *w13_sessiontui_41eb5b_profile_leaf(const struct json_t *cfg,
                                                   const struct json_t *key,
                                                   const struct json_t *value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_SESSIONTUI_41EB5B_H */
