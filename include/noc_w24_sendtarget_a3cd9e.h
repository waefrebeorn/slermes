/*
 * noc_w24_sendtarget_a3cd9e.h - C surface for upstream
 * gateway/platforms/send_targets.py (gate w24_sendtarget_a3cd9e).
 *
 * What a platform plugin declares to the send_message contract: the target_ref
 * parsers (pure ref -> (chat_id, thread_id) | None | NOT_EXPLICIT) and the
 * StandaloneSendPolicy field defaults. Everything is a pure function of the
 * ref, the match, and the declared fields; nothing here sends.
 *
 * The regex ENGINE is the seam: regex_groups_parser / regex_stripped_parser are
 * closures over a compiled pattern, and their bodies only decide the tuple shape
 * from an already-computed re.fullmatch result. So the match arrives as values
 * (matched plus the two groups) and what is ported is exactly what the closures
 * do with it -- Python's re semantics stay in Python.
 */
#ifndef W24_SENDTARGET_A3CD9E_H
#define W24_SENDTARGET_A3CD9E_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* StandaloneSendPolicy field count, in declaration order. */
#define W24_SENDTARGET_A3CD9E_POLICY_FIELDS 9

/* req: {ref} -> [stripped_ref, null] when the stripped ref is non-empty, else
 * null (fall through to core's generic id rules). */
json_t *w24_sendtarget_a3cd9e_parse_nonempty(const json_t *req);

/* req: {matched, ref} -> [ref.strip(), null] when the pattern fully matched,
 * else null (regex_stripped_parser). */
json_t *w24_sendtarget_a3cd9e_regex_stripped(const json_t *req);

/* req: {matched, group1, group2, thread_group} ->
 * [group1, thread_group ? group2 : null] on a full match, else null
 * (regex_groups_parser). */
json_t *w24_sendtarget_a3cd9e_regex_groups(const json_t *req);

/* req: {<policy field>: value, ...} -> the StandaloneSendPolicy a plugin
 * declares: every field, declaration default unless the request overrides it. */
json_t *w24_sendtarget_a3cd9e_policy(const json_t *req);

/* NOT_EXPLICIT's type plus every policy field default. */
json_t *w24_sendtarget_a3cd9e_constants(void);

#ifdef __cplusplus
}
#endif
#endif
