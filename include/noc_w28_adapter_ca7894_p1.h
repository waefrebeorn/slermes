/*
 * noc_w28_adapter_ca7894_p1.h - C surface for the header / body text leaves
 * of upstream plugins/platforms/email/adapter.py (gate w28_adapter_ca7894_p1).
 *
 * Ported: the security-mode normaliser, the automated-sender predicate, the
 * charset-safe byte decoder, the RFC 2047 header decoder, the HTML tag
 * stripper and the (possibly nested) comment stripper.  Sockets, IMAP/SMTP and
 * every env/file read are seams that stay in the caller.
 */
#ifndef NOC_W28_ADAPTER_CA7894_P1_H
#define NOC_W28_ADAPTER_CA7894_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* in: {value, default} -> "tls" | "starttls" | "plain". */
json_t *w28_adapter_ca7894_normalize_security(const json_t *in);

/* in: {address, headers} -> bool (noreply pattern or bulk-mail header). */
json_t *w28_adapter_ca7894_is_automated_sender(const json_t *in);

/* in: {payload_b64, charset} -> decoded text (never raises). */
json_t *w28_adapter_ca7894_safe_decode(const json_t *in);

/* in: {raw} -> RFC 2047 decoded header value. */
json_t *w28_adapter_ca7894_decode_header_value(const json_t *in);

/* in: {html} -> tag-stripped, entity-decoded, whitespace-trimmed text. */
json_t *w28_adapter_ca7894_strip_html(const json_t *in);

/* in: {text} -> text with every (nested) (comment) replaced by one space. */
json_t *w28_adapter_ca7894_strip_comments(const json_t *in);

#ifdef __cplusplus
}
#endif
#endif
