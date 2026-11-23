#ifndef NOC_W14_CHANGEWATC_E37815_H
#define NOC_W14_CHANGEWATC_E37815_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes tui_gateway/change_watcher.py (gate w14_changewatc_e37815).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w14_changewatc_e37815_skin_payload @ tui_gateway/change_watcher.py:skin_payload
 * `skin` is the injected active-skin object (hermes_cli.skin_engine is stubbed
 * in the oracle); a missing/non-object skin models the live except -> {} path. */
struct json_t *w14_changewatc_e37815_skin_payload(const struct json_t *config,
                                                  const struct json_t *skin);

/* PoP: w14_changewatc_e37815_groups_changed_payload @ tui_gateway/change_watcher.py:_groups_changed_payload
 * `epoch` replaces the live replay_epoch() call; the process-wide
 * itertools.count(1) counter is mirrored by a static counter in the C file. */
struct json_t *w14_changewatc_e37815_groups_changed_payload(const struct json_t *before,
                                                            const struct json_t *after,
                                                            const char *epoch);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_CHANGEWATC_E37815_H */
