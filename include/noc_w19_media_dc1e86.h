#ifndef NOC_W19_MEDIA_DC1E86_H
#define NOC_W19_MEDIA_DC1E86_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_media_dc1e86_media_base_url @ gateway/relay/media.py:media_base_url
 * ``ws(s)://.../relay`` dial URL -> ``http(s)://...`` connector base.  Returns a new
 * NUL-terminated string (caller frees). */
char *w19_media_dc1e86_media_base_url(const char *relay_dial_url);

/* PoP: w19_media_dc1e86_is_relay_media_url @ gateway/relay/media.py:RelayMediaClient.is_relay_media_url
 * True only for a URL on the connector's own origin (scheme, host, effective port)
 * under ``{base path}/relay/media/``, with no userinfo/backslash netloc and no ``..``
 * segment after percent-decoding.  ``base_url`` is the client's raw base URL (the
 * trailing slashes are stripped here, as the constructor does). */
int w19_media_dc1e86_is_relay_media_url(const char *base_url, const char *url);

/* PoP: w19_media_dc1e86_enabled @ gateway/relay/media.py:RelayMediaClient.enabled
 * ``bool(base_url.rstrip("/") and gateway_id and secret)``; NULL counts as "". */
int w19_media_dc1e86_enabled(const char *base_url, const char *gateway_id, const char *secret);

/* PoP: w19_media_dc1e86_clean_attachment_filename @ gateway/platforms/event.py:clean_attachment_filename
 * Display-safe basename: last path component, Cc/Cf code points dropped, stripped,
 * ``""`` for an unusable name, capped at 255 characters with the extension kept.
 * Returns a new string (caller frees; never NULL - "" for an unusable name). */
char *w19_media_dc1e86_clean_attachment_filename(const char *name);

/* PoP: w19_media_dc1e86_content_disposition_filename @ gateway/relay/media.py:_content_disposition_filename
 * The filename a Content-Disposition header names, cleaned for display; an RFC 5987
 * ``filename*`` wins over the plain ``filename``; ``""`` when neither names one.
 * Single-byte charsets (iso-8859-1/latin-1) and UTF-8/ASCII are transcoded; other
 * charsets are treated as UTF-8.  Returns a new string (caller frees). */
char *w19_media_dc1e86_content_disposition_filename(const char *header);

/* PoP: w19_media_dc1e86_warn_foreign_rehost @ gateway/relay/media.py:RelayMediaClient._warn_foreign_rehost
 * The "warn once per foreign origin" decision: 1 when the call would log (a
 * ``/relay/media/`` path off the relay origin whose origin is not in ``seen``),
 * 0 otherwise.  ``seen`` is an array of ``[scheme, host, port]`` triples. */
int w19_media_dc1e86_warn_foreign_rehost(const char *base_url, const char *url, const struct json_t *seen);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_MEDIA_DC1E86_H */
