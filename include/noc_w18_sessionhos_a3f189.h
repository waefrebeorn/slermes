/*
 * noc_w18_sessionhos_a3f189.h — public C API for the real C11 port of
 *   upstream Hermes gateway/session_hosted_members.py.
 *
 * PoP: w18_sessionhos_a3f189_member_roster      @ gateway/session_hosted_members.py:member_roster
 * PoP: w18_sessionhos_a3f189_served             @ gateway/session_hosted_members.py:_served
 * PoP: w18_sessionhos_a3f189_configured         @ gateway/session_hosted_members.py:_configured
 * PoP: w18_sessionhos_a3f189_config_entry       @ gateway/session_hosted_members.py:_config_entry
 * PoP: w18_sessionhos_a3f189_report             @ gateway/session_hosted_members.py:_report
 * PoP: w18_sessionhos_a3f189_refuse_unseatable  @ gateway/session_hosted_members.py:refuse_unseatable
 *
 * Every exported symbol is prefixed w18_sessionhos_a3f189_ so the port never
 * collides with existing C definitions.
 */
#ifndef NOC_W18_SESSIONHOS_A3F189_H
#define NOC_W18_SESSIONHOS_A3F189_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The roster of local profiles a hosted group on one authority may seat.
 *
 * `profile_id`  - authority.profile_id (a profile home path).
 * `bots`        - the local_bots(authority.runner) snapshot as a JSON array of
 *                 objects: {name, home, problem|null, retiring, is_self}.
 *                 `is_self` stands in for `bot.authority is authority`.
 * `siblings`    - the host-scoped caller flag (default True upstream).
 * `config`      - the loaded gateway config object (hosted_rooms section).
 *
 * On success returns 0 and hands back:
 *   *out_homes   - object: member name -> home path string (str(Path)).
 *   *out_refused - object: name -> [reason, field, detail].
 *   *out_reports - array of {"field","detail"} for each entry logged once.
 * Any out pointer may be NULL when the caller does not want it.
 */
int w18_sessionhos_a3f189_member_roster(const char *profile_id, const json_t *bots,
                                       int siblings, const json_t *config,
                                       json_t **out_homes, json_t **out_refused,
                                       json_t **out_reports);

/* (own name, name -> home, name -> [reason, field, detail]) over the local
 * bots this process serves. *out_own is malloc'd (caller frees). */
int w18_sessionhos_a3f189_served(const char *profile_id, const json_t *bots,
                                 char **out_own, json_t **out_homes,
                                 json_t **out_refused);

/* The hosted_rooms.profiles mapping ({} when the section carries no entries).
 * A non-mapping section is logged once and yields {}. */
json_t *w18_sessionhos_a3f189_configured(const char *own, const json_t *config,
                                         json_t **out_reports);

/* One config entry -> {"home","reason","detail"}. `name` is the raw JSON key
 * value (a non-string key is refused), `value` the raw entry value, `served`
 * an object name -> home, `seated` an object home-key -> name. */
json_t *w18_sessionhos_a3f189_config_entry(const json_t *name, const json_t *value,
                                           const json_t *served, const json_t *seated);

/* Log one ignored entry once per (own, field, value, detail). Returns 1 when
 * this call logged it, 0 when the key was already reported. */
int w18_sessionhos_a3f189_report(const char *own, const char *field,
                                 const json_t *value, const char *detail);

/* The named refusal for the first local member whose profile is refused:
 * {"raised","reason","field","detail"} (raised=false when none matches). */
json_t *w18_sessionhos_a3f189_refuse_unseatable(const json_t *members,
                                                const json_t *refused);

/* --- helpers the differential oracle probes directly ---------------------- */

/* str(Path(value)) on this (Windows) host: '/' -> '\', '.' parts dropped. */
char *w18_sessionhos_a3f189_path_str(const char *value);

/* hermes_constants.hermes_home_key: resolved home, normcased. */
char *w18_sessionhos_a3f189_home_key(const char *value);

/* gateway.hosted_rooms_common.IDENTIFIER_RE.fullmatch(name). */
int w18_sessionhos_a3f189_identifier_ok(const char *name);

/* gateway.session_authorities.served_profile_name(home) or 'default'. */
char *w18_sessionhos_a3f189_served_profile_name(const char *home);

/* Forget every key _report already logged (module-global set). */
void w18_sessionhos_a3f189_report_reset(void);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W18_SESSIONHOS_A3F189_H */
