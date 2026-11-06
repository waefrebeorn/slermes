/*
 * port_media_labels.h — pure formatting/derivation helpers, one unit:
 *   gateway/relay/media.py  media_base_url, is_relay_media_url
 *   hermes_cli/platforms.py _plugin_label
 *   hermes_cli/browser_credential.py generation_of
 */
#ifndef PORT_MEDIA_LABELS_H
#define PORT_MEDIA_LABELS_H

/* media.py media_base_url: map a ws(s)://…connector relay dial URL to the
 * http(s)://… base (strip, trailing-/ strip, ws/wss prefix swap, drop a
 * trailing "/relay" suffix). Returns malloc'd, caller frees. */
char *ml_media_base_url(const char *relay_dial_url);

/* media.py RelayMediaClient.is_relay_media_url: True when the URL is a
 * connector re-host reference ("/relay/media/" appears in it). */
int ml_is_relay_media_url(const char *url);

/* media.py _content_disposition_filename: the filename a Content-Disposition
 * header names, cleaned for display; "" when none. RFC 5987 filename*
 * (UTF-8''r%C3%A9sum%C3%A9.pdf) wins over plain filename; quoted values and
 * escapes handled like the stdlib MIME parser. Returns malloc'd, caller frees. */
char *ml_content_disposition_filename(const char *header);

/* platforms.py _plugin_label: "<emoji>  <label>" when emoji is set, else
 * <label>. emoji may be NULL/empty. Returns malloc'd, caller frees. */
char *ml_plugin_label(const char *emoji, const char *label);

/* browser_credential.py generation_of: SHA-256 hexdigest of the credential
 * (lowercase 64 hex chars). Returns malloc'd, caller frees. */
char *ml_generation_of(const char *credential);

#endif /* PORT_MEDIA_LABELS_H */
