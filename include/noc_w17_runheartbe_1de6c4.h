#ifndef NOC_W17_RUNHEARTBE_1DE6C4_H
#define NOC_W17_RUNHEARTBE_1DE6C4_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_runheartbe_1de6c4_watched_homes @ gateway/run_heartbeat_restore.py:_watched_homes
 * Args: default_home (JSON string), multiplex_homes (JSON array of [name, home]
 * pairs), multiplex_profiles (int). Returns a new JSON array of the deduplicated
 * homes as Windows pathlib strings, in first-seen order. */
struct json_t *w17_runheartbe_1de6c4_watched_homes(const struct json_t *default_home,
                                        const struct json_t *multiplex_homes,
                                        int multiplex_profiles);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_RUNHEARTBE_1DE6C4_H */
