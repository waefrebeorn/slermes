/*
 * noc_w28_adapter_ca7894_p3.h - C surface for the authentication verdict and
 * the dispatch/reply derivations of upstream
 * plugins/platforms/email/adapter.py (gate w28_adapter_ca7894_p3).
 *
 * Ported: the DMARC/SPF/DKIM verdict over the topmost Authentication-Results
 * header, the pre-dispatch sender gate, the seen-UID trim, the Message-ID
 * domain, the reply subject/threading derivation and the attachment-metadata
 * decision.  The IMAP/SMTP sockets, the env reads (_get_secret), the gateway
 * allowlist verdict (_is_sender_authorized), the attachment cache writers and
 * the thread-context dict are fixtures, not ports.
 */
#ifndef NOC_W28_ADAPTER_CA7894_P3_H
#define NOC_W28_ADAPTER_CA7894_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {from_addr, authserv_id, headers: [Authentication-Results values...]}
 * -> [authenticated(bool), reason].  Only headers[0] is authoritative. */
json_t *w28_adapter_ca7894_verify_sender_auth(const json_t *in);

/* in: {sender, address, require_auth, msg: {sender_authenticated, auth_reason},
 *      env: {EMAIL_ALLOWED_USERS, GATEWAY_ALLOWED_USERS,
 *            EMAIL_ALLOW_ALL_USERS, GATEWAY_ALLOW_ALL_USERS},
 *      extra: {unauthorized_dm_behavior}, authorized: true|false|null}
 * -> {accepted: bool, log: "" | "debug" | "warning"}. */
json_t *w28_adapter_ca7894_sender_accepted(const json_t *in);

/* in: {uids: ["1", ...], max} -> the surviving UIDs, numerically sorted. */
json_t *w28_adapter_ca7894_trim_seen_uids(const json_t *in);

/* in: {address} -> the Message-ID domain ("localhost" when there is none). */
json_t *w28_adapter_ca7894_message_id_domain(const json_t *in);

/* in: {address, to_addr, ctx: {subject, message_id}|null, reply_to} ->
 * {subject, in_reply_to, references}. */
json_t *w28_adapter_ca7894_reply_threading(const json_t *in);

/* in: {disposition, content_type, subtype, filename|null, has_payload, skip} ->
 * {filename, type: "image"|"document", media_type} or null when the part is
 * not an attachment the adapter caches. */
json_t *w28_adapter_ca7894_attachment_meta(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
