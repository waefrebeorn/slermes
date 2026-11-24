/*
 * noc_w28_adapter_ca7894_p2.h - C surface for the From: identity and
 * Authentication-Results grammar of upstream
 * plugins/platforms/email/adapter.py (gate w28_adapter_ca7894_p2).
 *
 * Ported: parseaddr's strict single-mailbox path plus the adapter's own
 * display-<addr> fallback and reply-To: round-trip gate, the domain helpers and
 * the RFC 8601 clause / property scanners.  The IMAP fetch that supplies the
 * header bytes is a seam.
 */
#ifndef NOC_W28_ADAPTER_CA7894_P2_H
#define NOC_W28_ADAPTER_CA7894_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Bounds the upstream applies before it hands a From: value to parseaddr. */
#define W28_ADAPTER_CA7894_MAX_FROM_LEN 2048
#define W28_ADAPTER_CA7894_MAX_FROM_PARENS 64

/* in: {raw} -> bare ASCII-lowercased addr-spec, or "" when the value is not
 * exactly one mailbox (the caller drops the message on ""). */
json_t *w28_adapter_ca7894_extract_email_address(const json_t *in);

/* in: {address} -> lowercased domain part ("" when the address has none). */
json_t *w28_adapter_ca7894_domain_of(const json_t *in);

/* in: {a, b} -> relaxed DMARC alignment of two domains. */
json_t *w28_adapter_ca7894_domains_aligned(const json_t *in);

/* in: {text} -> ["authserv-id", " method=result props", ...] or null when a
 * quote or comment is unbalanced. */
json_t *w28_adapter_ca7894_ar_clauses(const json_t *in);

/* in: {text} -> [["header.from", "example.com"], ...] property pairs. */
json_t *w28_adapter_ca7894_auth_props(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
