/*
 * noc_w29_pluginsins_4e3ca4_p2.h - C surface for upstream
 * hermes_cli/plugins_install_scan_context.py (gate w29_pluginsins_4e3ca4_p2).
 *
 * Gate p2 partition: the literal/exec seam predicates - quoted test data,
 * base64 media and archive round-trips, alternation tokens inside literals,
 * advisory `sudo`, safety-negation clauses, quoted injection phrases, benign
 * lazy `__import__("os")`, `authorized_keys` mentions, loopback addresses and
 * continuations, coin names, public client keys, and benign `/proc` reads.
 *
 * Same seams as gate p1 (see noc_w29_pluginsins_4e3ca4_p1.h): posix rel_path
 * in, (pattern_id, severity, category) in, no filesystem, no host state.
 */
#ifndef NOC_W29_PLUGINSINS_4E3CA4_P2_H
#define NOC_W29_PLUGINSINS_4E3CA4_P2_H

#include <stddef.h>
#include "hermes_json.h"
#include "noc_w29_pluginsins_4e3ca4_p1.h"

#ifdef __cplusplus
extern "C" {
#endif

/* is_inert_fixture_line(finding, line, is_code, rel_path) - quoted test data
 * that nothing on the line runs. */
int noc_pis_p2_is_inert_fixture_line(const noc_finding_t *f, const char *line,
                                    int is_code, const char *rel_path);

/* is_base64_media(line) - the first long base64 run decodes to a media header. */
int noc_pis_p2_is_base64_media(const char *line);

/* is_base64_archive_roundtrip(finding, line) - tar-create + base64 encode/decode. */
int noc_pis_p2_is_base64_archive_roundtrip(const noc_finding_t *f, const char *line);

/* is_regex_alternation_token(finding, line) - token is `a|token|b` or a whole literal. */
int noc_pis_p2_is_regex_alternation_token(const noc_finding_t *f, const char *line);

/* is_sudo_inert(finding, line, rel_path) - advisory hint or safety disclaimer. */
int noc_pis_p2_is_sudo_inert(const noc_finding_t *f, const char *line, const char *rel_path);

/* is_env_dump_in_safety_clause(finding, line) - `env|`/printenv under a negation. */
int noc_pis_p2_is_env_dump_in_safety_clause(const noc_finding_t *f, const char *line);

/* is_quoted_injection_phrase(finding, line) - the phrase sits in a quoted span. */
int noc_pis_p2_is_quoted_injection_phrase(const noc_finding_t *f, const char *line);

/* is_benign_lazy_os_import(finding, line) - __import__("os").<benign member>. */
int noc_pis_p2_is_benign_lazy_os_import(const noc_finding_t *f, const char *line);

/* is_authorized_keys_prose(finding, line, is_doc) - a mention with no write action. */
int noc_pis_p2_is_authorized_keys_prose(const noc_finding_t *f, const char *line, int is_doc);

/* is_loopback_only(finding, line) - every IP:port on the line is 127.0.0.0/8. */
int noc_pis_p2_is_loopback_only(const noc_finding_t *f, const char *line);

/* is_loopback_continuation(finding, line, joined) - a `\`-continued loopback fetch. */
int noc_pis_p2_is_loopback_continuation(const noc_finding_t *f, const char *line,
                                       const char *joined);

/* is_coin_name_only(finding, line) - the standalone word `monero`, no machinery. */
int noc_pis_p2_is_coin_name_only(const noc_finding_t *f, const char *line);

/* is_public_client_key(finding, line) - every literal is a public-by-design key. */
int noc_pis_p2_is_public_client_key(const noc_finding_t *f, const char *line);

/* is_own_proc_read(finding, line) - every /proc path is a benign leaf, read only. */
int noc_pis_p2_is_own_proc_read(const noc_finding_t *f, const char *line);

#ifdef __cplusplus
}
#endif
#endif
